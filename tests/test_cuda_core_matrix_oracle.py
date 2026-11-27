#!/usr/bin/env python3
"""Validate test-only matrix observations against pinned Core and Python."""

import argparse
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


HEADER_NONCE6 = bytes.fromhex(
    "0040002003020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c"
    "5b5d507845ead90ea4c4caeef4baef7817c0fb2169d20739883e22bb6d8cf8ef0be1697171e3021d06000000"
)
UINT32_MAX = (1 << 32) - 1
INPUT_MAGIC = b"PPWSEED1"
OUTPUT_MAGIC = b"PPWOBS1\0"
MASK64 = (1 << 64) - 1
ELEMENTS = 4096
OBSERVATION_BYTES = 52
CASE_BYTES = 32 + 32 + ELEMENTS * OBSERVATION_BYTES


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def header_with_nonce(header, nonce):
    require(len(header) == 80, "header must be exactly 80 bytes")
    result = bytearray(header)
    result[76:80] = nonce.to_bytes(4, "little")
    return bytes(result)


def core_matrix_seed(oracle, header):
    completed = subprocess.run(
        [oracle, header.hex(), "matrix-seed"],
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    require(completed.returncode == 0,
            f"pinned Core BLAKE3 oracle failed: {completed.stderr.strip()}")
    value = completed.stdout.strip()
    require(len(value) == 64, "Core matrix seed must be a full 32-byte digest")
    try:
        seed = bytes.fromhex(value)
    except ValueError as error:
        raise AssertionError(f"Core matrix seed is not hex: {error}") from error
    require(len(seed) == 32, "Core matrix seed must be exactly 32 bytes")
    return seed


def make_seeds(blake3_oracle):
    nonce6 = header_with_nonce(HEADER_NONCE6, 6)
    nonce_max = header_with_nonce(HEADER_NONCE6, UINT32_MAX)
    seed6 = core_matrix_seed(blake3_oracle, nonce6)
    seed_max = core_matrix_seed(blake3_oracle, nonce_max)
    require(seed6.hex() ==
            "28d0be17bea0de270743b7c09c785157504aecc287f62e2673051bbadf764d6d",
            "pinned Core matrix-seed vector for nonce 6 changed")
    require(seed_max == seed6,
            "one header template with different little-endian nonces must reuse its matrix seed")

    headers = []
    for template_index in range(31):
        header = bytearray(80)
        for byte_index in range(76):
            header[byte_index] = (byte_index * 131 + template_index * 17 + 0x5B) & 0xFF
        header[0] ^= template_index
        header[76:80] = (template_index * 257).to_bytes(4, "little")
        headers.append(bytes(header))

    seeds = [seed6]
    seeds.extend(core_matrix_seed(blake3_oracle, header) for header in headers)
    require(len(seeds) == 32, "matrix test must cover 32 seed inputs")
    require(len(set(seeds)) == len(seeds), "matrix corpus must contain 32 distinct seeds")
    return seeds


def rotl64(value, amount):
    value &= MASK64
    return ((value << amount) | (value >> (64 - amount))) & MASK64


def python_core_matrix_transcript(seed):
    state = list(struct.unpack("<4Q", seed))
    initial_state = tuple(state)
    output = bytearray(seed)
    output.extend(struct.pack("<4Q", *initial_state))

    for _ in range(ELEMENTS):
        s0, s1, s2, s3 = state
        value = (rotl64((s0 + s3) & MASK64, 23) + s0) & MASK64
        transition = (s1 << 17) & MASK64

        s2 ^= s0
        s3 ^= s1
        s1 ^= s2
        s0 ^= s3
        s2 ^= transition
        s3 = rotl64(s3, 45)
        state = [s0 & MASK64, s1 & MASK64, s2 & MASK64, s3 & MASK64]

        lower = value & 0xFFFFFFFF
        matrix_value = float(lower) / float(0xFFFFFFFF) * 1000000.0
        matrix_bits = struct.unpack("<Q", struct.pack("<d", matrix_value))[0]
        output.extend(struct.pack("<Q4QIQ", value, *state, lower, matrix_bits))

    return bytes(output)


def run_oracle(oracle, seed_path, output_path):
    completed = subprocess.run(
        [oracle, "--input", str(seed_path), "--output", str(output_path)],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    require(completed.returncode == 0,
            f"pinned Core matrix oracle failed: {completed.stderr.strip()}")
    return completed.stdout.strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", required=True)
    parser.add_argument("--blake3-oracle", required=True)
    args = parser.parse_args()

    seeds = make_seeds(args.blake3_oracle)
    corpus = INPUT_MAGIC + struct.pack("<I", len(seeds)) + b"".join(seeds)
    expected = bytearray(OUTPUT_MAGIC + struct.pack("<I", len(seeds)))
    for seed in seeds:
        expected.extend(python_core_matrix_transcript(seed))

    with tempfile.TemporaryDirectory(prefix="pepepow-core-matrix-") as directory:
        seed_path = Path(directory) / "matrix-seeds.bin"
        output_path = Path(directory) / "core-observations.bin"
        seed_path.write_bytes(corpus)
        summary = run_oracle(args.oracle, seed_path, output_path)
        actual = output_path.read_bytes()

    expected_length = 12 + len(seeds) * CASE_BYTES
    require(len(actual) == expected_length,
            f"Core oracle output has {len(actual)} bytes; expected {expected_length}")
    require(actual[:8] == OUTPUT_MAGIC, "Core oracle output magic changed")
    require(struct.unpack_from("<I", actual, 8)[0] == len(seeds),
            "Core oracle output seed count changed")
    require(actual == bytes(expected),
            "pinned Core intermediate transcript differs from independent Python model")

    print(summary)
    print("PINNED CORE MATRIX ORACLE CONTRACT PASS: 32 distinct seeds; "
          "131072 PRNG outputs/states and 131072 binary64 matrix bit patterns checked")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"PINNED CORE MATRIX ORACLE CONTRACT FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
