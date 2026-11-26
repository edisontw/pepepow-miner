#!/usr/bin/env python3
"""Compare the first HooHash V110 BLAKE3 stage with pinned Core."""

import argparse
import json
import os
import re
import subprocess
import sys


HEADER_NONCE6 = bytes.fromhex(
    "0040002003020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c"
    "5b5d507845ead90ea4c4caeef4baef7817c0fb2169d20739883e22bb6d8cf8ef0be1697171e3021d06000000"
)
UINT32_MAX = (1 << 32) - 1
ERROR_EXIT_CODES = {
    "no_device": 2,
    "driver_unavailable": 3,
    "initialization_failed": 4,
    "cuda_failure": 5,
    "invalid_arguments": 64,
}

# Filled from and checked against the BLAKE3 implementation at the pinned
# PEPEPOW Core revision before accepting any CUDA output.
EXPECTED = {
    ("nonce-6", "header"): "7232d2fdfba6031bba97f015ce7c6cadc582d710342b06be9e16a3953312edeb",
    ("nonce-6", "matrix-seed"): "28d0be17bea0de270743b7c09c785157504aecc287f62e2673051bbadf764d6d",
    ("uint32-max", "header"): "5549afc7e9658f8a77bea3a2e1fd3ddbad2171ddab0facac8339fec2d4b3075c",
    ("uint32-max", "matrix-seed"): "28d0be17bea0de270743b7c09c785157504aecc287f62e2673051bbadf764d6d",
}


class StageError(AssertionError):
    pass


def require(condition, message):
    if not condition:
        raise StageError(message)


def header_with_nonce(nonce):
    header = bytearray(HEADER_NONCE6)
    header[76:80] = nonce.to_bytes(4, "little")
    return bytes(header)


def run_cpu_oracle(oracle, header, mode):
    completed = subprocess.run(
        [oracle, header.hex(), mode],
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    require(completed.returncode == 0,
            f"pinned-Core BLAKE3 oracle failed ({mode}): {completed.stderr.strip()}")
    digest = completed.stdout.strip()
    require(re.fullmatch(r"[0-9a-f]{64}", digest) is not None,
            "pinned-Core BLAKE3 oracle must emit exactly 32 bytes as lowercase hex")
    return digest


def validate_gpu_error(completed, payload):
    require(isinstance(payload, dict) and set(payload) == {
        "schema_version", "status", "stage", "input_mode",
        "input_size_bytes", "digest_hex", "error",
    }, "CUDA error JSON fields changed")
    require(payload["schema_version"] == 1 and payload["status"] == "error",
            "CUDA error JSON schema/status changed")
    require(payload["stage"] == "hoohash_v110_blake3_80" and
            payload["input_size_bytes"] == 80,
            "CUDA error lost the BLAKE3 stage contract")
    error = payload["error"]
    require(isinstance(error, dict) and set(error) == {
        "code", "cuda_error", "exit_code",
    }, "CUDA error object fields changed")
    require(error["code"] in {"no_device", "driver_unavailable",
                               "initialization_failed", "cuda_failure"},
            "unexpected CUDA runtime failure")
    require(error["exit_code"] == ERROR_EXIT_CODES[error["code"]] and
            completed.returncode == error["exit_code"],
            "CUDA JSON and process exit codes disagree")
    return error


def validate_gpu_success(completed, payload, mode, expected):
    require(completed.returncode == 0, "CUDA BLAKE3 stage must exit 0")
    require(isinstance(payload, dict) and set(payload) == {
        "schema_version", "status", "stage", "input_mode",
        "input_size_bytes", "digest_hex", "error",
    }, "CUDA success JSON fields changed")
    require(payload["schema_version"] == 1 and payload["status"] == "ok",
            "CUDA success JSON schema/status changed")
    require(payload["stage"] == "hoohash_v110_blake3_80" and
            payload["input_mode"] == mode and payload["input_size_bytes"] == 80,
            "CUDA result does not identify the requested 80-byte BLAKE3 stage")
    require(payload["error"] is None, "CUDA success error must be null")
    digest = payload["digest_hex"]
    require(isinstance(digest, str) and re.fullmatch(r"[0-9a-f]{64}", digest),
            "CUDA stage must return the complete 32-byte digest")
    require(digest == expected,
            f"full BLAKE3 digest mismatch: CPU={expected} CUDA={digest}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", required=True)
    parser.add_argument("--device-stage", required=True)
    args = parser.parse_args()

    vectors = {
        "nonce-6": header_with_nonce(6),
        "uint32-max": header_with_nonce(UINT32_MAX),
    }
    expected_results = {}
    for vector_name, header in vectors.items():
        for mode in ("header", "matrix-seed"):
            expected = run_cpu_oracle(args.oracle, header, mode)
            pinned = EXPECTED[(vector_name, mode)]
            require(pinned and expected == pinned,
                    f"pinned Core BLAKE3 fixture changed for {vector_name}/{mode}: "
                    f"expected={pinned or '<unset>'} actual={expected}")
            expected_results[(vector_name, mode)] = expected
    print("PINNED CORE: four complete 80-byte BLAKE3 stage vectors PASS")

    for (vector_name, mode), expected in expected_results.items():
        completed = subprocess.run(
            [args.device_stage, "--header-hex", vectors[vector_name].hex(),
             "--mode", mode],
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
        require(completed.stdout.strip(), "CUDA stage must emit one JSON line")
        try:
            payload = json.loads(completed.stdout)
        except json.JSONDecodeError as error:
            raise StageError(f"CUDA stage stdout is not JSON: {error}") from error

        if isinstance(payload, dict) and payload.get("status") == "error":
            error = validate_gpu_error(completed, payload)
            require(os.environ.get("PEPEPOW_CUDA_REQUIRE_DEVICE") != "1",
                    f"physical GPU test required but CUDA returned {error['code']}")
            print(f"NO GPU: device stage execution skipped ({error['code']}); CPU vectors passed")
            return 0

        validate_gpu_success(completed, payload, mode, expected)
        print(f"LIVE CUDA: {vector_name}/{mode}, complete 32-byte digest PASS")

    print("CUDA BLAKE3 stage parity PASS; full HooHash V110 parity is not claimed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (StageError, OSError, subprocess.TimeoutExpired) as error:
        print(f"CUDA BLAKE3 stage FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
