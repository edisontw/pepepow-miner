#!/usr/bin/env python3
"""Validate the CUDA probe JSON contract with mocked errors and a live call."""

import argparse
import json
import os
import subprocess
import sys


EXIT_BY_ERROR = {
    "no_device": 2,
    "driver_unavailable": 3,
    "initialization_failed": 4,
    "query_failed": 5,
    "invalid_arguments": 64,
}
VERSION_FIELDS = ("driver_api_supported", "runtime", "toolkit_build")
DEVICE_FIELDS = (
    "index",
    "name",
    "compute_capability",
    "global_memory_bytes",
    "shared_memory_per_block_bytes",
    "shared_memory_per_block_optin_bytes",
    "registers_per_block",
    "registers_per_multiprocessor",
    "multiprocessor_count",
    "warp_size",
    "max_threads_per_block",
    "fp64",
)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def require_int(value, field, minimum=0):
    require(type(value) is int and value >= minimum, f"{field} must be an integer >= {minimum}")


def validate_versions(payload):
    versions = payload["cuda_versions"]
    require(isinstance(versions, dict), "cuda_versions must be an object")
    require(set(versions) == set(VERSION_FIELDS), "cuda_versions fields changed")
    for field in VERSION_FIELDS:
        version = versions[field]
        if version is None:
            continue
        if field == "toolkit_build":
            require(isinstance(version, str) and version, "toolkit_build must be a non-empty version string")
            continue
        require(isinstance(version, dict), f"{field} must be null or a version object")
        require(set(version) == {"encoded", "major", "minor"}, f"{field} version fields changed")
        require_int(version["encoded"], f"{field}.encoded")
        require_int(version["major"], f"{field}.major")
        require_int(version["minor"], f"{field}.minor")
        require(version["encoded"] == version["major"] * 1000 + version["minor"] * 10,
                f"{field} encoding is inconsistent")


def validate_device(device, expected_index):
    require(isinstance(device, dict), "device entry must be an object")
    require(set(device) == set(DEVICE_FIELDS), "device fields changed")
    require_int(device["index"], "device.index")
    require(device["index"] == expected_index, "device indices must be contiguous and zero-based")
    require(isinstance(device["name"], str) and device["name"], "device.name must be non-empty")

    capability = device["compute_capability"]
    require(isinstance(capability, dict), "compute_capability must be an object")
    require(set(capability) == {"major", "minor", "architecture"}, "compute capability fields changed")
    require_int(capability["major"], "compute_capability.major", 1)
    require_int(capability["minor"], "compute_capability.minor")
    require(capability["architecture"] == f"sm_{capability['major'] * 10 + capability['minor']}",
            "compute capability architecture label is inconsistent")

    for field in DEVICE_FIELDS[3:11]:
        require_int(device[field], f"device.{field}", 1 if field in ("global_memory_bytes", "multiprocessor_count", "warp_size", "max_threads_per_block") else 0)

    fp64 = device["fp64"]
    require(isinstance(fp64, dict), "fp64 must be an object")
    require(set(fp64) == {"attribute_available", "supported", "single_to_double_precision_perf_ratio", "limitation"},
            "fp64 fields changed")
    require(type(fp64["attribute_available"]) is bool, "fp64.attribute_available must be boolean")
    require(fp64["supported"] is None or type(fp64["supported"]) is bool,
            "fp64.supported must be boolean or null")
    ratio = fp64["single_to_double_precision_perf_ratio"]
    require(ratio is None or (type(ratio) is int and ratio >= 0), "FP64 throughput ratio must be integer or null")
    require(isinstance(fp64["limitation"], str) and fp64["limitation"], "FP64 limitation note is required")


def decode_process(process, label):
    require(process.stdout.strip(), f"{label} must write JSON to stdout")
    try:
        payload = json.loads(process.stdout)
    except json.JSONDecodeError as error:
        raise AssertionError(f"{label} stdout is not valid JSON: {error}") from error
    require(isinstance(payload, dict), f"{label} JSON root must be an object")
    require(set(payload) == {"schema_version", "status", "cuda_versions", "devices", "error"},
            f"{label} top-level fields changed")
    require(payload["schema_version"] == 1, f"{label} schema_version must remain 1")
    validate_versions(payload)
    require(isinstance(payload["devices"], list), f"{label} devices must be an array")

    if payload["status"] == "ok":
        require(process.returncode == 0, f"{label} success must exit 0")
        require(payload["error"] is None, f"{label} success error must be null")
        require(payload["devices"], f"{label} success must contain at least one device")
        for index, device in enumerate(payload["devices"]):
            validate_device(device, index)
        return payload

    require(payload["status"] == "error", f"{label} status must be ok or error")
    require(payload["devices"] == [], f"{label} errors must return an empty devices array")
    error = payload["error"]
    require(isinstance(error, dict), f"{label} error must be an object")
    require(set(error) == {"code", "cuda_error", "message", "exit_code"}, f"{label} error fields changed")
    require(error["code"] in EXIT_BY_ERROR, f"{label} error code is not recognized")
    expected_exit = EXIT_BY_ERROR[error["code"]]
    require(error["exit_code"] == expected_exit, f"{label} JSON exit code does not match error code")
    require(process.returncode == expected_exit, f"{label} process exit code does not match JSON")
    require(isinstance(error["cuda_error"], str) and error["cuda_error"], f"{label} cuda_error must be a string")
    require(isinstance(error["message"], str) and error["message"], f"{label} message must be a string")
    return payload


def run(executable, label, *arguments):
    process = subprocess.run(
        [executable, *arguments],
        check=False,
        capture_output=True,
        text=True,
        timeout=10,
    )
    payload = decode_process(process, label)
    require(not process.stderr.strip(), f"{label} must keep diagnostics out of stderr")
    return payload


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    parser.add_argument("--fixtures", required=True)
    arguments = parser.parse_args()

    for error_code, exit_code in EXIT_BY_ERROR.items():
        if error_code == "invalid_arguments":
            payload = run(arguments.probe, "LIVE invalid arguments", "--invalid-argument")
            require(payload["error"]["code"] == error_code,
                    "LIVE invalid arguments emitted the wrong error")
            print(f"LIVE: invalid_arguments JSON and exit {exit_code} PASS")
            continue
        payload = run(
            arguments.fixtures,
            f"MOCK {error_code}",
            f"--fixture-error={error_code}",
        )
        require(payload["error"]["code"] == error_code, f"MOCK {error_code} emitted the wrong error")
        require(payload["error"]["exit_code"] == exit_code, f"MOCK {error_code} emitted the wrong exit code")
        print(f"MOCK: {error_code} JSON and exit {exit_code} PASS")

    live = run(arguments.probe, "LIVE CUDA Runtime API probe")
    if os.environ.get("PEPEPOW_CUDA_REQUIRE_DEVICE") == "1":
        require(live["status"] == "ok", "GPU-host mode requires a real CUDA device")
    if live["status"] == "ok":
        print(f"LIVE: {len(live['devices'])} CUDA device(s) reported")
    else:
        print(f"LIVE: {live['error']['code']} reported by the installed runtime/driver")
    print("CUDA device probe JSON contract PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"CUDA device probe contract FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
