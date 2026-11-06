#!/usr/bin/env python3
"""Compare complete 32-byte digests from separately produced TSV files.

Matching output alone does NOT establish independent implementation provenance.
"""
import argparse
import re
import sys

ROW = re.compile(r'(g0-synthetic-[0-9]{5})\t([0-9a-f]{160})\t([0-9a-f]{64})\Z')


def rows(path):
    with open(path, encoding='ascii', newline='') as stream:
        for index, line in enumerate(stream):
            match = ROW.fullmatch(line.rstrip('\n'))
            if match is None or match.group(1) != f'g0-synthetic-{index:05d}':
                raise ValueError(f'{path}: malformed or unordered row {index + 1}')
            yield match.groups()


def compare(reference, candidate, count):
    if not 1 <= count <= 10000:
        raise ValueError('count must be 1..10000')
    a, b = rows(reference), rows(candidate)
    for index in range(count):
        left = next(a, None)
        right = next(b, None)
        if left is None or right is None:
            raise ValueError(f'missing row {index + 1}')
        if left[:2] != right[:2]:
            raise ValueError(f'case/header mismatch at row {index + 1}')
        if left[2] != right[2]:
            raise ValueError(f'full 32-byte digest mismatch at row {index + 1}')
    if next(a, None) is not None or next(b, None) is not None:
        raise ValueError('extra rows beyond requested count')
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference')
    parser.add_argument('candidate')
    parser.add_argument('--count', type=int, required=True)
    args = parser.parse_args()
    try:
        count = compare(args.reference, args.candidate, args.count)
    except (OSError, UnicodeError, ValueError) as exc:
        print(f'FAIL: {exc}', file=sys.stderr)
        return 1
    print(f'PASS: {count} full 32-byte digest comparisons (provenance not verified)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
