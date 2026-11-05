#!/usr/bin/env python3
"""Export Core-reference results; NOT independent golden-vector parity."""
import argparse
import hashlib
import re
import subprocess

def export(kat, corpus, output, limit):
    checksum = hashlib.sha256()
    count = 0
    with open(corpus, encoding="ascii") as source, open(output, "w", encoding="ascii", newline="\n") as dest:
        for row in source:
            if count == limit:
                break
            fields = row.rstrip("\n").split("\t")
            if len(fields) != 2 or fields[0] != f"g0-synthetic-{count:05d}":
                raise ValueError(f"invalid case id/columns at row {count + 1}")
            if len(fields[1]) != 160 or not re.fullmatch(r"[0-9a-f]{160}", fields[1]):
                raise ValueError(f"invalid 80-byte header at row {count + 1}")
            run = subprocess.run([kat, fields[1], "00" * 32],
                                 capture_output=True, text=True, timeout=30, check=False)
            matches = re.findall(r"^actual=([0-9a-f]{64})$", run.stdout, re.MULTILINE)
            if len(matches) != 1:
                raise RuntimeError(f"missing/ambiguous digest: {fields[0]}")
            actual = matches[0]
            expected_code = 0 if actual == "00" * 32 else 1
            expected_marker = "MATCH" if expected_code == 0 else "MISMATCH"
            markers = [line for line in (run.stdout.splitlines() + run.stderr.splitlines())
                       if line in ("MATCH", "MISMATCH")]
            if run.returncode != expected_code or markers != [expected_marker]:
                raise RuntimeError(f"reference result inconsistent: {fields[0]}")
            line = f"{fields[0]}\t{fields[1]}\t{actual}\n"
            dest.write(line)
            checksum.update(line.encode("ascii"))
            count += 1
    if count != limit:
        raise ValueError(f"expected {limit} rows, found {count}")
    print(f"Reference-generated cases: {count}")
    print(f"Reference TSV SHA256: {checksum.hexdigest()}")
    print("Not independent parity evidence; compare against a separate implementation.")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kat", required=True)
    parser.add_argument("--corpus", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--limit", type=int, default=16)
    args = parser.parse_args()
    if not 1 <= args.limit <= 10000:
        parser.error("--limit must be 1..10000")
    export(args.kat, args.corpus, args.output, args.limit)

if __name__ == "__main__":
    main()
