#!/usr/bin/env python3
"""Offline full-digest KAT corpus runner. Never generates expected digests.

JSONL input: one object per line with case_id, header_hex and
expected_core_function_output_hex. These are *function bytes*, not RPC hex.
Only use independently verified, pinned Core outputs as acceptance evidence.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys

HEX80 = re.compile(r"[0-9a-fA-F]{160}\Z")
HEX32 = re.compile(r"[0-9a-fA-F]{64}\Z")


def run_corpus(executable, corpus):
    seen = set()
    count = 0
    with open(corpus, encoding="utf-8") as source:
        for line_no, line in enumerate(source, 1):
            if not line.strip():
                continue
            try:
                row = json.loads(line)
                case_id = row["case_id"]
                header = row["header_hex"]
                digest = row["expected_core_function_output_hex"]
                if not isinstance(case_id, str) or not case_id or case_id in seen:
                    raise ValueError("empty or duplicate case_id")
                if not isinstance(header, str) or not HEX80.fullmatch(header):
                    raise ValueError("header_hex must contain exactly 80 bytes")
                if not isinstance(digest, str) or not HEX32.fullmatch(digest):
                    raise ValueError("expected Core function digest must contain 32 bytes")
                seen.add(case_id)
            except (ValueError, TypeError, KeyError, json.JSONDecodeError) as exc:
                print(f"INVALID line {line_no}: {exc}", file=sys.stderr)
                return 2
            try:
                result = subprocess.run(
                    [str(executable), header, digest], capture_output=True,
                    text=True, timeout=60, check=False,
                )
            except (OSError, subprocess.TimeoutExpired) as exc:
                print(f"ERROR {case_id}: {exc}", file=sys.stderr)
                return 2
            if result.returncode != 0:
                print(f"FAIL {case_id} line {line_no}: exit={result.returncode}\n"
                      f"{result.stdout}{result.stderr}", file=sys.stderr)
                return 1
            count += 1
    if count == 0:
        print("INVALID: empty corpus", file=sys.stderr)
        return 2
    print(f"PASS {count} full-digest KAT cases (reference provenance must be audited separately)")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("corpus", type=pathlib.Path)
    args = parser.parse_args()
    return run_corpus(args.executable.resolve(), args.corpus)


if __name__ == "__main__":
    sys.exit(main())
