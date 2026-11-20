"""Offline corpus runner contract tests; synthetic responses are NOT HooHash parity."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import subprocess

SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "run_kat_corpus.py"
spec = importlib.util.spec_from_file_location("run_kat_corpus", SCRIPT)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)

HEADER = bytes(range(80)).hex()
DIGEST = bytes(range(32)).hex()


class CorpusContract(unittest.TestCase):
    def run_rows(self, rows, returncode=0):
        with tempfile.TemporaryDirectory() as directory:
            corpus = Path(directory) / "cases.jsonl"
            corpus.write_text("\n".join(json.dumps(r) for r in rows) + "\n", encoding="utf-8")
            with patch.object(runner.subprocess, "run", return_value=subprocess.CompletedProcess([], returncode, "", "")) as call:
                with contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
                    status = runner.run_corpus("synthetic-only", corpus)
                return status, call.call_args_list

    def row(self, case_id="one", **changes):
        return dict(case_id=case_id, header_hex=HEADER,
                    expected_core_function_output_hex=DIGEST, **changes)

    def test_valid_full_digest_and_argument_order(self):
        status, calls = self.run_rows([self.row(), self.row("two")])
        self.assertEqual(status, 0)
        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[0].args[0], ["synthetic-only", HEADER, DIGEST])

    def test_last_digest_byte_is_not_ignored(self):
        altered = DIGEST[:-2] + "ff"
        row = self.row()
        row["expected_core_function_output_hex"] = altered
        status, calls = self.run_rows([row], returncode=1)
        self.assertEqual(status, 1)
        self.assertEqual(calls[0].args[0][2], altered)

    def test_nonzero_reference_error_returns_failure(self):
        status, _ = self.run_rows([self.row()], returncode=2)
        self.assertEqual(status, 1)

    def test_mismatch_returns_failure(self):
        status, _ = self.run_rows([self.row()], returncode=1)
        self.assertEqual(status, 1)

    def test_duplicate_case_id_rejected(self):
        status, calls = self.run_rows([self.row(), self.row()])
        self.assertEqual(status, 2)
        self.assertEqual(len(calls), 1)

    def test_invalid_header_rejected_without_execution(self):
        row = self.row()
        row["header_hex"] = HEADER[:-2]
        status, calls = self.run_rows([row])
        self.assertEqual((status, len(calls)), (2, 0))

    def test_invalid_digest_rejected_without_execution(self):
        row = self.row()
        row["expected_core_function_output_hex"] = "zz" + DIGEST[2:]
        status, calls = self.run_rows([row])
        self.assertEqual((status, len(calls)), (2, 0))

    def test_empty_corpus_rejected(self):
        status, calls = self.run_rows([])
        self.assertEqual((status, len(calls)), (2, 0))

@unittest.skipIf(__import__("os").name == "nt", "POSIX executable script smoke test")
class CorpusSubprocessSmoke(unittest.TestCase):
    """Real subprocess invocation with a synthetic oracle, not HooHash parity."""

    def test_real_subprocess_success_and_last_byte_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "synthetic_kat.py"
            executable.write_text(
                '#!/usr/bin/env python3\nimport sys\nsys.exit(0 if sys.argv[2] == "'
                + DIGEST + '" else 1)\n', encoding="utf-8"
            )
            executable.chmod(0o755)
            corpus = root / "cases.jsonl"
            row = dict(case_id="synthetic", header_hex=HEADER,
                       expected_core_function_output_hex=DIGEST)
            corpus.write_text(json.dumps(row) + "\n", encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner.run_corpus(executable, corpus), 0)
            row["expected_core_function_output_hex"] = DIGEST[:-2] + "ff"
            corpus.write_text(json.dumps(row) + "\n", encoding="utf-8")
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(runner.run_corpus(executable, corpus), 1)


if __name__ == "__main__":
    unittest.main()
