"""Check the native offline search against the independent Python reference."""
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from g0_v110_independent import v110


def main():
    binary = sys.argv[1]
    fixture = Path(__file__).parent / "vectors/candidates/explorer-chain-4371217-4371226.jsonl"
    header = bytes.fromhex(json.loads(fixture.read_text().splitlines()[0])["header_hex"])
    start = 0xFFFFFFF8
    digests = [v110(header[:76] + nonce.to_bytes(4, "little"))
               for nonce in range(start, start + 8)]
    hashes = [int.from_bytes(digest, "little") for digest in digests]
    index = hashes.index(min(hashes))
    if index == 0 or hashes[index] == 0:
        raise AssertionError("Fixture must exercise misses before the first hit")

    def check(target, count, code, output, *flags):
        args = [binary, header.hex(), f"{target:064x}", str(start), str(count), *flags]
        result = subprocess.run(args, text=True, capture_output=True, timeout=10)
        if result.returncode != code or not result.stdout.startswith(output) or result.stderr:
            raise AssertionError((args, result))

    target = hashes[index]
    found = f"status=found nonce={start + index} digest_raw={digests[index].hex()}\n"
    check(target, 8, 0, found)
    check(target + 1, 8, 0, found)
    check(target - 1, 8, 1, "status=exhausted\n")
    check(target, index, 1, "status=exhausted\n")
    check(target, 8, 3, "status=stale\n", "--replace-job")
    check(target, 9, 2, "Usage:")
    check(target, 0, 2, "Usage:")
    check(target, 10001, 2, "Usage:")
    print(f"PASS: 8 cases, first qualifying nonce {start + index}, digest {digests[index].hex()}")


if __name__ == "__main__":
    main()
