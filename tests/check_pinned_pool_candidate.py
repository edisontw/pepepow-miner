"""Pinned pool candidate KAT: verify the entire 32-byte function-order digest.

The source is pool-pepepow@c1298d7b40e5e9fd39f76af179c6e10a1c313147,
apps/pool-core/tools/verify_known_vector.c. That upstream program only
compares eight bytes and exits zero even on failure. This fixture therefore
remains a PROVISIONAL candidate, not independently established consensus KAT.
"""
import argparse
import subprocess
import sys

HEADER = ("0040002038e31388c54124146478ff691985eecd02610db91efbc9cd7aabca4900000000"
          "07647f0508057dbf8c99ddaa87543c04e31dfe3f383e7386903d50c91728fabe830be"
          "16971e3021da96d9d33")
POOL_DISPLAY = "00000001fb895a82973fca52938848908d6a6cb3c0dfb93995dc61020ced0a6b"


def check(kat):
    if len(HEADER) != 160 or len(POOL_DISPLAY) != 64:
        raise ValueError("invalid pinned fixture lengths")
    raw = bytes.fromhex(POOL_DISPLAY)[::-1].hex()
    positive = subprocess.run([kat, HEADER, raw], capture_output=True, text=True,
                              timeout=120, check=False)
    if (positive.returncode != 0 or
            positive.stdout.splitlines().count("MATCH") != 1 or
            positive.stdout.splitlines().count("actual=" + raw) != 1):
        raise RuntimeError("full 32-byte pinned candidate KAT failed")
    # Mutate the last byte in function order; a prefix-only check must fail.
    replacement = "00" if raw[-2:] != "00" else "01"
    corrupted = raw[:-2] + replacement
    negative = subprocess.run([kat, HEADER, corrupted], capture_output=True,
                              text=True, timeout=120, check=False)
    if (negative.returncode != 1 or
            negative.stderr.splitlines().count("MISMATCH") != 1):
        raise RuntimeError("last-byte negative KAT failed")
    # A changed header must not validate against the original digest. This
    # exercises the real reference's header sensitivity, not just its digest
    # comparator; it is still not independent consensus parity.
    changed_header = ("01" if HEADER[:2] != "01" else "02") + HEADER[2:]
    changed = subprocess.run([kat, changed_header, raw], capture_output=True,
                             text=True, timeout=120, check=False)
    if (changed.returncode != 1 or
            changed.stderr.splitlines().count("MISMATCH") != 1):
        raise RuntimeError("mutated-header negative KAT failed")
    print("PASS: provisional pool candidate, full digest and negative controls")
    print("Provenance remains provisional; not an independent G0 consensus KAT.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kat", required=True)
    args = parser.parse_args()
    try:
        check(args.kat)
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
