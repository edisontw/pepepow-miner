"""Offline synthetic regression checks for the G0 inventory parser.

Run: python tools/test_g0_openlibm_inventory.py
These fixtures do not establish the archived source inventory or license clearance.
"""
import hashlib
import json
import unittest
from unittest.mock import patch
import g0_openlibm_inventory as inv


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.names = [f"src/f{i:03d}.c" for i in range(254)]
        self.rows = [{"file": "C:/fixture/openlibm/" + n} for n in self.names]
        self.data = json.dumps(self.rows).encode()
        fingerprint = hashlib.sha256(
            ("\n".join(self.names) + "\n").encode()
        ).hexdigest()
        self.addCleanup(patch.stopall)
        patch.object(inv, "EXPECTED", fingerprint).start()

    def test_valid_and_reordered(self):
        self.assertEqual(inv.paths(self.data), self.names)
        self.assertEqual(inv.paths(json.dumps(self.rows[::-1])), self.names)

    def test_duplicate(self):
        self.rows[-1] = self.rows[0]
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            inv.paths(json.dumps(self.rows))

    def test_traversal(self):
        self.rows[0]["file"] = "C:/fixture/openlibm/src/../test.c"
        with self.assertRaisesRegex(ValueError, "Noncanonical"):
            inv.paths(json.dumps(self.rows))

    def test_test_directory(self):
        self.rows[0]["file"] = "C:/fixture/openlibm/test/test-double.c"
        with self.assertRaisesRegex(ValueError, "Unexpected source directory"):
            inv.paths(json.dumps(self.rows))

    def test_fingerprint(self):
        self.rows[0]["file"] = "C:/fixture/openlibm/src/changed.c"
        with self.assertRaisesRegex(ValueError, "fingerprint"):
            inv.paths(json.dumps(self.rows))

    def test_wrong_count(self):
        with self.assertRaisesRegex(ValueError, "254"):
            inv.paths(json.dumps(self.rows[:-1]))

    def test_wrong_commit(self):
        with patch.object(inv.subprocess, "check_output", return_value=b"wrong\n"):
            with self.assertRaisesRegex(ValueError, "pinned commit"):
                inv.inventory(".", self.data, self.data)


    def test_debug_database_checksum_rejected(self):
        self.assert_bad_database("Debug", [b"invalid-debug", b"unused"])

    def test_release_database_checksum_rejected(self):
        self.assert_bad_database("Release", [b"fixture-debug", b"invalid-release"],
                                 first_hash="f78ec6131bb4d3204c71f2e63f2641f3066a096e9402a1dd8056ee82da8eb375")

    def assert_bad_database(self, configuration, contents, first_hash=None):
        from contextlib import redirect_stderr
        from io import StringIO
        stderr = StringIO()
        with patch.object(inv.Path, "read_bytes", side_effect=contents), \
                patch.object(__import__("sys"),
                             "argv", ["inventory", ".", "debug.json", "release.json"]), \
                patch.object(inv, "inventory") as run, redirect_stderr(stderr):
            if first_hash is not None:
                with patch.object(inv, "sha", side_effect=[first_hash, "invalid"]):
                    with self.assertRaises(SystemExit) as error:
                        inv.main()
            else:
                with self.assertRaises(SystemExit) as error:
                    inv.main()
            self.assertEqual(error.exception.code, 1)
            self.assertIn(configuration + " database checksum mismatch", stderr.getvalue())
            run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
