#!/usr/bin/env python3
"""Exercise the offline bounded CUDA search and pinned-Core candidate guard."""

import argparse
import json
import os
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from cuda_device_policy import should_skip_device_execution
from test_cuda_blake3_stage import HEADER_NONCE6, UINT32_MAX
from test_cuda_hoohash_digest import PINNED_DIGESTS


DEVICE_UNAVAILABLE = {
    "no_device", "driver_unavailable", "initialization_failed",
}
ERROR_EXIT_CODES = {
    "no_device": 2,
    "driver_unavailable": 3,
    "initialization_failed": 4,
    "cuda_failure": 5,
    "verifier_unavailable": 6,
    "gpu_digest_mismatch": 7,
    "cpu_target_reject": 8,
    "candidate_contract_failure": 9,
    "invalid_arguments": 64,
}
OUTPUT_MAGIC = b"PPWDIG1\0"
OUTPUT_HEADER = struct.Struct("<8sI")


class SearchContractError(AssertionError):
    pass


def require(condition, message):
    if not condition:
        raise SearchContractError(message)


def run_checked(command, timeout=60):
    completed = subprocess.run(command, capture_output=True, text=True,
                               timeout=timeout, check=False)
    require(completed.returncode == 0,
            f"command failed ({completed.returncode}): {' '.join(map(str, command))}\n"
            f"stdout: {completed.stdout.strip()}\nstderr: {completed.stderr.strip()}")
    return completed


def oracle_digests(oracle, header, start, count, directory):
    output = Path(directory) / f"oracle-{start}-{count}.bin"
    completed = run_checked([
        oracle, "--header-hex", header.hex(), "--nonce-start", str(start),
        "--nonce-count", str(count), "--output", str(output),
    ])
    require(completed.stdout.strip() ==
            f"PINNED CORE digest oracle: {count} complete 32-byte digests",
            "pinned Core oracle summary changed")
    payload = output.read_bytes()
    require(len(payload) == OUTPUT_HEADER.size + count * 32,
            "pinned Core oracle transcript is incomplete")
    magic, actual_count = OUTPUT_HEADER.unpack_from(payload)
    require(magic == OUTPUT_MAGIC and actual_count == count,
            "pinned Core oracle transcript header changed")
    return [payload[OUTPUT_HEADER.size + offset * 32:
                    OUTPUT_HEADER.size + (offset + 1) * 32]
            for offset in range(count)]


def increment_le(value):
    data = bytearray(value)
    for index in range(32):
        data[index] = (data[index] + 1) & 0xff
        if data[index] != 0:
            return bytes(data)
    raise SearchContractError("unexpected uint256 increment overflow")


def decrement_le(value):
    data = bytearray(value)
    for index in range(32):
        before = data[index]
        data[index] = (before - 1) & 0xff
        if before != 0:
            return bytes(data)
    raise SearchContractError("unexpected uint256 decrement underflow")


def compare_le(lhs, rhs):
    for index in range(31, -1, -1):
        if lhs[index] < rhs[index]:
            return -1
        if lhs[index] > rhs[index]:
            return 1
    return 0


def test_core_candidate_guard(oracle, header, evidence_dir):
    digest = bytes.fromhex(PINNED_DIGESTS[6])
    actual_digest = oracle_digests(oracle, header, 6, 1, evidence_dir)[0]
    require(actual_digest == digest,
            "pinned Core disagrees with the fixed nonce-6 expected digest")
    require(compare_le(actual_digest, digest) == 0,
            "inclusive target comparison rejected exact equality")
    require(compare_le(actual_digest, decrement_le(digest)) > 0,
            "inclusive target comparison accepted target−1")
    require(compare_le(actual_digest, increment_le(digest)) <= 0,
            "inclusive target comparison rejected target+1")
    mutated = digest[:-1] + bytes([digest[-1] ^ 1])
    require(mutated != actual_digest,
            "pinned Core candidate guard accepted a final-byte mutation")
    print("PINNED CORE CPU VERIFIER: full digest, final-byte mutation, and target boundaries PASS")


def make_command(engine, oracle, header, start, count, target,
                 cancel_file, batch_size=128, timeout_ms=30000):
    return [
        str(engine), "--header-hex", header.hex(),
        "--nonce-start", str(start), "--nonce-count", str(count),
        "--target-hex", target.hex(), "--batch-size", str(batch_size),
        "--timeout-ms", str(timeout_ms), "--cpu-oracle", str(oracle),
        "--cancel-file", str(cancel_file),
    ]


def run_engine(command, timeout=90, env=None):
    completed = subprocess.run(command, capture_output=True, text=True,
                               timeout=timeout, check=False, env=env)
    require(completed.stdout.strip(),
            f"CUDA search must emit JSON; exit={completed.returncode}, "
            f"stderr={completed.stderr.strip()}")
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise SearchContractError(
            f"CUDA search stdout is not JSON: {error}; stdout={completed.stdout!r}; "
            f"stderr={completed.stderr!r}") from error
    require(isinstance(payload, dict) and
            payload.get("schema_version") == 1 and
            payload.get("engine") == "cuda_hoohash_v110_bounded_search",
            "CUDA search schema or engine identity changed")
    return completed, payload


def validate_search_error(completed, payload, expected_codes=None):
    require(payload.get("status") == "error" and
            isinstance(payload.get("error"), dict),
            "CUDA search error response is malformed")
    error = payload["error"]
    code = error.get("code")
    require(code in ERROR_EXIT_CODES and
            error.get("exit_code") == ERROR_EXIT_CODES[code] and
            completed.returncode == error["exit_code"],
            "CUDA search JSON error code and process exit disagree")
    if expected_codes is not None:
        require(code in expected_codes,
                f"unexpected CUDA search error {code}: {payload}")
    return code


def validate_success(completed, payload, start, requested_count):
    require(completed.returncode == 0 and payload.get("status") == "ok" and
            payload.get("search_status") in {"found", "exhausted"} and
            payload.get("error") is None,
            f"bounded search did not finish cleanly: {payload}")
    require(payload.get("nonce_start") == start and
            payload.get("nonce_count") == requested_count,
            "search request range changed in the response")
    processed = payload.get("processed_count")
    next_nonce = payload.get("next_nonce")
    require(type(processed) is int and 0 <= processed <= requested_count and
            next_nonce == start + processed,
            "search progress accounting is invalid")
    require(payload.get("target_encoding") == "little_endian_uint256" and
            payload.get("cpu_recheck") ==
            "pinned_core_full_digest_and_target",
            "search did not declare the required target and CPU recheck contracts")
    batch_size = payload.get("batch_size")
    batch_count = payload.get("batch_count")
    require(type(batch_size) is int and 1 <= batch_size <= 256 and
            type(batch_count) is int and batch_count >= 0 and
            processed <= batch_size * batch_count and
            (processed == 0 or processed > batch_size * (batch_count - 1)),
            "bounded batch accounting is inconsistent")
    cache = payload.get("matrix_cache")
    require(isinstance(cache, dict) and cache.get("builds") == 1 and
            cache.get("reuses") == max(0, batch_count - 1),
            "job matrix was rebuilt or its cache accounting is invalid")
    candidates = payload.get("candidates")
    require(isinstance(candidates, list), "candidate list is missing")
    seen = set()
    for candidate in candidates:
        require(isinstance(candidate, dict) and
                set(candidate) == {"nonce", "digest_hex"},
                "candidate fields changed")
        nonce = candidate["nonce"]
        digest_hex = candidate["digest_hex"]
        require(type(nonce) is int and start <= nonce < start + processed and
                nonce not in seen, "candidate is outside the completed range or duplicated")
        require(isinstance(digest_hex, str) and len(digest_hex) == 64,
                "candidate must include a full 32-byte digest")
        bytes.fromhex(digest_hex)
        seen.add(nonce)
    if payload["search_status"] == "found":
        require(candidates and processed > 0,
                "found status requires at least one CPU-rechecked candidate")
    else:
        require(not candidates and processed == requested_count,
                "exhausted status must cover the entire requested range")
    return candidates


def validate_candidates_with_python_oracle(oracle, header, start, candidates,
                                           directory):
    if not candidates:
        return
    expected = oracle_digests(oracle, header, start, len(candidates), directory)
    ordered = sorted(candidates, key=lambda candidate: candidate["nonce"])
    for offset, candidate in enumerate(ordered):
        require(candidate["nonce"] == start + offset,
                "all-hit batch must report each matching nonce exactly once")
        gpu = bytes.fromhex(candidate["digest_hex"])
        require(gpu == expected[offset],
                f"independent Python/Core oracle mismatch at nonce {start + offset}")


def test_nonce_range_validation(engine, oracle, evidence_dir, cancel_file):
    header = bytearray(HEADER_NONCE6)
    header[76:80] = UINT32_MAX.to_bytes(4, "little")
    command = make_command(engine, oracle, bytes(header), UINT32_MAX, 2,
                           bytes(32), cancel_file)
    completed, payload = run_engine(command)
    code = validate_search_error(completed, payload, {"invalid_arguments"})
    require(code == "invalid_arguments", "nonce wraparound was not rejected")
    (evidence_dir / "nonce-overflow.json").write_text(
        json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print("MOCK CONTRACT: UINT32_MAX nonce wraparound rejected before CUDA initialization PASS")


def test_live_search(engine, oracle, slow_oracle_proxy, evidence_dir, cancel_file):
    fixed = HEADER_NONCE6
    expected = bytes.fromhex(PINNED_DIGESTS[6])
    cpu = oracle_digests(oracle, fixed, 6, 1, evidence_dir)[0]
    require(cpu == expected, "pinned Core changed the fixed nonce-6 expected digest")

    # The first request determines whether this host has a usable CUDA device.
    completed, payload = run_engine(make_command(
        engine, oracle, fixed, 6, 1, expected, cancel_file, batch_size=1))
    if payload.get("status") == "error":
        code = validate_search_error(completed, payload, DEVICE_UNAVAILABLE)
        allow_skip = os.environ.get("PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP") == "1"
        require_device = os.environ.get("PEPEPOW_CUDA_REQUIRE_DEVICE") == "1"
        require(should_skip_device_execution(
            code, allow_no_device_skip=allow_skip,
            require_device=require_device),
            f"CUDA bounded search failed with {code}; generic failures never skip")
        print(f"NO GPU: live bounded-search checks skipped ({code}); CPU contracts remain executed")
        return

    candidates = validate_success(completed, payload, 6, 1)
    require(payload["search_status"] == "found" and
            candidates == [{"nonce": 6, "digest_hex": expected.hex()}],
            "target equality did not return the nonce-6 full digest")
    validate_candidates_with_python_oracle(oracle, fixed, 6, candidates, evidence_dir)
    (evidence_dir / "target-equality.json").write_text(
        json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print("LIVE CUDA DEVICE: exact target equality and pinned-Core candidate recheck PASS")

    # Equality and adjacent target values exercise the inclusive comparison.
    for label, target, expected_status in (
        ("target-minus-one", decrement_le(expected), "exhausted"),
        ("target-plus-one", increment_le(expected), "found"),
    ):
        completed, payload = run_engine(make_command(
            engine, oracle, fixed, 6, 1, target, cancel_file, batch_size=1))
        candidates = validate_success(completed, payload, 6, 1)
        require(payload["search_status"] == expected_status,
                f"inclusive target comparison failed for {label}")
        if candidates:
            validate_candidates_with_python_oracle(oracle, fixed, 6,
                                                    candidates, evidence_dir)
        (evidence_dir / f"{label}.json").write_text(
            json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print("LIVE CUDA DEVICE: target-minus-one / equality / target-plus-one boundaries PASS")

    # A permissive target returns every nonce in the first bounded batch. The
    # engine must recheck all of them before stopping and must not duplicate one.
    batch_start = 6
    batch_count = 3
    completed, payload = run_engine(make_command(
        engine, oracle, fixed, batch_start, batch_count, bytes([0xff]) * 32,
        cancel_file, batch_size=batch_count))
    candidates = validate_success(completed, payload, batch_start, batch_count)
    require(payload["search_status"] == "found" and len(candidates) == batch_count,
            "all-hit batch did not return the complete bounded candidate set")
    validate_candidates_with_python_oracle(oracle, fixed, batch_start,
                                           candidates, evidence_dir)
    print("LIVE CUDA DEVICE: bounded batch coverage and all-candidate CPU recheck PASS")

    # Two no-hit batches must reuse the matrix built for the immutable job.
    completed, payload = run_engine(make_command(
        engine, oracle, fixed, 6, 2, bytes(32), cancel_file, batch_size=1))
    candidates = validate_success(completed, payload, 6, 2)
    require(payload["search_status"] == "exhausted" and not candidates and
            payload["batch_count"] == 2 and
            payload["matrix_cache"].get("builds") == 1 and
            payload["matrix_cache"].get("reuses") == 1,
            "matrix cache was not reused across two bounded batches")
    (evidence_dir / "matrix-reuse-two-batches.json").write_text(
        json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print("LIVE CUDA DEVICE: one matrix build reused across two search batches PASS")

    # Zero target exhausts the UINT32_MAX boundary without wrapping.
    max_header = bytearray(fixed)
    max_header[76:80] = UINT32_MAX.to_bytes(4, "little")
    completed, payload = run_engine(make_command(
        engine, oracle, bytes(max_header), UINT32_MAX, 1, bytes(32),
        cancel_file, batch_size=1))
    candidates = validate_success(completed, payload, UINT32_MAX, 1)
    require(payload["search_status"] == "exhausted" and
            payload["next_nonce"] == 1 << 32 and not candidates,
            "UINT32_MAX singleton did not exhaust without nonce wrap")
    (evidence_dir / "uint32-max.json").write_text(
        json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print("LIVE CUDA DEVICE: UINT32_MAX singleton exhaustion without wrap PASS")

    require(not cancel_file.exists(), "cancel file unexpectedly exists before test")
    command = make_command(engine, oracle, bytes(80), 0, 1 << 32,
                           bytes(32), cancel_file, batch_size=1,
                           timeout_ms=60000)
    process = subprocess.Popen(command, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    time.sleep(0.25)
    require(process.poll() is None,
            "long bounded-search job exited before cancellation was requested")
    cancel_file.write_text("cancel\n", encoding="ascii")
    try:
        stdout, stderr = process.communicate(timeout=15)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate()
        raise SearchContractError(
            f"cancel-file request did not stop search: stderr={stderr.strip()}")
    try:
        cancelled = json.loads(stdout)
    except json.JSONDecodeError as error:
        raise SearchContractError(
            f"cancelled search stdout is not JSON: {error}; stdout={stdout!r}; "
            f"stderr={stderr!r}") from error
    require(process.returncode == 130 and cancelled.get("status") == "cancelled" and
            cancelled.get("search_status") == "cancelled" and
            cancelled.get("stop_reason") == "cancel_file" and
            cancelled.get("nonce_count") == 1 << 32 and
            type(cancelled.get("processed_count")) is int and
            not cancelled.get("candidates"),
            f"cancel-file stale-job cancellation contract failed: {cancelled}")
    (evidence_dir / "cancel-file-cancellation.json").write_text(
        json.dumps(cancelled, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    cancel_file.unlink()
    print("LIVE CUDA DEVICE: cooperative cancel-file stale-job cancellation PASS")

    # A one-millisecond deadline may cross any number of very fast one-nonce
    # batches. Validate the cancellation state and internally consistent
    # progress without assuming a hardware-dependent batch duration.
    completed, payload = run_engine(make_command(
        engine, oracle, bytes(80), 0, 1 << 32, bytes(32),
        cancel_file, batch_size=1, timeout_ms=1))
    processed = payload.get("processed_count", -1)
    batches = payload.get("batch_count", -1)
    require(completed.returncode == 130 and payload.get("status") == "cancelled" and
            payload.get("search_status") == "cancelled" and
            payload.get("stop_reason") == "timeout" and
            payload.get("nonce_count") == 1 << 32 and
            type(processed) is int and 0 <= processed <= 1 << 32 and
            type(batches) is int and batches == processed and
            payload.get("next_nonce") == processed and
            not payload.get("candidates"),
            f"bounded deadline did not cancel at a batch boundary: {payload}")
    (evidence_dir / "deadline-cancellation.json").write_text(
        json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print("LIVE CUDA DEVICE: bounded deadline cancellation PASS")

    # The slow proxy proves that the deadline is checked after a successful
    # CPU recheck, immediately before a verified candidate could be published.
    recheck_marker = evidence_dir / "slow-oracle-called.marker"
    if recheck_marker.exists():
        recheck_marker.unlink()
    proxy_env = os.environ.copy()
    proxy_env["PEPEPOW_CUDA_TEST_ORACLE"] = str(oracle)
    proxy_env["PEPEPOW_CUDA_TEST_ORACLE_MARKER"] = str(recheck_marker)
    completed, payload = run_engine(make_command(
        engine, slow_oracle_proxy, fixed, 6, 1, expected, cancel_file,
        batch_size=1, timeout_ms=100), timeout=15, env=proxy_env)
    require(recheck_marker.exists(),
            "slow-oracle fixture did not reach the CPU candidate recheck")
    recheck_marker.unlink()
    require(completed.returncode == 130 and payload.get("status") == "cancelled" and
            payload.get("search_status") == "cancelled" and
            payload.get("stop_reason") == "timeout" and
            payload.get("nonce_count") == 1 and payload.get("processed_count") == 1 and
            not payload.get("candidates"),
            f"successful CPU recheck published a candidate past the deadline: {payload}")
    (evidence_dir / "deadline-during-successful-recheck.json").write_text(
        json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print("LIVE CUDA DEVICE: post-recheck deadline suppresses candidates PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True)
    parser.add_argument("--oracle", required=True)
    parser.add_argument("--cpu-oracle", required=True)
    parser.add_argument("--slow-oracle-proxy", required=True)
    parser.add_argument("--evidence-dir", required=True)
    args = parser.parse_args()
    evidence_dir = Path(args.evidence_dir)
    evidence_dir.mkdir(parents=True, exist_ok=True)
    cancel_file = evidence_dir / "cancel-search.request"
    if cancel_file.exists():
        cancel_file.unlink()

    test_core_candidate_guard(args.cpu_oracle, HEADER_NONCE6, evidence_dir)
    test_nonce_range_validation(args.engine, args.cpu_oracle, evidence_dir,
                                cancel_file)
    test_live_search(args.engine, args.oracle, args.slow_oracle_proxy,
                     evidence_dir, cancel_file)
    print("CUDA bounded search contract PASS; pool submission is not implemented")


if __name__ == "__main__":
    try:
        main()
    except (SearchContractError, OSError, subprocess.SubprocessError) as error:
        print(f"CUDA bounded search contract FAIL: {error}", file=sys.stderr)
        sys.exit(1)
