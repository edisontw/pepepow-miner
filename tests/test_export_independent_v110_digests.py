"""Focus: real independent V110 candidate digests, input safety, no Core mocks."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'tests'))
from export_independent_v110_digests import export


class IndependentExportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.input = Path(self.temporary.name) / 'input.tsv'
        self.output = Path(self.temporary.name) / 'candidate.tsv'
        rows = [json.loads(s) for s in
                (REPO/'tests/vectors/candidates/explorer-chain-4371217-4371226.jsonl').read_text().splitlines()]
        self.first, self.second = rows[:2]
        self.input.write_text('g0-synthetic-00000\t'+self.first['header_hex']+'\n'
                              'g0-synthetic-00001\t'+self.second['header_hex']+'\n',encoding='ascii')

    def test_exports_two_real_full_digests(self):
        self.assertEqual(len(export(self.input, self.output, 2)), 64)
        lines = [s.split('\t') for s in self.output.read_text().splitlines()]
        self.assertEqual(len(lines), 2)
        self.assertEqual(lines[0][2], self.first['expected_core_function_output_hex'])
        self.assertEqual(lines[1][2], self.second['expected_core_function_output_hex'])

    def test_rejects_invalid_corpus_without_truncating_output(self):
        self.output.write_text('preserve', encoding='ascii')
        self.input.write_text('g0-synthetic-00000\t00\n', encoding='ascii')
        with self.assertRaisesRegex(ValueError, 'invalid 80-byte header'):
            export(self.input, self.output, 1)
        self.assertEqual(self.output.read_text(), 'preserve')

    def test_rejects_missing_rows_and_same_file(self):
        with self.assertRaisesRegex(ValueError, 'expected 3 rows'):
            export(self.input, self.output, 3)
        with self.assertRaisesRegex(ValueError, 'corpus and output must be different'):
            export(self.input, self.input, 2)

    def test_rejects_duplicate_or_out_of_order_case(self):
        content = self.input.read_text()
        self.input.write_text(content.replace('g0-synthetic-00001','g0-synthetic-00000'))
        with self.assertRaisesRegex(ValueError,'invalid case id/columns at row 2'):
            export(self.input,self.output,2)


if __name__ == '__main__':
    unittest.main()
