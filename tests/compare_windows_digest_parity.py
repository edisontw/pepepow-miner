#!/usr/bin/env python3
"""Windows parity comparator with a complete first-mismatch reproduction."""
import argparse
import sys

from compare_digest_parity import compare, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("core")
    parser.add_argument("independent")
    parser.add_argument("--count", type=int, required=True)
    args = parser.parse_args()
    try:
        print(f"PASS: {compare(args.core, args.independent, args.count)} full 32-byte comparisons")
        return 0
    except (OSError, UnicodeError, ValueError) as exc:
        try:
            for index, (core, independent) in enumerate(
                zip(rows(args.core), rows(args.independent)), start=1
            ):
                if core[:2] != independent[:2]:
                    break
                if core[2] != independent[2]:
                    print(
                        f"MISMATCH row={index} case={core[0]} header={core[1]} "
                        f"core_digest={core[2]} independent_digest={independent[2]}",
                        file=sys.stderr,
                    )
                    return 1
        except (OSError, UnicodeError, ValueError):
            pass
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
