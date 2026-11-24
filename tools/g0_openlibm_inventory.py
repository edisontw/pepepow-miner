"""Inventory archived OpenLibm compile databases against the pinned Git tree.

Usage: python tools/g0_openlibm_inventory.py OPENLIBM_CHECKOUT DEBUG_JSON RELEASE_JSON
Output is JSON on stdout. License signals are triage only, not legal conclusions.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess

PIN = "9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d"
EXPECTED = "b58b2e555e29a5bcc246abd5ed2b8006817a6f215ad282d7fabbdea0314cf03a"
ADDITIONAL_FILES = {
    "LICENSE.md": "aggregate-license-notice",
    "test/test-double.c": "LGPL-test-source-not-in-Windows-compile-databases",
    "test/test-float.c": "LGPL-test-source-not-in-Windows-compile-databases",
}
LICENSE_PATTERNS = (
    ("FDLIBM", re.compile(r"FDLIBM|SUN MICROSYSTEMS", re.I)),
    ("ISC (Moshier notice; see LICENSE.md)", re.compile(r"MOSHIER", re.I)),
    ("BSD-style", re.compile(r"REDISTRIBUTION AND USE IN SOURCE AND BINARY FORMS", re.I)),
    ("Public domain", re.compile(r"PUBLIC DOMAIN", re.I)),
    ("MIT", re.compile(r"SPDX-LICENSE-IDENTIFIER:\s*MIT|MIT LICENSE", re.I)),
    ("LGPL", re.compile(r"LGPL|LESSER GENERAL PUBLIC LICENSE", re.I)),
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def classify_header(raw):
    """Return header-license candidates without treating signals as clearance."""
    header = "\n".join(raw.decode("utf-8", errors="replace").splitlines()[:100])
    candidates = [name for name, pattern in LICENSE_PATTERNS if pattern.search(header)]
    if len(candidates) != 1:
        status = "REVIEW_REQUIRED"
    else:
        status = "EVIDENCE_ONLY"
    return candidates, status


def paths(data):
    rows = json.loads(data)
    if not isinstance(rows, list) or len(rows) != 254:
        raise ValueError("Expected 254 archived compile commands")
    result = set()
    for row in rows:
        name = row["file"].replace("\\", "/")
        # Archived runner paths contain the checkout directory openlibm.
        parts = name.split("/openlibm/")
        if len(parts) != 2:
            raise ValueError("Unexpected checkout path: " + name)
        rel = parts[1]
        if str(PurePosixPath(rel)) != rel or any(
            part in ("", ".", "..") for part in rel.split("/")
        ):
            raise ValueError("Noncanonical source path: " + rel)
        if rel.split("/")[0] not in {"src", "ld80", "bsdsrc", "amd64"}:
            raise ValueError("Unexpected source directory: " + rel)
        if rel in result:
            raise ValueError("Duplicate source: " + rel)
        result.add(rel)
    ordered = sorted(result)
    if sha(("\n".join(ordered) + "\n").encode()) != EXPECTED:
        raise ValueError("Archived path fingerprint differs")
    return ordered


def inventory(root, debug, release):
    def git(*args):
        return subprocess.check_output(["git", "-C", str(root), *args])

    if git("rev-parse", "HEAD").decode().strip() != PIN:
        raise ValueError("OpenLibm checkout is not at the pinned commit")
    a, b = paths(debug), paths(release)
    if a != b:
        raise ValueError("Debug and Release paths differ")
    def describe(path, role):
        spec = PIN + ":" + path
        blob = git("rev-parse", spec).decode().strip()
        raw = git("cat-file", "blob", blob)
        # Heuristic search only; a missing match is not a license determination.
        lines = raw.decode("utf-8", errors="replace").splitlines()[:100]
        signals = [
            {"line": i + 1, "text": line.strip()}
            for i, line in enumerate(lines)
            if re.search(r"copyright|SPDX-License-Identifier|permission|redistribut|license|licence|public domain", line, re.I)
        ]
        candidates, license_status = classify_header(raw)
        return {
            "path": path, "git_blob_sha1": blob, "sha256": sha(raw),
            "url": f"https://github.com/JuliaMath/openlibm/blob/{PIN}/{path}",
            "role": role,
            "header_signals": signals,
            "license_candidates": candidates,
            "license_review_status": license_status,
            "redistribution_review": "pending",
        }

    files = [describe(path, "Windows portable-C library translation unit") for path in a]
    additional = [describe(path, role) for path, role in ADDITIONAL_FILES.items()]
    candidate_counts = {}
    review_required = []
    for item in files:
        for candidate in item["license_candidates"]:
            candidate_counts[candidate] = candidate_counts.get(candidate, 0) + 1
        if item["license_review_status"] == "REVIEW_REQUIRED":
            review_required.append(item["path"])
    return {"commit": PIN, "debug_database_sha256": sha(debug),
            "release_database_sha256": sha(release),
            "translation_unit_count": len(files), "license_candidate_counts": candidate_counts,
            "review_required_translation_units": review_required,
            "files": files, "additional_notice_files": additional,
            "scope": "Pinned upstream Windows portable-C translation units and named license/test notices. Per-file header candidates are triage evidence only, not legal conclusions. Does not include included headers, final linked members, redistribution clearance, or Linux inventories."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkout", type=Path)
    parser.add_argument("debug", type=Path)
    parser.add_argument("release", type=Path)
    args = parser.parse_args()
    try:
        debug, release = args.debug.read_bytes(), args.release.read_bytes()
        if sha(debug) != "f78ec6131bb4d3204c71f2e63f2641f3066a096e9402a1dd8056ee82da8eb375":
            raise ValueError("Debug database checksum mismatch")
        if sha(release) != "cf9e66249af5707d462b6a1b922011591674e4ff539e26c11a92222b5af84e70":
            raise ValueError("Release database checksum mismatch")
        print(json.dumps(inventory(args.checkout, debug, release), indent=2))
    except (ValueError, KeyError, TypeError, OSError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"Inventory failed: {exc}\n")


if __name__ == "__main__":
    main()
