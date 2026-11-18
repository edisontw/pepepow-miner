#!/usr/bin/env python3
"""Export independent Python V110 *candidate* digests for existing G0 TSV corpus.

This is NOT consensus parity on its own. Compare output against a separately
built, pinned Core reference using compare_digest_parity.py. No pool I/O.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from g0_v110_independent import v110

HEADER = re.compile(r'[0-9a-f]{160}\Z')


def export(corpus: Path, output: Path, count: int) -> str:
    """Validate ordered inputs, compute all 32 bytes, atomically publish TSV."""
    corpus, output = Path(corpus), Path(output)
    if not 1 <= count <= 10000:
        raise ValueError('count must be 1..10000')
    if corpus.resolve() == output.resolve():
        raise ValueError('corpus and output must be different files')
    with corpus.open('r', encoding='ascii') as stream:
        lines = stream.readlines()
    if len(lines) != count:
        raise ValueError(f'expected {count} rows, got {len(lines)}')
    cases = []
    for index, row in enumerate(lines):
        fields = row.rstrip('\n').split('\t')
        expected_id = f'g0-synthetic-{index:05d}'
        if len(fields) != 2 or fields[0] != expected_id:
            raise ValueError(f'invalid case id/columns at row {index + 1}')
        if HEADER.fullmatch(fields[1]) is None:
            raise ValueError(f'invalid 80-byte header at row {index + 1}')
        cases.append((expected_id, fields[1]))
    checksum = hashlib.sha256()
    # Only replace final output once *all* independent digests have completed.
    with tempfile.TemporaryDirectory(prefix='.v110-candidate-', dir=output.parent) as tmp:
        staged = Path(tmp) / 'candidate.tsv'
        with staged.open('w', encoding='ascii', newline='\n') as f:
            for case_id, header_hex in cases:
                digest = v110(bytes.fromhex(header_hex)).hex()
                line = f'{case_id}\t{header_hex}\t{digest}\n'
                f.write(line)
                checksum.update(line.encode('ascii'))
        os.replace(staged, output)
    return checksum.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('corpus', type=Path, help='TSV from generate_g0_headers.py')
    parser.add_argument('output', type=Path, help='independent candidate digest TSV')
    parser.add_argument('--count', type=int, default=10000)
    args = parser.parse_args()
    try:
        digest = export(args.corpus, args.output, args.count)
    except (OSError, UnicodeError, ValueError, OverflowError) as exc:
        print(f'FAIL: {exc}', file=sys.stderr)
        return 1
    print(f'EXPORTED {args.count} independent Python candidate digests SHA256={digest}')
    print('NOT G0 parity; compare with separately built pinned Core reference')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
