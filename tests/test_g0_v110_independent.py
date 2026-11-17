"""Offline checks for the independent, non-Core PEPEPOW V110 reference."""
from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path
import sys
import unittest

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'tools'))
from g0_v110_independent import blake3_small, v110  # noqa: E402

VECTORS = REPO / 'tests/vectors/candidates/explorer-chain-4371217-4371226.jsonl'

class IndependentV110Test(unittest.TestCase):
    def test_official_unkeyed_blake3_vectors(self):
        # https://github.com/BLAKE3-team/BLAKE3/blob/master/test_vectors/test_vectors.json
        known = {
            0: 'af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262',
            1: '2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213',
            2: '7b7015bb92cf0b318037702a6cdd81dee41224f734684c2c122cd6359cb1ee63',
            63: 'e9bc37a594daad83be9470df7f7b3798297c3d834ce80ba85d6e207627b7db7b',
            64: '4eed7141ea4a5cd4b788606bd23f46e212af9cacebacdc7d1f4c6dc7f2511b98',
            65: 'de1e5fa0be70df6d2be8fffd0e99ceaa8eb6e8c93a63f2d8d1c30ecb6b263dee',
            1024: '42214739f095a406f3fc83deb889744ac00df831c10daa55189b5d121c855af7',
        }
        for n, expected in known.items():
            with self.subTest(length=n):
                self.assertEqual(blake3_small(bytes(i % 251 for i in range(n))).hex(), expected)

    def test_blake3_rejects_more_than_one_chunk(self):
        with self.assertRaises(ValueError):
            blake3_small(b'\0' * 1025)

    def test_v110_requires_80_bytes(self):
        for n in (0, 79, 81):
            with self.subTest(length=n), self.assertRaises(ValueError):
                v110(bytes(n))

    def test_chain_digest_full_32_bytes_and_last_byte_mutation(self):
        records = [json.loads(line) for line in VECTORS.read_text(encoding='utf-8').splitlines() if line.strip()]
        self.assertEqual(len(records), 10)
        for r in records:
            with self.subTest(id=r['case_id']):
                got = v110(bytes.fromhex(r['header_hex']))
                expected = bytes.fromhex(r['expected_core_function_output_hex'])
                self.assertEqual(len(got), 32)
                self.assertEqual(got, expected)
                wrong = bytearray(expected)
                wrong[-1] ^= 1
                self.assertNotEqual(got, bytes(wrong))
                self.assertEqual(got[::-1].hex(), r['display_hash'])

    def test_cli_exit_status_valid_and_poisoned_final_byte(self):
        script = REPO / 'tools/g0_v110_independent.py'
        success = subprocess.run([sys.executable, str(script), str(VECTORS)],
                                 capture_output=True, text=True, check=False)
        self.assertEqual(success.returncode, 0, success.stdout + success.stderr)
        self.assertIn('matched: 10 mismatched: 0', success.stdout)
        first = json.loads(VECTORS.read_text(encoding='utf-8').splitlines()[0])
        wrong = bytearray.fromhex(first['expected_core_function_output_hex'])
        wrong[-1] ^= 1
        first['expected_core_function_output_hex'] = wrong.hex()
        with tempfile.TemporaryDirectory() as td:
            poisoned = Path(td) / 'poison.jsonl'
            poisoned.write_text(json.dumps(first) + '\n', encoding='utf-8')
            failure = subprocess.run([sys.executable, str(script), str(poisoned)],
                                     capture_output=True, text=True, check=False)
        self.assertEqual(failure.returncode, 1, failure.stdout + failure.stderr)
        self.assertIn('MISMATCH', failure.stdout)

if __name__ == '__main__':
    unittest.main()
