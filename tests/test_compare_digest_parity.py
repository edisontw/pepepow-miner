"""Offline digest comparison tests; no hashing or pool access."""
from pathlib import Path
import tempfile
import unittest
from compare_digest_parity import compare

HEADER = '00' * 80
DIGEST = 'ab' * 32


class ParityTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.ref = Path(self.tmp.name) / 'ref.tsv'
        self.candidate = Path(self.tmp.name) / 'candidate.tsv'
        self.line = f'g0-synthetic-00000\t{HEADER}\t{DIGEST}\n'
        self.ref.write_text(self.line, encoding='ascii')
        self.candidate.write_text(self.line, encoding='ascii')

    def test_match(self):
        self.assertEqual(compare(self.ref, self.candidate, 1), 1)

    def test_same_file_rejected(self):
        with self.assertRaisesRegex(ValueError, 'same file'):
            compare(self.ref, self.ref, 1)

    def test_hardlink_alias_rejected(self):
        alias = Path(self.tmp.name) / 'alias.tsv'
        try:
            alias.hardlink_to(self.ref)
        except OSError as exc:
            self.skipTest(f'hardlinks unavailable: {exc}')
        with self.assertRaisesRegex(ValueError, 'same file'):
            compare(self.ref, alias, 1)

    def test_symlink_alias_rejected(self):
        alias = Path(self.tmp.name) / 'symlink.tsv'
        try:
            alias.symlink_to(self.ref)
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f'symlinks unavailable: {exc}')
        with self.assertRaisesRegex(ValueError, 'same file'):
            compare(self.ref, alias, 1)

    def test_distinct_files_with_identical_content_are_not_independent_proof(self):
        # The comparator checks bytes, not implementation provenance.
        self.assertNotEqual(self.ref.stat().st_ino, self.candidate.stat().st_ino)
        self.assertEqual(compare(self.ref, self.candidate, 1), 1)

    def test_last_byte_mismatch(self):
        self.candidate.write_text(self.line.replace(DIGEST, DIGEST[:-2] + 'ac'), encoding='ascii')
        with self.assertRaisesRegex(ValueError, 'full 32-byte digest mismatch'):
            compare(self.ref, self.candidate, 1)

    def test_header_mismatch(self):
        self.candidate.write_text(self.line.replace(HEADER, '01' + HEADER[2:]), encoding='ascii')
        with self.assertRaisesRegex(ValueError, 'case/header mismatch'):
            compare(self.ref, self.candidate, 1)

    def test_truncated(self):
        self.candidate.write_text('', encoding='ascii')
        with self.assertRaisesRegex(ValueError, 'missing row'):
            compare(self.ref, self.candidate, 1)

    def test_extra(self):
        self.candidate.write_text(self.line + f'g0-synthetic-00001\t{HEADER}\t{DIGEST}\n', encoding='ascii')
        with self.assertRaisesRegex(ValueError, 'extra rows'):
            compare(self.ref, self.candidate, 1)

    def test_reordered_id(self):
        self.candidate.write_text(self.line.replace('00000', '00001'), encoding='ascii')
        with self.assertRaisesRegex(ValueError, 'unordered'):
            compare(self.ref, self.candidate, 1)

    def test_invalid_hex(self):
        self.candidate.write_text(self.line.replace(DIGEST, 'zz' * 32), encoding='ascii')
        with self.assertRaisesRegex(ValueError, 'malformed'):
            compare(self.ref, self.candidate, 1)

    def test_invalid_count(self):
        with self.assertRaisesRegex(ValueError, 'count'):
            compare(self.ref, self.candidate, 0)


if __name__ == '__main__':
    unittest.main()
