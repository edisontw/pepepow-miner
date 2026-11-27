#!/usr/bin/env python3
"""Compare full CUDA HooHash V110 digests with the pinned Core oracle."""

import argparse
import hashlib
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
from contextlib import nullcontext
from pathlib import Path

from cuda_device_policy import should_skip_device_execution
from test_cuda_blake3_stage import HEADER_NONCE6, UINT32_MAX


OUTPUT_MAGIC = b"PPWDIG1\0"
OUTPUT_HEADER = struct.Struct("<8sI")
ERROR_EXIT_CODES = {
    "no_device": 2,
    "driver_unavailable": 3,
    "initialization_failed": 4,
    "cuda_failure": 5,
    "output_failure": 6,
    "invalid_arguments": 64,
}
CORE_SHA = "bc08108ceccd957c79040dc7fedc1d56d8d47f97"
OPENLIBM_SHA = "9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d"
# Filled from the pinned Core/OpenLibm oracle and intentionally held fixed so
# a changed CPU oracle cannot silently become its own expected result.
PINNED_DIGESTS = {
    0: "18c43a01e1ca2f574b34b80fd1ba1e6523e8ffb3701f73c3127804bfe57649c2",
    6: "a0cba391e2eda64e067cf2c4f2209370bae8b31ac7ff4cc6cf141ebb25073a15",
    7: "9e96e181670e3b198f0bfb4c8c6586dd431c9d422628577b575e484f8068a8e7",
    8: "90f260f1decd6ee5dbf494e8e25dfba16b3db3c13146719499d655f2b58d8fbd",
    UINT32_MAX: "4195b28c3892661c3bd563d368b4cd141d880921fb7652ae3bcd34f64ea39b61",
}


class DigestStageError(AssertionError):
    pass


def require(condition, message):
    if not condition:
        raise DigestStageError(message)


def run_checked(command, *, timeout=60):
    completed = subprocess.run(
        command, capture_output=True, text=True, timeout=timeout, check=False
    )
    require(completed.returncode == 0,
            f"command failed ({completed.returncode}): {' '.join(map(str, command))}\n"
            f"stdout: {completed.stdout.strip()}\nstderr: {completed.stderr.strip()}")
    return completed


def read_transcript(path, expected_count):
    payload = Path(path).read_bytes()
    require(len(payload) >= OUTPUT_HEADER.size,
            "digest transcript is shorter than its header")
    magic, count = OUTPUT_HEADER.unpack_from(payload)
    require(magic == OUTPUT_MAGIC and count == expected_count,
            "digest transcript magic or count changed")
    require(len(payload) == OUTPUT_HEADER.size + count * 32,
            "digest transcript must contain every complete 32-byte digest")
    return [payload[OUTPUT_HEADER.size + index * 32:
                    OUTPUT_HEADER.size + (index + 1) * 32]
            for index in range(count)]


def oracle_digests(oracle, header, start, count, output):
    completed = run_checked([
        oracle, "--header-hex", header.hex(), "--nonce-start", str(start),
        "--nonce-count", str(count), "--output", str(output),
    ], timeout=120)
    require(completed.stdout.strip() ==
            f"PINNED CORE digest oracle: {count} complete 32-byte digests",
            "CPU oracle summary changed")
    return read_transcript(output, count)


def validate_error(completed):
    require(completed.stdout.strip(), "CUDA stage must emit one JSON line")
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise DigestStageError(f"CUDA output is not JSON: {error}") from error
    require(isinstance(payload, dict) and set(payload) == {
        "schema_version", "status", "stage", "nonce_start",
        "nonce_count", "error",
    }, "CUDA error JSON fields changed")
    require(payload["schema_version"] == 1 and payload["status"] == "error" and
            payload["stage"] == "hoohash_v110_full_digest_experiment" and
            payload["nonce_start"] is None and payload["nonce_count"] is None,
            "CUDA error JSON schema or stage changed")
    error = payload["error"]
    require(isinstance(error, dict) and set(error) == {
        "code", "cuda_error", "exit_code",
    }, "CUDA error object fields changed")
    code = error["code"]
    require(code in ERROR_EXIT_CODES and
            error["exit_code"] == ERROR_EXIT_CODES[code] and
            completed.returncode == error["exit_code"],
            "CUDA JSON and process exit codes disagree")
    return error


def validate_success(completed, start, count):
    require(completed.returncode == 0, "CUDA full-digest stage must exit 0")
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise DigestStageError(f"CUDA success output is not JSON: {error}") from error
    require(isinstance(payload, dict) and set(payload) == {
        "schema_version", "status", "stage", "nonce_start", "nonce_count",
        "digest_count", "gpu_pipeline_ms", "gpu_pipeline_digests_per_second",
        "gpu_pipeline_timing_scope", "device", "cuda_versions", "error",
    }, "CUDA success JSON fields changed")
    require(payload["schema_version"] == 1 and payload["status"] == "ok" and
            payload["stage"] == "hoohash_v110_full_digest_experiment" and
            payload["nonce_start"] == start and payload["nonce_count"] == count and
            payload["digest_count"] == count and payload["error"] is None,
            "CUDA success metadata changed")
    elapsed_ms = payload["gpu_pipeline_ms"]
    digests_per_second = payload["gpu_pipeline_digests_per_second"]
    timing_scope = (
        "CUDA event kernel-only; excludes host setup, allocation, transfer, "
        "oracle, and output; not miner hashrate"
    )
    require(isinstance(elapsed_ms, (int, float)) and
            not isinstance(elapsed_ms, bool) and math.isfinite(elapsed_ms) and
            elapsed_ms > 0 and isinstance(digests_per_second, (int, float)) and
            not isinstance(digests_per_second, bool) and
            math.isfinite(digests_per_second) and digests_per_second > 0 and
            math.isclose(digests_per_second, count * 1000.0 / elapsed_ms,
                         rel_tol=1e-12),
            f"CUDA event timing must be finite, positive, and self-consistent "
            f"(elapsed_ms={elapsed_ms!r}, "
            f"digests_per_second={digests_per_second!r}, "
            f"expected={count * 1000.0 / elapsed_ms if isinstance(elapsed_ms, (int, float)) and elapsed_ms > 0 else None!r})")
    require(payload["gpu_pipeline_timing_scope"] == timing_scope,
            "CUDA event timing scope changed")
    device = payload["device"]
    require(isinstance(device, dict) and set(device) == {
        "index", "name", "compute_capability",
    } and device["index"] == 0 and isinstance(device["name"], str) and
            isinstance(device["compute_capability"], str),
            "CUDA device identity is invalid")
    versions = payload["cuda_versions"]
    require(isinstance(versions, dict) and set(versions) == {
        "driver_api", "runtime_api", "toolkit_build",
    } and all(isinstance(versions[key], (int, str)) for key in versions),
            "CUDA version metadata is invalid")
    return payload


def validate_pinned_digests(start, digests):
    for offset, digest in enumerate(digests):
        nonce = start + offset
        fixed = PINNED_DIGESTS.get(nonce)
        if fixed:
            require(digest.hex() == fixed,
                    f"pinned Core fixture changed at nonce {nonce}: "
                    f"expected={fixed} actual={digest.hex()}")


def validate_success_contract_fixture():
    payload = {
        "schema_version": 1,
        "status": "ok",
        "stage": "hoohash_v110_full_digest_experiment",
        "nonce_start": 23,
        "nonce_count": 4,
        "digest_count": 4,
        "gpu_pipeline_ms": 2.5,
        "gpu_pipeline_digests_per_second": 1600.0,
        "gpu_pipeline_timing_scope": (
            "CUDA event kernel-only; excludes host setup, allocation, transfer, "
            "oracle, and output; not miner hashrate"
        ),
        "device": {
            "index": 0,
            "name": "mock-device",
            "compute_capability": "8.6",
        },
        "cuda_versions": {
            "driver_api": 13030,
            "runtime_api": 13030,
            "toolkit_build": "13.3.1",
        },
        "error": None,
    }
    valid = subprocess.CompletedProcess(
        args=["mock-cuda-stage"], returncode=0,
        stdout=json.dumps(payload), stderr="",
    )
    validate_success(valid, 23, 4)

    invalid_payloads = []
    invalid_elapsed = dict(payload, gpu_pipeline_ms=float("nan"))
    invalid_payloads.append(invalid_elapsed)
    invalid_rate = dict(payload, gpu_pipeline_digests_per_second=1599.0)
    invalid_payloads.append(invalid_rate)
    invalid_scope = dict(payload, gpu_pipeline_timing_scope="total host time")
    invalid_payloads.append(invalid_scope)
    for invalid in invalid_payloads:
        completed = subprocess.CompletedProcess(
            args=["mock-cuda-stage"], returncode=0,
            stdout=json.dumps(invalid), stderr="",
        )
        try:
            validate_success(completed, 23, 4)
        except DigestStageError:
            continue
        raise DigestStageError("CUDA success JSON accepted an invalid mock fixture")
    print("MOCK CUDA success JSON contract: PASS")


def run_case(stage, header, start, count, root, cpu):
    gpu_path = root / f"gpu-{start}-{count}.bin"
    completed = subprocess.run([
        stage, "--header-hex", header.hex(), "--nonce-start", str(start),
        "--nonce-count", str(count), "--output", str(gpu_path),
    ], capture_output=True, text=True, timeout=240, check=False)
    if completed.returncode != 0:
        error = validate_error(completed)
        allow_skip = os.environ.get("PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP") == "1"
        require_device = os.environ.get("PEPEPOW_CUDA_REQUIRE_DEVICE") == "1"
        require(should_skip_device_execution(
            error["code"], allow_no_device_skip=allow_skip,
            require_device=require_device,
        ), f"CUDA digest execution failed with {error['code']} "
           f"({error['cuda_error']}); only explicit no-device jobs may skip")
        return {"status": "skipped_no_device", "error": error["code"],
                "device": None, "gpu_path": None}

    payload = validate_success(completed, start, count)
    gpu = read_transcript(gpu_path, count)
    for offset, (cpu_digest, gpu_digest) in enumerate(zip(cpu, gpu)):
        if cpu_digest != gpu_digest:
            raise DigestStageError(
                f"full 32-byte digest mismatch at nonce {start + offset}: "
                f"CPU={cpu_digest.hex()} CUDA={gpu_digest.hex()}"
            )
    print(f"LIVE CUDA: {start}..{start + count - 1}, {count} full 32-byte "
          f"digests match pinned Core on {payload['device']['name']}")
    return {"status": "live", "error": None, "device": payload["device"],
            "cuda_versions": payload["cuda_versions"], "gpu_path": gpu_path,
            "gpu_pipeline_ms": payload["gpu_pipeline_ms"],
            "gpu_pipeline_digests_per_second": payload[
                "gpu_pipeline_digests_per_second"]}


def write_report(path, header, cases, result):
    reports = []
    for start, count, cpu_path, cpu_digests, device_result in cases:
        gpu_path = device_result["gpu_path"]
        reports.append({
            "nonce_start": start,
            "nonce_count": count,
            "cpu_transcript": cpu_path.name,
            "cpu_transcript_sha256": hashlib.sha256(cpu_path.read_bytes()).hexdigest(),
            "gpu_transcript": gpu_path.name if gpu_path else None,
            "gpu_transcript_sha256": (
                hashlib.sha256(gpu_path.read_bytes()).hexdigest()
                if gpu_path else None
            ),
            "complete_digests_compared": len(cpu_digests),
            "status": device_result["status"],
            "device_error": device_result["error"],
            "gpu_pipeline_ms": device_result.get("gpu_pipeline_ms"),
            "gpu_pipeline_digests_per_second": device_result.get(
                "gpu_pipeline_digests_per_second"),
        })
    report = {
        "schema_version": 1,
        "stage": "hoohash_v110_full_digest_experiment",
        "diagnostic_only": True,
        "full_digest_parity_passed": result == "live",
        "gpu_status": result,
        "core_sha": CORE_SHA,
        "openlibm_sha": OPENLIBM_SHA,
        "header_template_hex": header.hex(),
        "nonce_encoding": "little_endian_uint32_at_header_offsets_76_to_79",
        "digest_bytes_compared_per_case": 32,
        "timing_scope": (
            "CUDA event time for the three kernel pipeline only; excludes host "
            "setup, allocations, transfers, oracle work, and output; not miner "
            "hashrate"
        ),
        "build_policy": {
            "cuda_compile_flags_required": [
                "--fmad=false", "--ftz=false", "--prec-div=true",
                "--prec-sqrt=true",
            ],
            "cuda_fast_math": False,
        },
        "cases": reports,
    }
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", required=True)
    parser.add_argument("--device-stage", required=True)
    parser.add_argument("--batch-start", type=int)
    parser.add_argument("--batch-count", type=int)
    parser.add_argument("--evidence-dir")
    args = parser.parse_args()

    require((args.batch_start is None) == (args.batch_count is None),
            "--batch-start and --batch-count must be supplied together")
    if args.batch_start is not None:
        require(0 <= args.batch_start <= UINT32_MAX and
                1 <= args.batch_count <= 10000 and
                args.batch_start + args.batch_count - 1 <= UINT32_MAX,
                "optional digest batch must fit the bounded uint32 nonce range")

    validate_success_contract_fixture()
    header = HEADER_NONCE6
    temporary_context = (
        nullcontext(Path(args.evidence_dir)) if args.evidence_dir else
        tempfile.TemporaryDirectory(prefix="pepepow-cuda-digest-")
    )
    with temporary_context as temporary:
        root = Path(temporary)
        root.mkdir(parents=True, exist_ok=True)
        # Include nonce zero, an Explorer nonce and its neighbors, and the
        # uint32 boundary. A requested larger physical batch is added after
        # those fixed fixtures so errors retain a small reproducer.
        ranges = [(0, 1), (6, 3), (UINT32_MAX, 1)]
        if args.batch_start is not None:
            ranges.append((args.batch_start, args.batch_count))

        prepared = []
        for start, count in ranges:
            cpu_path = root / f"cpu-{start}-{count}.bin"
            digests = oracle_digests(args.oracle, header, start, count, cpu_path)
            validate_pinned_digests(start, digests)
            prepared.append((start, count, cpu_path, digests))
        print("PINNED CORE: all complete 32-byte fixed digest fixtures PASS")

        device_cases = []
        for start, count, cpu_path, cpu_digests in prepared:
            device_result = run_case(args.device_stage, header, start, count,
                                     root, cpu_digests)
            device_cases.append((start, count, cpu_path, cpu_digests,
                                 device_result))
            if device_result["status"] == "skipped_no_device":
                print("NO GPU: pinned Core full-digest fixtures passed; "
                      f"device execution explicitly skipped ({device_result['error']})")
                write_report(root / "full-digest-report.json", header,
                             device_cases, "skipped_no_device")
                return 0

        invalid = subprocess.run([
            args.device_stage, "--header-hex", header.hex(),
            "--nonce-start", str(UINT32_MAX), "--nonce-count", "2",
            "--output", str(root / "invalid.bin"),
        ], capture_output=True, text=True, timeout=10, check=False)
        error = validate_error(invalid)
        require(error["code"] == "invalid_arguments",
                "uint32 nonce wraparound must fail before CUDA initialization")
        write_report(root / "full-digest-report.json", header, device_cases, "live")

    print("PINNED CORE FULL-DIGEST CORRECTNESS PASS; no production GPU miner "
          "or hashrate claim is made")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (DigestStageError, OSError, subprocess.TimeoutExpired) as error:
        print(f"CUDA HOOHASH FULL-DIGEST TEST FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
