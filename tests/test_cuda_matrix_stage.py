#!/usr/bin/env python3
"""Compare full CUDA PRNG/matrix transcripts with the pinned Core oracle."""

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from cuda_device_policy import should_skip_device_execution
from test_cuda_core_matrix_oracle import (
    INPUT_MAGIC,
    OUTPUT_MAGIC,
    make_seeds,
    run_oracle,
)


ERROR_EXIT_CODES = {
    "no_device": 2,
    "driver_unavailable": 3,
    "initialization_failed": 4,
    "cuda_failure": 5,
    "output_failure": 6,
    "invalid_arguments": 64,
}
ELEMENTS_PER_SEED = 4096
CASE_BYTES = 32 + 32 + ELEMENTS_PER_SEED * 52
MASK64 = (1 << 64) - 1


class MatrixStageError(AssertionError):
    pass


def require(condition, message):
    if not condition:
        raise MatrixStageError(message)


def first_difference(left, right):
    common = min(len(left), len(right))
    for offset in range(common):
        if left[offset] != right[offset]:
            if offset < 12:
                return f"binary header byte {offset}"
            case_offset = offset - 12
            case_index, within_case = divmod(case_offset, CASE_BYTES)
            if within_case < 64:
                return f"seed {case_index}, seed/initial-state byte {within_case}"
            element_index, within_element = divmod(within_case - 64, 52)
            field = "matrix binary64 bits" if within_element >= 44 else (
                "lower32" if within_element >= 40 else
                "post-step PRNG state" if within_element >= 8 else
                "PRNG output"
            )
            return f"seed {case_index}, row-major element {element_index}, {field}, byte {within_element}"
    if len(left) != len(right):
        return f"transcript length {len(left)} != {len(right)} at byte {common}"
    return "no difference"


def compare_transcript(expected, actual):
    if expected != actual:
        raise MatrixStageError(
            "pinned Core/CUDA transcript mismatch at " +
            first_difference(expected, actual)
        )


def count_prng_wraps(seed):
    state = list(int.from_bytes(seed[index:index + 8], "little")
                 for index in range(0, 32, 8))
    wraps = 0
    for _ in range(ELEMENTS_PER_SEED):
        s0, s1, s2, s3 = state
        first_sum = s0 + s3
        wraps += first_sum > MASK64
        wrapped_first_sum = first_sum & MASK64
        rotated = ((wrapped_first_sum << 23) | (wrapped_first_sum >> 41)) & MASK64
        output_sum = rotated + s0
        wraps += output_sum > MASK64
        transition = s1 << 17
        wraps += transition > MASK64
        s2 ^= s0
        s3 ^= s1
        s1 ^= s2
        s0 ^= s3
        s2 ^= transition & MASK64
        s3 = ((s3 << 45) | (s3 >> 19)) & MASK64
        state = [s0 & MASK64, s1 & MASK64, s2 & MASK64, s3]
    return wraps


def invoke_device_stage(executable, seed_path, output_path):
    return subprocess.run(
        [executable, "--input", str(seed_path), "--output", str(output_path)],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )


def validate_error(completed):
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise MatrixStageError(f"CUDA stage error response is not JSON: {error}") from error
    require(isinstance(payload, dict) and set(payload) == {
        "schema_version", "status", "stage", "error",
    }, "CUDA matrix error JSON fields changed")
    require(payload["schema_version"] == 1 and payload["status"] == "error" and
            payload["stage"] == "hoohash_v110_matrix_prng_4096",
            "CUDA matrix error JSON contract changed")
    error = payload["error"]
    require(isinstance(error, dict) and set(error) == {
        "code", "cuda_error", "exit_code",
    }, "CUDA matrix error fields changed")
    require(error["code"] in ERROR_EXIT_CODES,
            f"unexpected CUDA matrix stage error: {error!r}")
    require((isinstance(error["cuda_error"], str)
             if error["code"] != "output_failure"
             else error["cuda_error"] is None),
            "CUDA and host-output errors must keep distinct diagnostics")
    require(error["exit_code"] == ERROR_EXIT_CODES[error["code"]] and
            completed.returncode == error["exit_code"],
            "CUDA matrix JSON and process exit codes disagree")
    return error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", required=True)
    parser.add_argument("--blake3-oracle", required=True)
    parser.add_argument("--device-stage", required=True)
    args = parser.parse_args()

    # Validate deterministic input rejection before any CUDA Runtime call.
    invalid = subprocess.run(
        [args.device_stage], capture_output=True, text=True, timeout=5, check=False
    )
    invalid_error = validate_error(invalid)
    require(invalid_error["code"] == "invalid_arguments" and invalid.returncode == 64,
            "invalid CUDA matrix invocation must return stable JSON and exit 64")

    seeds = make_seeds(args.blake3_oracle)
    seeds.append(b"\xff" * 32)
    require(len(set(seeds)) == len(seeds), "overflow seed must be distinct")
    overflow_count = count_prng_wraps(seeds[-1])
    require(overflow_count > 0,
            "all-FF state must exercise uint64 wraparound in the pinned PRNG operations")
    corpus = INPUT_MAGIC + len(seeds).to_bytes(4, "little") + b"".join(seeds)
    with tempfile.TemporaryDirectory(prefix="pepepow-cuda-matrix-") as directory:
        root = Path(directory)
        seed_path = root / "matrix-seeds.bin"
        cpu_path = root / "pinned-core-transcript.bin"
        gpu_path = root / "cuda-transcript.bin"
        seed_path.write_bytes(corpus)
        malformed_path = root / "malformed-seeds.bin"
        malformed_path.write_bytes(b"invalid")
        malformed = subprocess.run(
            [args.device_stage, "--input", str(malformed_path),
             "--output", str(root / "unused.bin")],
            capture_output=True, text=True, timeout=5, check=False,
        )
        malformed_error = validate_error(malformed)
        require(malformed_error["code"] == "invalid_arguments" and
                malformed.returncode == 64,
                "invalid seed corpus must fail before CUDA initialization")
        cpu_summary = run_oracle(args.oracle, seed_path, cpu_path)
        expected = cpu_path.read_bytes()
        require(expected[:8] == OUTPUT_MAGIC, "pinned Core transcript magic changed")
        require(len(expected) == 12 + len(seeds) * CASE_BYTES,
                "pinned Core transcript has an unexpected size")

        # Negative control: corrupt the last matrix element's final bit and
        # prove that the complete transcript comparator detects it.
        mutated = bytearray(expected)
        mutated[-1] ^= 1
        try:
            compare_transcript(expected, bytes(mutated))
        except MatrixStageError as error:
            require("element 4095" in str(error) and
                    "matrix binary64 bits" in str(error),
                    f"last-element negative control found the wrong field: {error}")
        else:
            raise MatrixStageError("mutating the last matrix element was not detected")

        completed = invoke_device_stage(args.device_stage, seed_path, gpu_path)
        if completed.returncode != 0:
            error = validate_error(completed)
            allow_skip = os.environ.get("PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP") == "1"
            require_device = os.environ.get("PEPEPOW_CUDA_REQUIRE_DEVICE") == "1"
            if should_skip_device_execution(
                error["code"], allow_no_device_skip=allow_skip,
                require_device=require_device,
            ):
                print(f"SKIP: explicit no-GPU job recognized {error['code']}")
                print(cpu_summary)
                print(f"UINT64 OVERFLOW CASE: all-FF initial state exercised {overflow_count} modulo-2^64 wrap events")
                print("CPU MATRIX ORACLE: all matrix bits match the independent transcript")
                print("NEGATIVE CONTROL: final matrix element mutation rejected")
                return 0
            raise MatrixStageError(
                f"required CUDA matrix stage failed: {error['code']} "
                f"({error['cuda_error']}); stderr={completed.stderr.strip()}"
            )

        try:
            payload = json.loads(completed.stdout)
        except json.JSONDecodeError as error:
            raise MatrixStageError(f"CUDA success response is not JSON: {error}") from error
        require(isinstance(payload, dict) and set(payload) == {
            "schema_version", "status", "stage", "seed_count",
            "elements_per_seed", "observation_record_bytes", "error",
        }, "CUDA matrix success JSON fields changed")
        require(payload["schema_version"] == 1 and payload["status"] == "ok" and
                payload["stage"] == "hoohash_v110_matrix_prng_4096" and
                payload["seed_count"] == len(seeds) and
                payload["elements_per_seed"] == ELEMENTS_PER_SEED and
                payload["observation_record_bytes"] == 52 and payload["error"] is None,
                "CUDA matrix success JSON contract changed")
        require(completed.returncode == 0, "CUDA matrix stage success must exit 0")
        actual = gpu_path.read_bytes()
        compare_transcript(expected, actual)

        output_failure = invoke_device_stage(
            args.device_stage, seed_path, root / "missing-directory" / "output.bin"
        )
        output_error = validate_error(output_failure)
        require(output_error["code"] == "output_failure" and
                output_error["cuda_error"] is None and output_failure.returncode == 6,
                "host output failure must not be mislabeled as a CUDA error")

    print(cpu_summary)
    print(f"UINT64 OVERFLOW CASE: all-FF initial state exercised {overflow_count} modulo-2^64 wrap events")
    print(f"LIVE CUDA MATRIX: {len(seeds)} seeds x 4096 elements; "
          f"{len(seeds) * ELEMENTS_PER_SEED} PRNG values, "
          "states, and binary64 patterns match pinned Core byte-for-byte")
    print("NEGATIVE CONTROL: final matrix element mutation rejected")
    print("ERROR CONTRACT: malformed corpus and host output failures return stable nonzero JSON")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (MatrixStageError, OSError, subprocess.TimeoutExpired) as error:
        print(f"CUDA MATRIX STAGE CONTRACT FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
