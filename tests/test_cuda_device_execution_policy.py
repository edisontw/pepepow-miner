#!/usr/bin/env python3
"""Prove CUDA test skips are limited to explicit no-GPU environments."""

import sys

from cuda_device_policy import should_skip_device_execution


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    for error_code in ("no_device", "driver_unavailable", "initialization_failed"):
        require(should_skip_device_execution(
            error_code, allow_no_device_skip=True, require_device=False),
            f"explicit no-GPU CI must be able to skip {error_code}")
        require(not should_skip_device_execution(
            error_code, allow_no_device_skip=False, require_device=False),
            f"{error_code} must fail when no-GPU skip is not enabled")
        require(not should_skip_device_execution(
            error_code, allow_no_device_skip=True, require_device=True),
            f"required physical-GPU mode must reject {error_code}")

    for error_code in ("cuda_failure", "query_failed", "invalid_arguments", "unknown"):
        require(not should_skip_device_execution(
            error_code, allow_no_device_skip=True, require_device=False),
            f"generic/runtime error {error_code} must never be skipped")

    print("CUDA device execution skip policy PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"CUDA device execution skip policy FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
