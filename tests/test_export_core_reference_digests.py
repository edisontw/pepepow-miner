#!/usr/bin/env python3
"""Offline contract tests for the Core digest exporter; no real hashing or pool access."""
import contextlib
import io
import os
from pathlib import Path
import stat
import tempfile
import unittest

from export_core_reference_digests import export

HEADER = "00" * 80
DIGEST = "ab" * 32


class ExportCoreReferenceDigestsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.corpus = self.root / "corpus.tsv"
        self.output = self.root / "digests.tsv"
        self.kat = self.root / "fake-kat"
        self.corpus.write_text(f"g0-synthetic-00000\t{HEADER}\n", encoding="ascii")

    def fake_kat(self, mode="valid"):
        script = """#!/usr/bin/env python3
import sys
mode = %r
if mode == "missing":
    print("MISMATCH")
    sys.exit(1)
if mode == "bad-exit":
    print("actual=" + "ab" * 32)
    print("MISMATCH")
    sys.exit(0)
print("actual=" + "ab" * 32)
print("MISMATCH")
sys.exit(1)
""" % mode
        self.kat.write_text(script, encoding="ascii")
        self.kat.chmod(self.kat.stat().st_mode | stat.S_IXUSR)

    def test_exports_full_digest(self):
        self.fake_kat()
        with contextlib.redirect_stdout(io.StringIO()) as stdout:
            export(str(self.kat), str(self.corpus), str(self.output), 1)
        self.assertEqual(self.output.read_text(encoding="ascii"),
                         f"g0-synthetic-00000\t{HEADER}\t{DIGEST}\n")
        self.assertIn("Not independent parity evidence", stdout.getvalue())

    def test_rejects_missing_digest(self):
        self.fake_kat("missing")
        with self.assertRaisesRegex(RuntimeError, "missing/ambiguous digest"):
            export(str(self.kat), str(self.corpus), str(self.output), 1)

    def test_rejects_inconsistent_exit_status(self):
        self.fake_kat("bad-exit")
        with self.assertRaisesRegex(RuntimeError, "reference result inconsistent"):
            export(str(self.kat), str(self.corpus), str(self.output), 1)

    def test_rejects_short_header(self):
        self.fake_kat()
        self.corpus.write_text("g0-synthetic-00000\t00\n", encoding="ascii")
        with self.assertRaisesRegex(ValueError, "invalid 80-byte header"):
            export(str(self.kat), str(self.corpus), str(self.output), 1)

    def test_rejects_short_corpus(self):
        self.fake_kat()
        with self.assertRaisesRegex(ValueError, "expected 2 rows"):
            export(str(self.kat), str(self.corpus), str(self.output), 2)


if __name__ == "__main__":
    unittest.main()
