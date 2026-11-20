"""Standalone, independently written Python reference for PEPEPOW HooHash V110.

Offline correctness investigation only. Uses no Core/pool algorithm sources or
native shared libraries; BLAKE3 covers the single-chunk inputs used by V110.
Python's system libm differs from consensus-pinned openlibm, so matching digests
must be established empirically and on each required native platform.
"""
from __future__ import annotations

import argparse
import json
import math
import struct

U32 = (1 << 32) - 1
U64 = (1 << 64) - 1
IV = [0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
      0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19]
PERMUTE = [2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8]
START, END, ROOT = 1, 2, 8


def _right_rotate(x: int, n: int) -> int:
    return ((x >> n) | (x << (32 - n))) & U32


def _mix(v: list[int], a: int, b: int, c: int, d: int, x: int, y: int) -> None:
    v[a] = (v[a] + v[b] + x) & U32
    v[d] = _right_rotate(v[d] ^ v[a], 16)
    v[c] = (v[c] + v[d]) & U32
    v[b] = _right_rotate(v[b] ^ v[c], 12)
    v[a] = (v[a] + v[b] + y) & U32
    v[d] = _right_rotate(v[d] ^ v[a], 8)
    v[c] = (v[c] + v[d]) & U32
    v[b] = _right_rotate(v[b] ^ v[c], 7)


def _compress(cv: list[int], block: bytes, length: int, flags: int) -> list[int]:
    words = list(struct.unpack('<16I', block.ljust(64, b'\0')))
    v = list(cv) + IV[:4] + [0, 0, length, flags]
    for round_no in range(7):
        _mix(v, 0, 4, 8, 12, words[0], words[1])
        _mix(v, 1, 5, 9, 13, words[2], words[3])
        _mix(v, 2, 6, 10, 14, words[4], words[5])
        _mix(v, 3, 7, 11, 15, words[6], words[7])
        _mix(v, 0, 5, 10, 15, words[8], words[9])
        _mix(v, 1, 6, 11, 12, words[10], words[11])
        _mix(v, 2, 7, 8, 13, words[12], words[13])
        _mix(v, 3, 4, 9, 14, words[14], words[15])
        if round_no != 6:
            words = [words[i] for i in PERMUTE]
    return [(v[i] ^ v[i + 8]) & U32 for i in range(8)] + [
        (v[i + 8] ^ cv[i]) & U32 for i in range(8)]


def blake3_small(message: bytes) -> bytes:
    """Unkeyed 32-byte BLAKE3 for 0..1024-byte single-chunk messages."""
    if len(message) > 1024:
        raise ValueError('single BLAKE3 chunk only')
    blocks = [message[i:i + 64] for i in range(0, len(message), 64)] or [b'']
    cv = IV[:]
    for i, block in enumerate(blocks):
        flags = (START if i == 0 else 0) | (END | ROOT if i == len(blocks) - 1 else 0)
        output = _compress(cv, block, len(block), flags)
        cv = output[:8]
    return struct.pack('<8I', *output[:8])


def _rotl64(x: int, n: int) -> int:
    return ((x << n) | (x >> (64 - n))) & U64


def _matrix(seed: bytes) -> list[list[float]]:
    state = list(struct.unpack('<4Q', seed))
    result = []
    for i in range(64):
        row = []
        for j in range(64):
            value = (_rotl64((state[0] + state[3]) & U64, 23) + state[0]) & U64
            t = (state[1] << 17) & U64
            state[2] ^= state[0]
            state[3] ^= state[1]
            state[1] ^= state[2]
            state[0] ^= state[3]
            state[2] ^= t
            state[3] = _rotl64(state[3], 45)
            row.append(float(value & U32) / float(U32) * 1_000_000.0)
        result.append(row)
    return result


def _nonlinear(x: float) -> float:
    xfactor = (x * 0.000001) / 8.0
    yfactor = (x * 0.000001) / 4.0
    first = xfactor - math.floor(xfactor)
    second = yfactor - math.floor(yfactor)
    adjusted = [x + (1 + second), x - (1 + second),
                x * (1 + second), x / (1 + second)][
                    min(int(second * 4), 3)]
    if first < 0.33:
        return math.exp(math.sin(adjusted) + math.cos(adjusted))
    if first < 0.66:
        if abs(adjusted - math.pi / 2) < 1e-9 or abs(adjusted - 3 * math.pi / 2) < 1e-9:
            return 0.0
        return math.sin(adjusted) * math.sin(adjusted)
    return 1.0 / math.sqrt(abs(adjusted) + 1.0)


def v110(header: bytes) -> bytes:
    if len(header) != 80:
        raise ValueError('V110 requires exactly 80-byte header')
    first = blake3_small(header)
    seed = blake3_small(header[:76] + b'\0' * 4)
    mat = _matrix(seed)
    nonce = int.from_bytes(header[76:80], 'little')
    w = struct.unpack('>8I', first)
    xor = w[0] ^ w[1] ^ w[2] ^ w[3] ^ w[4] ^ w[5] ^ w[6] ^ w[7]
    nibs = [n for byte in first for n in (byte >> 4, byte & 0xF)]
    product = [0.0] * 64
    switch = 0.0
    for i in range(64):
        for j in range(64):
            if switch <= 0.02:
                value = mat[i][j] * float(xor) * float(nibs[j]) + float(nonce & 0xFF)
                product[i] += _nonlinear(value) * float(nibs[j]) * 1234.0
            else:
                product[i] += mat[i][j] * 0.0001 * float(nibs[j])
            fraction = product[i] / 1024.0
            switch = fraction - math.floor(fraction)
    intermediate = bytes(first[i] ^ ((int(product[2*i]) + int(product[2*i+1])) & 0xFF)
                         for i in range(32))
    return blake3_small(intermediate)[::-1]


def main() -> int:
    parser = argparse.ArgumentParser(description='Offline independent HooHash V110 prototype')
    parser.add_argument('vector_jsonl')
    args = parser.parse_args()
    ok = 0
    bad = 0
    with open(args.vector_jsonl, encoding='utf-8') as f:
        for line in f:
            if not line.strip():
                continue
            entry = json.loads(line)
            actual = v110(bytes.fromhex(entry['header_hex'])).hex()
            expected = entry['expected_core_function_output_hex']
            good = actual == expected
            print(entry['case_id'], 'PASS' if good else 'MISMATCH', 'actual='+actual, 'expected='+expected)
            ok += good
            bad += not good
    print('matched:',ok,'mismatched:',bad,'(not G0 acceptance)')
    return 0 if ok > 0 and bad == 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
