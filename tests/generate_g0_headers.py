#!/usr/bin/env python3
"""Deterministic synthetic 80-byte HooHash inputs; NOT golden expected digests."""
import argparse
import hashlib
import sys


def make_header(index: int) -> bytes:
    if not 0 <= index < 2**32:
        raise ValueError("index out of uint32 range")
    # PEPEPOW HooHash version flag 0x4000, little-endian header field.
    version = (0x20004000).to_bytes(4, "little")
    seed = hashlib.sha256(b"PEPEPOW-G0-SYNTHETIC-HEADER-v1" + index.to_bytes(4, "little")).digest()
    prevhash = hashlib.sha256(b"prevhash" + seed).digest()
    merkle = hashlib.sha256(b"merkle" + seed).digest()
    timestamp = (1700000000 + index).to_bytes(4, "little")
    bits = bytes.fromhex("ffff001d")
    # Cover all 256 low-byte nonce values, then explicit boundary cases.
    nonce = (index if index < 256 else (0, 1, 255, 256, 0xffffffff)[index - 256]
             if index < 261 else index).to_bytes(4, "little")
    header = version + prevhash + merkle + timestamp + bits + nonce
    assert len(header) == 80
    return header


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, default=10000)
    args = parser.parse_args(argv)
    if not 1 <= args.count <= 100000:
        parser.error("--count must be between 1 and 100000")
    for index in range(args.count):
        print(f"g0-synthetic-{index:05d}\t{make_header(index).hex()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
