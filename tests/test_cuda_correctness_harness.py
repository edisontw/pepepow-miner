#!/usr/bin/env python3
"""Check the pinned-Core/GPU batch contract before a HooHash CUDA kernel exists."""

import argparse
import json
import os
import re
import subprocess
import sys

from cuda_device_policy import should_skip_device_execution


HEADER_NONCE6 = bytes.fromhex(
    "0040002003020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c"
    "5b5d507845ead90ea4c4caeef4baef7817c0fb2169d20739883e22bb6d8cf8ef0be1697171e3021d06000000"
)
DIGEST_NONCE6 = bytes.fromhex(
    "a0cba391e2eda64e067cf2c4f2209370bae8b31ac7ff4cc6cf141ebb25073a15"
)
DIGEST_NONCE_MAX = bytes.fromhex(
    "4195b28c3892661c3bd563d368b4cd141d880921fb7652ae3bcd34f64ea39b61"
)
UINT32_MAX = (1 << 32) - 1
MAX_COVERAGE_BATCH = 4096
NO_DEVICE_CODES = {"no_device", "driver_unavailable", "initialization_failed"}
ERROR_EXIT_CODES = {
    "no_device": 2,
    "driver_unavailable": 3,
    "initialization_failed": 4,
    "query_failed": 5,
    "invalid_arguments": 64,
}


class ContractError(AssertionError):
    pass


def require(condition, message):
    if not condition:
        raise ContractError(message)


def header_with_nonce(header, nonce):
    require(len(header) == 80, "header must contain exactly 80 bytes")
    require(type(nonce) is int and 0 <= nonce <= UINT32_MAX,
            "nonce must fit uint32")
    result = bytearray(header)
    result[76:80] = nonce.to_bytes(4, "little")
    return bytes(result)


def make_request(header, nonce_start, nonce_count):
    require(len(header) == 80, "request header must contain exactly 80 bytes")
    require(type(nonce_start) is int and 0 <= nonce_start <= UINT32_MAX,
            "nonce_start must fit uint32")
    require(type(nonce_count) is int and 1 <= nonce_count <= MAX_COVERAGE_BATCH,
            f"nonce_count must be in 1..{MAX_COVERAGE_BATCH}")
    require(nonce_start + nonce_count <= 1 << 32,
            "nonce batch must not wrap the uint32 nonce space")
    require(int.from_bytes(header[76:80], "little") == nonce_start,
            "header nonce bytes must be little-endian nonce_start")
    return {
        "header_size_bytes": 80,
        "nonce_byte_order": "little-endian",
        "header_hex": header.hex(),
        "nonce_start": nonce_start,
        "nonce_count": nonce_count,
    }


def validate_response(request, payload):
    require(isinstance(payload, dict), "GPU response must be a JSON object")
    require(set(payload) == {
        "schema_version", "status", "contract", "device", "covered_nonces",
        "candidates", "error",
    }, "GPU response top-level contract changed")
    require(payload["schema_version"] == 1, "GPU response schema_version must be 1")
    require(payload["status"] == "ok", "GPU response status must be ok")
    require(payload["contract"] == request, "GPU response did not echo the exact request")
    require(payload["error"] is None, "successful GPU response error must be null")
    device = payload["device"]
    require(isinstance(device, dict) and set(device) == {"index", "name"},
            "GPU device metadata fields changed")
    require(type(device["index"]) is int and device["index"] >= 0,
            "GPU device index must be a non-negative integer")
    require(isinstance(device["name"], str) and device["name"],
            "GPU device name must be non-empty")

    expected = list(range(request["nonce_start"],
                          request["nonce_start"] + request["nonce_count"]))
    covered = payload["covered_nonces"]
    require(isinstance(covered, list), "covered_nonces must be an array")
    require(all(type(nonce) is int and 0 <= nonce <= UINT32_MAX for nonce in covered),
            "covered_nonces entries must be uint32 integers")
    require(len(covered) == len(set(covered)),
            "covered_nonces contains a duplicate nonce")
    require(sorted(covered) == expected,
            "covered_nonces has a missing, extra, or out-of-range nonce")

    candidates = payload["candidates"]
    require(isinstance(candidates, list), "candidates must be an array")
    candidate_nonces = set()
    for candidate in candidates:
        require(isinstance(candidate, dict) and
                set(candidate) == {"nonce", "digest_hex"},
                "candidate fields must be nonce and full digest_hex")
        nonce = candidate["nonce"]
        require(type(nonce) is int and nonce in expected,
                "candidate nonce must be covered by this batch")
        require(nonce not in candidate_nonces,
                "candidate list contains a duplicate nonce")
        candidate_nonces.add(nonce)
        digest_hex = candidate["digest_hex"]
        require(isinstance(digest_hex, str) and re.fullmatch(r"[0-9a-f]{64}", digest_hex),
                "candidate digest_hex must contain all 32 digest bytes")
    return candidates


def validate_error_response(payload, contract, allowed_codes):
    require(isinstance(payload, dict) and set(payload) == {
        "schema_version", "status", "contract", "device", "covered_nonces",
        "candidates", "error",
    }, "GPU error response top-level contract changed")
    require(payload["schema_version"] == 1 and payload["status"] == "error",
            "GPU error response schema/status changed")
    require(payload["contract"] == contract, "GPU error response lost the input contract")
    require(payload["device"] is None and payload["covered_nonces"] == [] and
            payload["candidates"] == [],
            "GPU error response must not invent device results")
    error = payload["error"]
    require(isinstance(error, dict) and set(error) == {
        "code", "cuda_error", "exit_code",
    }, "GPU error fields changed")
    require(error["code"] in allowed_codes, "GPU returned an unexpected error code")
    require(isinstance(error["cuda_error"], str) and error["cuda_error"],
            "GPU cuda_error must be a non-empty string")
    require(type(error["exit_code"]) is int and error["exit_code"] > 0,
            "GPU error exit_code must be a positive integer")
    require(error["exit_code"] == ERROR_EXIT_CODES[error["code"]],
            "GPU JSON error code and exit_code disagree")
    return error


def run_core_kat(oracle, header, expected_digest):
    require(len(header) == 80 and len(expected_digest) == 32,
            "Core KAT input sizes must be 80 and 32 bytes")
    completed = subprocess.run(
        [oracle, header.hex(), expected_digest.hex()],
        capture_output=True,
        text=True,
        timeout=15,
        check=False,
    )
    actual_lines = re.findall(r"^actual=([0-9a-f]{64})$", completed.stdout, re.MULTILINE)
    require(len(actual_lines) == 1, "pinned Core KAT must emit one full actual digest")
    actual = bytes.fromhex(actual_lines[0])
    if actual == expected_digest:
        require(completed.returncode == 0, "Core KAT must exit 0 for a full-digest match")
        require("MATCH" in completed.stdout, "Core KAT must report MATCH")
    else:
        require(completed.returncode == 1,
                "Core KAT must exit 1 when any byte of the full digest differs")
        require("MISMATCH" in completed.stderr,
                "Core KAT must report the full-digest mismatch")
    return actual


def recheck_candidates(oracle, request, candidates):
    base_header = bytes.fromhex(request["header_hex"])
    for candidate in candidates:
        header = header_with_nonce(base_header, candidate["nonce"])
        gpu_digest = bytes.fromhex(candidate["digest_hex"])
        cpu_digest = run_core_kat(oracle, header, gpu_digest)
        require(cpu_digest == gpu_digest,
                f"CPU recheck mismatch for nonce {candidate['nonce']}: "
                f"CPU={cpu_digest.hex()} GPU={gpu_digest.hex()}")


def run_json_process(command, label, timeout=10):
    completed = subprocess.run(command, capture_output=True, text=True,
                               timeout=timeout, check=False)
    require(completed.stdout.strip(), f"{label} must write JSON to stdout")
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise ContractError(f"{label} stdout is not valid JSON: {error}") from error
    return completed, payload


def make_mock_response(request, candidates):
    return {
        "schema_version": 1,
        "status": "ok",
        "contract": request,
        "device": {"index": 0, "name": "MOCK GPU provider"},
        "covered_nonces": list(range(
            request["nonce_start"], request["nonce_start"] + request["nonce_count"]
        )),
        "candidates": candidates,
        "error": None,
    }


def expect_contract_error(function, label):
    try:
        function()
    except ContractError:
        print(f"MOCK: {label} rejected PASS")
        return
    raise ContractError(f"negative contract fixture was accepted: {label}")


def test_cpu_oracle_and_candidate_recheck(oracle):
    base_header = bytearray(HEADER_NONCE6)
    base_header[76:80] = b"\x00\x00\x00\x00"
    header6 = header_with_nonce(bytes(base_header), 6)
    require(header6 == HEADER_NONCE6, "nonce byte-order fixture does not reconstruct header80")
    actual6 = run_core_kat(oracle, header6, DIGEST_NONCE6)
    require(actual6 == DIGEST_NONCE6, "pinned Core disagrees with the nonce-6 full digest")
    print("PINNED CORE: fixed 80-byte vector and all 32 digest bytes PASS")

    header_max = header_with_nonce(bytes(base_header), UINT32_MAX)
    actual_max = run_core_kat(oracle, header_max, DIGEST_NONCE_MAX)
    require(actual_max == DIGEST_NONCE_MAX,
            "pinned Core disagrees with the UINT32_MAX full digest")
    print("PINNED CORE: UINT32_MAX nonce vector and all 32 digest bytes PASS")

    request = make_request(header6, 6, 1)
    correct = make_mock_response(request, [{
        "nonce": 6,
        "digest_hex": DIGEST_NONCE6.hex(),
    }])
    candidates = validate_response(request, correct)
    recheck_candidates(oracle, request, candidates)
    print("MOCK GPU CANDIDATE: full-digest pinned-Core CPU recheck PASS")

    corrupt = make_mock_response(request, [{
        "nonce": 6,
        "digest_hex": (DIGEST_NONCE6[:-1] + bytes([DIGEST_NONCE6[-1] ^ 1])).hex(),
    }])

    def reject_last_byte_mutation():
        recheck_candidates(oracle, request, validate_response(request, corrupt))

    expect_contract_error(reject_last_byte_mutation,
                          "last digest byte mutation (full 32-byte compare)")


def test_coverage_contract_mutations():
    header = header_with_nonce(bytes(80), UINT32_MAX - 3)
    request = make_request(header, UINT32_MAX - 3, 4)
    good = make_mock_response(request, [])
    validate_response(request, good)
    print("MOCK: complete UINT32_MAX boundary batch coverage PASS")

    no_device = {
        "schema_version": 1,
        "status": "error",
        "contract": request,
        "device": None,
        "covered_nonces": [],
        "candidates": [],
        "error": {"code": "no_device", "cuda_error": "cudaErrorNoDevice",
                  "exit_code": 2},
    }
    validate_error_response(no_device, request, NO_DEVICE_CODES)
    print("MOCK: no-device JSON and nonzero exit contract PASS")

    duplicate = make_mock_response(request, [])
    duplicate["covered_nonces"][-1] = duplicate["covered_nonces"][-2]
    expect_contract_error(lambda: validate_response(request, duplicate),
                          "duplicate covered nonce")

    missing = make_mock_response(request, [])
    missing["covered_nonces"].pop()
    expect_contract_error(lambda: validate_response(request, missing),
                          "missing covered nonce")

    extra_candidate = make_mock_response(request, [{
        "nonce": 1,
        "digest_hex": "00" * 32,
    }])
    expect_contract_error(lambda: validate_response(request, extra_candidate),
                          "candidate outside batch")

    expect_contract_error(lambda: make_request(
        header_with_nonce(bytes(80), UINT32_MAX), UINT32_MAX, 2),
        "nonce range overflow beyond UINT32_MAX")

    bad_header = bytearray(header)
    bad_header[79] ^= 1
    expect_contract_error(lambda: make_request(bytes(bad_header), UINT32_MAX - 3, 4),
                          "header nonce/endian mismatch")


def test_live_cuda_coverage(executable):
    base_header = bytearray(80)
    base_header[0:4] = b"PPW1"

    overflow_header = header_with_nonce(bytes(base_header), UINT32_MAX)
    overflow_completed, overflow_payload = run_json_process([
        executable,
        "--header-hex", overflow_header.hex(),
        "--nonce-start", str(UINT32_MAX),
        "--nonce-count", "2",
    ], "invalid nonce overflow request")
    overflow_error = validate_error_response(
        overflow_payload, None, {"invalid_arguments"})
    require(overflow_error["exit_code"] == 64 and overflow_completed.returncode == 64,
            "overflow batch must return invalid-arguments exit code 64")
    print("LIVE CONTRACT: uint32 nonce overflow rejected before GPU work PASS")

    batch_start = UINT32_MAX - 31
    batch_header = header_with_nonce(bytes(base_header), batch_start)
    request = make_request(batch_header, batch_start, 32)
    command = [
        executable,
        "--header-hex", request["header_hex"],
        "--nonce-start", str(batch_start),
        "--nonce-count", str(request["nonce_count"]),
    ]
    completed, payload = run_json_process(command, "LIVE CUDA batch coverage")

    if payload.get("status") == "error":
        error = validate_error_response(payload, request, NO_DEVICE_CODES)
        require(completed.returncode == error.get("exit_code"),
                "CUDA coverage process exit must match JSON error")
        allow_skip = os.environ.get("PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP") == "1"
        require_device = os.environ.get("PEPEPOW_CUDA_REQUIRE_DEVICE") == "1"
        require(should_skip_device_execution(
            error["code"],
            allow_no_device_skip=allow_skip,
            require_device=require_device,
        ), f"live CUDA batch failed with {error['code']}; only explicitly enabled "
           "no-device CI may skip recognized device-unavailability errors")
        print(f"NO GPU: skipped live batch coverage ({error['code']})")
        return

    require(completed.returncode == 0, "successful GPU coverage must exit 0")
    candidates = validate_response(request, payload)
    require(candidates == [], "coverage-only kernel must not fabricate hash candidates")
    print("LIVE CUDA DEVICE: 32-nonce coverage near UINT32_MAX PASS; no hash kernel")

    max_header = header_with_nonce(bytes(base_header), UINT32_MAX)
    max_request = make_request(max_header, UINT32_MAX, 1)
    max_completed, max_payload = run_json_process([
        executable,
        "--header-hex", max_request["header_hex"],
        "--nonce-start", str(UINT32_MAX),
        "--nonce-count", "1",
    ], "LIVE CUDA UINT32_MAX coverage")
    require(max_completed.returncode == 0, "UINT32_MAX GPU batch must exit 0")
    validate_response(max_request, max_payload)
    require(max_payload["covered_nonces"] == [UINT32_MAX],
            "GPU must cover UINT32_MAX exactly once")
    print("LIVE CUDA DEVICE: UINT32_MAX singleton coverage PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", required=True)
    parser.add_argument("--coverage-probe", required=True)
    arguments = parser.parse_args()

    test_cpu_oracle_and_candidate_recheck(arguments.oracle)
    test_coverage_contract_mutations()
    test_live_cuda_coverage(arguments.coverage_probe)
    print("CUDA correctness harness contract PASS; no HooHash CUDA parity claim")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ContractError, OSError, subprocess.TimeoutExpired) as error:
        print(f"CUDA correctness harness FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
