"""Offline regression checks for the G0 inventory parser and checked-in artifact.

Run: python tools/test_g0_openlibm_inventory.py
Fixture tests do not re-fetch artifacts or establish license clearance.
"""
import hashlib
import json
from pathlib import Path
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

    def test_license_candidates_require_review_when_unknown_or_mixed(self):
        candidates, status = inv.classify_header(
            b"Copyright (C) 1993 Sun Microsystems, Inc.\n"
            b"Redistribution and use in source and binary forms are permitted.\n"
        )
        self.assertEqual(candidates, ["FDLIBM", "BSD-style"])
        self.assertEqual(status, "REVIEW_REQUIRED")

        candidates, status = inv.classify_header(
            b"Copyright (C) 2013 Elliot Saba\nPermission to use, copy, modify.\n"
        )
        self.assertEqual(candidates, [])
        self.assertEqual(status, "REVIEW_REQUIRED")

        candidates, status = inv.classify_header(
            b"Copyright (c) 2008 Stephen L. Moshier\n"
        )
        self.assertEqual(candidates, ["ISC (Moshier notice; see LICENSE.md)"])
        self.assertEqual(status, "EVIDENCE_ONLY")

    def test_checked_in_windows_inventory_is_complete(self):
        path = (Path(__file__).resolve().parent.parent / "docs" / "sources" /
                "openlibm-windows-compile-inventory.json")
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(data["commit"], inv.PIN)
        self.assertEqual(data["translation_unit_count"], 254)
        self.assertEqual(len(data["files"]), 254)
        self.assertEqual(len({row["path"] for row in data["files"]}), 254)
        self.assertEqual(data["debug_database_sha256"],
                         "f78ec6131bb4d3204c71f2e63f2641f3066a096e9402a1dd8056ee82da8eb375")
        self.assertEqual(data["release_database_sha256"],
                         "cf9e66249af5707d462b6a1b922011591674e4ff539e26c11a92222b5af84e70")
        self.assertEqual(len(data["review_required_translation_units"]), 17)
        for row in data["files"]:
            self.assertEqual(len(row["git_blob_sha1"]), 40)
            self.assertEqual(len(row["sha256"]), 64)
            self.assertTrue(row["url"].startswith("https://github.com/JuliaMath/openlibm/blob/"))


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
