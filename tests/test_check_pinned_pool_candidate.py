"""Offline regression tests for the provisional pool candidate KAT runner.

No Core binary or network connection is required. These tests check that the
runner rejects false success reports and verifies the last digest byte.
"""
import subprocess
import unittest
from unittest.mock import patch

import check_pinned_pool_candidate as candidate


class PinnedCandidateTests(unittest.TestCase):
    def fake_run(self, *, positive_stdout=None, positive_rc=0,
                 negative_stderr="MISMATCH\n", negative_rc=1):
        raw = bytes.fromhex(candidate.POOL_DISPLAY)[::-1].hex()
        positive = subprocess.CompletedProcess(
            ["kat"], positive_rc,
            positive_stdout if positive_stdout is not None else f"actual={raw}\nMATCH\n", "")
        negative = subprocess.CompletedProcess(["kat"], negative_rc, "", negative_stderr)
        return raw, [positive, negative]

    def test_positive_and_last_byte_negative(self):
        raw, responses = self.fake_run()
        with patch.object(candidate.subprocess, "run", side_effect=responses) as run:
            candidate.check("fake-kat")
        self.assertEqual(run.call_count, 2)
        first, second = (call.args[0] for call in run.call_args_list)
        self.assertEqual(first, ["fake-kat", candidate.HEADER, raw])
        self.assertEqual(second[:2], ["fake-kat", candidate.HEADER])
        self.assertEqual(second[2][:-2], raw[:-2])
        self.assertNotEqual(second[2][-2:], raw[-2:])

    def test_positive_must_report_full_actual(self):
        raw, responses = self.fake_run(positive_stdout="MATCH\n")
        with patch.object(candidate.subprocess, "run", side_effect=responses):
            with self.assertRaisesRegex(RuntimeError, "full 32-byte"):
                candidate.check("fake-kat")

    def test_positive_nonzero_is_failure(self):
        _, responses = self.fake_run(positive_rc=1)
        with patch.object(candidate.subprocess, "run", side_effect=responses):
            with self.assertRaises(RuntimeError):
                candidate.check("fake-kat")

    def test_negative_zero_exit_is_failure(self):
        _, responses = self.fake_run(negative_rc=0)
        with patch.object(candidate.subprocess, "run", side_effect=responses):
            with self.assertRaisesRegex(RuntimeError, "last-byte"):
                candidate.check("fake-kat")

    def test_negative_requires_mismatch_marker(self):
        _, responses = self.fake_run(negative_stderr="")
        with patch.object(candidate.subprocess, "run", side_effect=responses):
            with self.assertRaisesRegex(RuntimeError, "last-byte"):
                candidate.check("fake-kat")

    def test_positive_prefix_only_is_failure(self):
        raw, responses = self.fake_run()
        responses[0].stdout = f"actual={raw[:16]}\nMATCH\n"
        with patch.object(candidate.subprocess, "run", side_effect=responses):
            with self.assertRaisesRegex(RuntimeError, "full 32-byte"):
                candidate.check("fake-kat")


if __name__ == "__main__":
    unittest.main()
