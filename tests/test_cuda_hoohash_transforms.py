#!/usr/bin/env python3
"""Measure pinned-Core/OpenLibm versus CUDA FP64 HooHash transforms."""

import argparse
import hashlib
import json
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from cuda_device_policy import should_skip_device_execution
from test_cuda_core_matrix_oracle import (
    CASE_BYTES as MATRIX_CASE_BYTES,
    HEADER_NONCE6,
    INPUT_MAGIC as MATRIX_INPUT_MAGIC,
    OUTPUT_MAGIC as MATRIX_OUTPUT_MAGIC,
    UINT32_MAX,
    core_matrix_seed,
    header_with_nonce,
    make_seeds,
    run_oracle as run_matrix_oracle,
)
from test_cuda_blake3_stage import run_cpu_oracle


INPUT_MAGIC = b"PPWTRIN1"
OUTPUT_MAGIC = b"PPWTRO1\0"
INPUT_RECORD = struct.Struct("<QB")
OUTPUT_RECORD = struct.Struct("<QQQIQQQQQQBQQQQQQQ")
MAXIMUM_INPUTS = 32768
MAXIMUM_REPORTED_MISMATCHES = 32768
MASK64 = (1 << 64) - 1
CORE_SHA = "bc08108ceccd957c79040dc7fedc1d56d8d47f97"
OPENLIBM_SHA = "9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d"
STAGE_NAME = "hoohash_v110_fp64_transform_diagnostic"
ERROR_EXIT_CODES = {
    "no_device": 2,
    "driver_unavailable": 3,
    "initialization_failed": 4,
    "cuda_failure": 5,
    "output_failure": 6,
    "invalid_arguments": 64,
}
FUNCTIONS = {
    "factor_one": 1,
    "factor_two": 2,
    "medium_complex_nonlinear": 4,
    "intermediate_complex_nonlinear": 5,
    "high_complex_nonlinear": 6,
    "complex_nonlinear": 7,
    "safe_complex_transform": 8,
    "transform_factor": 9,
    "medium_sine": 11,
    "medium_cosine": 12,
    "medium_sine_plus_cosine": 13,
    "intermediate_sine": 14,
    "high_abs_plus_one": 15,
    "high_sqrt": 16,
    "complex_argument": 17,
}
PI = 3.14159265358979323846
EPSILON = 1e-9
COMPLEX_TRANSFORM_MULTIPLIER = 0.000001


class TransformDiagnosticError(AssertionError):
    pass


def require(condition, message):
    if not condition:
        raise TransformDiagnosticError(message)


def run_checked(command, *, timeout=60):
    completed = subprocess.run(
        command, capture_output=True, text=True, timeout=timeout, check=False
    )
    require(completed.returncode == 0,
            f"command failed ({completed.returncode}): {' '.join(map(str, command))}\n"
            f"stdout: {completed.stdout.strip()}\nstderr: {completed.stderr.strip()}")
    return completed


def deterministic_template(index):
    header = bytearray(80)
    for byte_index in range(76):
        header[byte_index] = (
            byte_index * 131 + index * 17 + 0x5B
        ) & 0xFF
    header[0] ^= index
    return bytes(header)


def run_transform_oracle(oracle, input_path, output_path):
    completed = run_checked(
        [oracle, "--input", str(input_path), "--output", str(output_path)],
        timeout=90,
    )
    return completed.stdout.strip()


def matrix_values_from_transcript(transcript, seed_count):
    require(transcript[:8] == MATRIX_OUTPUT_MAGIC,
            "pinned Core matrix transcript magic changed")
    require(struct.unpack_from("<I", transcript, 8)[0] == seed_count,
            "pinned Core matrix transcript seed count changed")
    require(len(transcript) == 12 + seed_count * MATRIX_CASE_BYTES,
            "pinned Core matrix transcript has an unexpected size")
    values_by_seed = []
    for seed_index in range(seed_count):
        case_start = 12 + seed_index * MATRIX_CASE_BYTES
        matrix_start = case_start + 32 + 32
        values = []
        for element in range(4096):
            observation = matrix_start + element * 52
            matrix_bits = struct.unpack_from("<Q", transcript, observation + 44)[0]
            values.append(struct.unpack("<d", struct.pack("<Q", matrix_bits))[0])
        values_by_seed.append(values)
    return values_by_seed


def matrix_corpus(matrix_oracle, blake3_oracle, root):
    seeds = make_seeds(blake3_oracle)
    seed_input = root / "pinned-core-matrix-input.bin"
    seed_output = root / "pinned-core-matrix-output.bin"
    seed_input.write_bytes(
        MATRIX_INPUT_MAGIC + struct.pack("<I", len(seeds)) + b"".join(seeds)
    )
    summary = run_matrix_oracle(matrix_oracle, seed_input, seed_output)
    transcript = seed_output.read_bytes()
    values = matrix_values_from_transcript(transcript, len(seeds))
    require(seeds[0] == core_matrix_seed(
        blake3_oracle, header_with_nonce(HEADER_NONCE6, 6)
    ), "nonce-6 Explorer matrix seed changed")
    return seeds, values, hashlib.sha256(transcript).hexdigest(), summary


def append_input(records, origins, value, origin, safe_enabled=True):
    if safe_enabled and not math.isfinite(value):
        raise TransformDiagnosticError(
            f"non-finite value was marked safe-enabled: {origin}"
        )
    bits = struct.unpack("<Q", struct.pack("<d", value))[0]
    records.append((bits, int(safe_enabled)))
    origins.append(origin)


def add_boundary_inputs(records, origins):
    for name, denominator, boundary in (
        ("factor-one-0.33", 8_000_000.0, 0.33),
        ("factor-one-0.66", 8_000_000.0, 0.66),
        ("factor-two-0.25", 4_000_000.0, 0.25),
        ("factor-two-0.50", 4_000_000.0, 0.50),
        ("factor-two-0.75", 4_000_000.0, 0.75),
    ):
        center = (9.0 + boundary) * denominator
        for label, candidate in (
            ("below", math.nextafter(center, -math.inf)),
            ("center", center),
            ("above", math.nextafter(center, math.inf)),
        ):
            append_input(records, origins, candidate,
                         f"branch-boundary:{name}:{label}")

    for center_name, center in (
        ("pi/2-minus-epsilon", PI / 2.0 - EPSILON),
        ("pi/2-plus-epsilon", PI / 2.0 + EPSILON),
        ("3pi/2-minus-epsilon", 3.0 * PI / 2.0 - EPSILON),
        ("3pi/2-plus-epsilon", 3.0 * PI / 2.0 + EPSILON),
    ):
        for label, candidate in (
            ("below", math.nextafter(center, -math.inf)),
            ("center", center),
            ("above", math.nextafter(center, math.inf)),
        ):
            append_input(records, origins, candidate,
                         f"intermediate-singularity:{center_name}:{label}")


def make_input_corpus(matrix_oracle, blake3_oracle, root):
    seeds, values_by_seed, matrix_transcript_sha256, matrix_summary = (
        matrix_corpus(matrix_oracle, blake3_oracle, root)
    )
    records = []
    origins = []
    real_header_count = 0

    header_cases = [
        ("explorer-nonce-0", header_with_nonce(HEADER_NONCE6, 0), 0),
        ("explorer-nonce-1", header_with_nonce(HEADER_NONCE6, 1), 0),
        ("explorer-nonce-6", header_with_nonce(HEADER_NONCE6, 6), 0),
        ("explorer-nonce-255", header_with_nonce(HEADER_NONCE6, 255), 0),
        ("explorer-nonce-256", header_with_nonce(HEADER_NONCE6, 256), 0),
        ("explorer-nonce-uint32-max",
         header_with_nonce(HEADER_NONCE6, UINT32_MAX), 0),
        ("template-0-nonce-6", header_with_nonce(deterministic_template(0), 6), 1),
    ]
    for name, header, seed_index in header_cases:
        require(core_matrix_seed(blake3_oracle, header) == seeds[seed_index],
                f"{name} did not resolve to its pinned Core matrix seed")
        first_pass = bytes.fromhex(run_cpu_oracle(
            blake3_oracle, header, "header"
        ))
        words = [int.from_bytes(first_pass[i:i + 4], "big")
                 for i in range(0, 32, 4)]
        hash_xor = 0
        for word in words:
            hash_xor ^= word
        nibbles = []
        for byte in first_pass:
            nibbles.extend((byte >> 4, byte & 0x0F))
        nonce_mod = int.from_bytes(header[76:80], "little") & 0xFF

        for element, matrix_value in enumerate(values_by_seed[seed_index]):
            column = element % 64
            product = matrix_value * float(hash_xor)
            product = product * float(nibbles[column])
            value = product + float(nonce_mod)
            append_input(
                records, origins, value,
                f"real-matrix:{name}:row-{element // 64}:column-{column}",
            )
        real_header_count += 1

    add_boundary_inputs(records, origins)
    random_generator = random.Random(0x50455045504F57)
    for index in range(512):
        value = random_generator.uniform(-25_000_000.0, 25_000_000.0)
        append_input(records, origins, value, f"deterministic-sweep:{index}")

    for name, value in (
        ("positive-zero", 0.0),
        ("negative-zero", -0.0),
        ("positive-min-subnormal", math.ulp(0.0)),
        ("negative-min-subnormal", -math.ulp(0.0)),
        ("positive-min-normal", sys.float_info.min),
        ("negative-min-normal", -sys.float_info.min),
        ("positive-max-finite", sys.float_info.max),
        ("negative-max-finite", -sys.float_info.max),
        ("positive-infinity", math.inf),
        ("negative-infinity", -math.inf),
        ("canonical-qnan", float("nan")),
    ):
        append_input(records, origins, value, f"special:{name}",
                     safe_enabled=math.isfinite(value))
    for bits, name in (
        (0x7FF8000000000042, "payload-qnan"),
        (0xFFF8000000000042, "negative-payload-qnan"),
    ):
        value = struct.unpack("<d", struct.pack("<Q", bits))[0]
        append_input(records, origins, value, f"special:{name}", safe_enabled=False)

    require(0 < len(records) <= MAXIMUM_INPUTS,
            f"transform input count {len(records)} exceeds the bounded stage cap")
    return {
        "records": records,
        "origins": origins,
        "real_header_count": real_header_count,
        "matrix_transcript_sha256": matrix_transcript_sha256,
        "matrix_summary": matrix_summary,
        "seed_count": len(seeds),
    }


def encode_input_corpus(records):
    payload = bytearray(INPUT_MAGIC + struct.pack("<I", len(records)))
    for bits, safe_enabled in records:
        payload.extend(INPUT_RECORD.pack(bits, safe_enabled))
    return bytes(payload)


def load_transcript(path, expected_count, expected_magic):
    data = Path(path).read_bytes()
    require(data[:8] == expected_magic, "transform transcript magic changed")
    require(struct.unpack_from("<I", data, 8)[0] == expected_count,
            "transform transcript count changed")
    expected_length = 12 + expected_count * OUTPUT_RECORD.size
    require(len(data) == expected_length,
            f"transform transcript has {len(data)} bytes; expected {expected_length}")
    return data


def validate_device_error(completed):
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise TransformDiagnosticError(
            f"CUDA stage error response is not JSON: {error}"
        ) from error
    require(isinstance(payload, dict) and set(payload) == {
        "schema_version", "status", "stage", "error",
    }, "CUDA transform error JSON fields changed")
    require(payload["schema_version"] == 1 and payload["status"] == "error" and
            payload["stage"] == STAGE_NAME,
            "CUDA transform error JSON schema changed")
    error = payload["error"]
    require(isinstance(error, dict) and set(error) == {
        "code", "cuda_error", "exit_code",
    }, "CUDA transform error fields changed")
    require(error["code"] in ERROR_EXIT_CODES,
            f"unexpected CUDA transform error code: {error!r}")
    require(error["exit_code"] == ERROR_EXIT_CODES[error["code"]] and
            completed.returncode == error["exit_code"],
            "CUDA transform JSON and process exit codes disagree")
    if error["code"] in {"output_failure", "invalid_arguments"}:
        require(error["cuda_error"] is None,
                "host/argument failures must not be labeled as CUDA failures")
    else:
        require(isinstance(error["cuda_error"], str),
                "CUDA runtime failure must preserve the Runtime API diagnostic")
    return error


def classify_bits(bits):
    value = struct.unpack("<d", struct.pack("<Q", bits))[0]
    if math.isnan(value):
        return "nan"
    if math.isinf(value):
        return "negative_infinity" if bits >> 63 else "positive_infinity"
    return "finite"


def ordered_integer(bits):
    if bits >> 63:
        return (~bits) & MASK64
    return bits | (1 << 63)


def finite_error(core_bits, cuda_bits):
    core = struct.unpack("<d", struct.pack("<Q", core_bits))[0]
    cuda = struct.unpack("<d", struct.pack("<Q", cuda_bits))[0]
    if not math.isfinite(core) or not math.isfinite(cuda):
        return None, None, None
    absolute = abs(core - cuda)
    relative = absolute / abs(core) if core != 0.0 else (
        0.0 if absolute == 0.0 else None
    )
    ulp = abs(ordered_integer(core_bits) - ordered_integer(cuda_bits))
    return absolute, relative, ulp


def compare_transcripts(core_data, cuda_data, origins):
    count = len(origins)
    core_records = [
        OUTPUT_RECORD.unpack_from(core_data, 12 + index * OUTPUT_RECORD.size)
        for index in range(count)
    ]
    cuda_records = [
        OUTPUT_RECORD.unpack_from(cuda_data, 12 + index * OUTPUT_RECORD.size)
        for index in range(count)
    ]
    branch_mismatches = []
    for index, (core, cuda) in enumerate(zip(core_records, cuda_records)):
        require(core[0] == cuda[0] and core[10] == cuda[10],
                f"input identity/flags differ at input {index}")
        if core[3] != cuda[3]:
            branch_mismatches.append({
                "index": index,
                "origin": origins[index],
                "input_bits": f"{core[0]:016x}",
                "core_branch": core[3],
                "cuda_branch": cuda[3],
                "core_factor_one_bits": f"{core[1]:016x}",
                "cuda_factor_one_bits": f"{cuda[1]:016x}",
                "core_factor_two_bits": f"{core[2]:016x}",
                "cuda_factor_two_bits": f"{cuda[2]:016x}",
            })

    summaries = {}
    for function, field_index in FUNCTIONS.items():
        exact = 0
        mismatches = []
        max_ulp = 0
        max_abs = 0.0
        max_rel = 0.0
        max_ulp_case = None
        max_abs_case = None
        max_rel_case = None
        category_mismatches = 0
        nan_pairs = 0
        infinity_pairs = 0
        finite_pairs = 0
        for index, (core, cuda) in enumerate(zip(core_records, cuda_records)):
            core_bits = core[field_index]
            cuda_bits = cuda[field_index]
            core_class = classify_bits(core_bits)
            cuda_class = classify_bits(cuda_bits)
            if core_class == "nan" and cuda_class == "nan":
                nan_pairs += 1
            if core_class.endswith("infinity") and cuda_class.endswith("infinity"):
                infinity_pairs += 1
            if core_class != cuda_class:
                category_mismatches += 1
            if core_bits == cuda_bits:
                exact += 1
                continue
            absolute, relative, ulp = finite_error(core_bits, cuda_bits)
            if absolute is not None:
                finite_pairs += 1
                if absolute > max_abs:
                    max_abs = absolute
                    max_abs_case = index
            if relative is not None and relative > max_rel:
                max_rel = relative
                max_rel_case = index
            if ulp is not None and ulp > max_ulp:
                max_ulp = ulp
                max_ulp_case = index
            if len(mismatches) < MAXIMUM_REPORTED_MISMATCHES:
                mismatches.append({
                    "index": index,
                    "origin": origins[index],
                    "input_bits": f"{core[0]:016x}",
                    "branch_core": core[3],
                    "branch_cuda": cuda[3],
                    "core_bits": f"{core_bits:016x}",
                    "cuda_bits": f"{cuda_bits:016x}",
                    "core_class": core_class,
                    "cuda_class": cuda_class,
                    "absolute_error": absolute,
                    "relative_error": relative,
                    "ulp_difference": ulp,
                })
        summaries[function] = {
            "cases": count,
            "bitwise_equal": exact,
            "bitwise_mismatches": count - exact,
            "finite_mismatch_pairs": finite_pairs,
            "nan_pairs": nan_pairs,
            "infinity_pairs": infinity_pairs,
            "classification_mismatches": category_mismatches,
            "max_absolute_error": max_abs if max_abs_case is not None else 0.0,
            "max_absolute_error_case": max_abs_case,
            "max_relative_error": max_rel if max_rel_case is not None else 0.0,
            "max_relative_error_case": max_rel_case,
            "max_ulp_difference": max_ulp,
            "max_ulp_case": max_ulp_case,
            "mismatches": mismatches,
        }

    core_branches = {}
    for record in core_records:
        core_branches[str(record[3])] = core_branches.get(str(record[3]), 0) + 1
    cuda_branches = {}
    for record in cuda_records:
        cuda_branches[str(record[3])] = cuda_branches.get(str(record[3]), 0) + 1
    parity_passed = not branch_mismatches and all(
        summary["bitwise_mismatches"] == 0 for summary in summaries.values()
    )
    return {
        "branch_mismatches": branch_mismatches,
        "branch_mismatch_count": len(branch_mismatches),
        "core_branch_counts": core_branches,
        "cuda_branch_counts": cuda_branches,
        "functions": summaries,
        "bitwise_parity_passed": parity_passed,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--core-oracle", required=True)
    parser.add_argument("--matrix-oracle", required=True)
    parser.add_argument("--blake3-oracle", required=True)
    parser.add_argument("--device-stage", required=True)
    parser.add_argument("--architecture", required=True)
    parser.add_argument("--oracle-compiler", required=True)
    parser.add_argument("--openlibm-library", required=True)
    parser.add_argument("--report", required=True)
    parser.add_argument("--evidence-dir", required=True)
    args = parser.parse_args()

    evidence_dir = Path(args.evidence_dir)
    evidence_dir.mkdir(parents=True, exist_ok=True)
    report_path = Path(args.report)
    report_path.parent.mkdir(parents=True, exist_ok=True)

    invalid = subprocess.run(
        [args.device_stage], capture_output=True, text=True, timeout=5, check=False
    )
    invalid_error = validate_device_error(invalid)
    require(invalid_error["code"] == "invalid_arguments" and
            invalid.returncode == 64,
            "invalid invocation must fail before CUDA initialization")
    with tempfile.TemporaryDirectory(prefix="pepepow-transform-invalid-") as temp:
        invalid_input = Path(temp) / "nonfinite-safe-input.bin"
        invalid_input.write_bytes(
            INPUT_MAGIC + struct.pack("<I", 1) +
            INPUT_RECORD.pack(0x7FF8000000000000, 1)
        )
        nonfinite_safe = subprocess.run(
            [args.device_stage, "--input", str(invalid_input),
             "--output", str(Path(temp) / "unused.bin")],
            capture_output=True, text=True, timeout=5, check=False,
        )
        nonfinite_error = validate_device_error(nonfinite_safe)
        require(nonfinite_error["code"] == "invalid_arguments" and
                nonfinite_safe.returncode == 64,
                "non-finite SafeComplexTransform input must be rejected before CUDA")

    with tempfile.TemporaryDirectory(prefix="pepepow-transform-input-") as temporary:
        corpus_metadata = make_input_corpus(
            args.matrix_oracle, args.blake3_oracle, Path(temporary)
        )
    records = corpus_metadata["records"]
    origins = corpus_metadata["origins"]
    input_data = encode_input_corpus(records)
    input_path = evidence_dir / "transform-inputs.bin"
    core_path = evidence_dir / "pinned-core-openlibm-transcript.bin"
    cuda_path = evidence_dir / "cuda-transform-transcript.bin"
    catalog_path = evidence_dir / "transform-input-catalog.jsonl"
    input_path.write_bytes(input_data)

    with catalog_path.open("w", encoding="utf-8", newline="\n") as catalog:
        for index, ((bits, safe_enabled), origin) in enumerate(zip(records, origins)):
            catalog.write(json.dumps({
                "index": index,
                "origin": origin,
                "input_bits": f"{bits:016x}",
                "safe_enabled": bool(safe_enabled),
            }, sort_keys=True, allow_nan=False) + "\n")

    core_summary = run_transform_oracle(
        args.core_oracle, input_path, core_path
    )
    core_data = load_transcript(core_path, len(records), OUTPUT_MAGIC)

    device = subprocess.run(
        [args.device_stage, "--input", str(input_path),
         "--output", str(cuda_path)],
        capture_output=True, text=True, timeout=45, check=False,
    )
    gpu_status = "live"
    device_error = None
    comparison = None
    if device.returncode != 0:
        device_error = validate_device_error(device)
        allow_skip = os.environ.get("PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP") == "1"
        require_device = os.environ.get("PEPEPOW_CUDA_REQUIRE_DEVICE") == "1"
        require(should_skip_device_execution(
            device_error["code"], allow_no_device_skip=allow_skip,
            require_device=require_device,
        ), f"CUDA transform stage failed with {device_error['code']} "
           f"({device_error['cuda_error']}); only explicit no-GPU jobs may skip")
        gpu_status = "skipped_no_device"
    else:
        try:
            payload = json.loads(device.stdout)
        except json.JSONDecodeError as error:
            raise TransformDiagnosticError(
                f"CUDA transform success output is not JSON: {error}"
            ) from error
        require(isinstance(payload, dict) and set(payload) == {
            "schema_version", "status", "stage", "input_count",
            "device", "cuda_versions", "error",
        }, "CUDA transform success JSON fields changed")
        require(payload["schema_version"] == 1 and payload["status"] == "ok" and
                payload["stage"] == STAGE_NAME and
                payload["input_count"] == len(records) and payload["error"] is None,
                "CUDA transform success JSON values changed")
        require(isinstance(payload["device"], dict) and set(payload["device"]) == {
            "index", "name", "compute_capability", "global_memory_bytes",
        } and payload["device"]["index"] == 0 and
                isinstance(payload["device"]["name"], str) and
                payload["device"]["compute_capability"] ==
                f"{args.architecture[:1]}.{args.architecture[1:]}",
                "CUDA transform device identity changed")
        require(isinstance(payload["cuda_versions"], dict) and
                set(payload["cuda_versions"]) == {
                    "driver_api_supported", "runtime", "toolkit_build",
                }, "CUDA transform version JSON fields changed")
        cuda_data = load_transcript(cuda_path, len(records), OUTPUT_MAGIC)
        comparison = compare_transcripts(core_data, cuda_data, origins)

        output_failure = subprocess.run(
            [args.device_stage, "--input", str(input_path),
             "--output", str(evidence_dir / "missing" / "transcript.bin")],
            capture_output=True, text=True, timeout=45, check=False,
        )
        output_error = validate_device_error(output_failure)
        require(output_error["code"] == "output_failure" and
                output_error["cuda_error"] is None,
                "host output failure must remain distinct from CUDA failures")

    report = {
        "schema_version": 1,
        "diagnostic_only": True,
        "bitwise_parity_passed": (
            comparison["bitwise_parity_passed"] if comparison is not None else None
        ),
        "core_sha": CORE_SHA,
        "openlibm_sha": OPENLIBM_SHA,
        "stage": STAGE_NAME,
        "gpu_status": gpu_status,
        "device": payload["device"] if gpu_status == "live" else None,
        "cuda_versions": (
            payload["cuda_versions"] if gpu_status == "live" else None
        ),
        "device_error": device_error,
        "input_count": len(records),
        "real_header_count": corpus_metadata["real_header_count"],
        "matrix_seed_count": corpus_metadata["seed_count"],
        "matrix_transcript_sha256": corpus_metadata["matrix_transcript_sha256"],
        "matrix_oracle_summary": corpus_metadata["matrix_summary"],
        "input_sha256": hashlib.sha256(input_data).hexdigest(),
        "core_transcript_sha256": hashlib.sha256(core_data).hexdigest(),
        "openlibm_library_sha256": hashlib.sha256(
            Path(args.openlibm_library).read_bytes()
        ).hexdigest(),
        "transform_oracle_compiler": args.oracle_compiler,
        "cuda_architecture": args.architecture,
        "cuda_transcript_sha256": (
            hashlib.sha256(cuda_path.read_bytes()).hexdigest()
            if gpu_status == "live" else None
        ),
        "output_record_bytes": OUTPUT_RECORD.size,
        "build_policy": {
            "cuda_compile_flags_required": [
                "--fmad=false", "--ftz=false",
                "--prec-div=true", "--prec-sqrt=true"
            ],
            "cuda_fast_math": False,
            "cpu_oracle_compile_flags_required": [
                "-fno-fast-math", "-ffp-contract=off", "-frounding-math",
                "-fno-builtin"
            ],
            "cuda_device_architecture": os.environ.get(
                "PEPEPOW_CUDA_TEST_ARCHITECTURE", args.architecture
            ),
        },
        "input_special_cases": [
            origin for origin in origins if origin.startswith("special:")
        ],
        "input_catalog": catalog_path.name,
        "core_oracle_summary": core_summary,
        "branch_mismatches": (
            comparison["branch_mismatches"] if comparison is not None else None
        ),
        "branch_mismatch_count": (
            comparison["branch_mismatch_count"] if comparison is not None else None
        ),
        "core_branch_counts": (
            comparison["core_branch_counts"] if comparison is not None else None
        ),
        "cuda_branch_counts": (
            comparison["cuda_branch_counts"] if comparison is not None else None
        ),
        "functions": comparison["functions"] if comparison is not None else None,
    }
    report_path.write_text(
        json.dumps(report, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
    )

    print(f"PINNED SOURCE: Core {CORE_SHA}; OpenLibm {OPENLIBM_SHA}")
    print(f"INPUT CORPUS: {len(records)} cases; "
          f"{corpus_metadata['real_header_count']} headers with Core-generated "
          "matrix values; singularity, branch-boundary, and special-value cases")
    print(f"CUDA DEVICE: {gpu_status}" +
          (f" ({device_error['code']})" if device_error else ""))
    print(f"REPORT: {report_path}")
    if comparison is not None:
        print(f"BRANCH CONTROL: {comparison['branch_mismatch_count']} "
              "CPU/GPU branch-selection mismatches")
        for function, summary in comparison["functions"].items():
            print(
                f"{function}: exact={summary['bitwise_equal']}/{summary['cases']}, "
                f"bitwise_mismatches={summary['bitwise_mismatches']}, "
                f"classification_mismatches={summary['classification_mismatches']}, "
                f"max_abs={summary['max_absolute_error']:.17g}, "
                f"max_rel={summary['max_relative_error']:.17g}, "
                f"max_ulp={summary['max_ulp_difference']}"
            )
        print("FP64 TRANSFORM RESULTS ARE DIAGNOSTIC ONLY; no GPU digest parity is claimed")
    else:
        print("NO-GPU CONTRACT: Core/OpenLibm baseline saved; device execution explicitly skipped")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (TransformDiagnosticError, OSError, subprocess.TimeoutExpired) as error:
        print(f"CUDA HOOHASH TRANSFORM DIAGNOSTIC FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
