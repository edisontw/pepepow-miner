"""Check the bounded native CLI using frozen values and an independent V110 reference.

If linked against a synthetic test hash, this is ONLY a CLI contract test.
Real HooHash validation requires the pinned Core/OpenLibm build separately.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from g0_v110_independent import v110

FIXTURE = Path(__file__).parent / 'vectors/g1-offline-search-fixed.json'


def main():
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != '--synthetic-timeout'):
        raise SystemExit('usage: check_offline_cpu_search.py BINARY [--synthetic-timeout]')
    binary = sys.argv[1]
    synthetic_timeout = len(sys.argv) == 3
    frozen = json.loads(FIXTURE.read_text(encoding='utf-8'))
    original = bytes.fromhex(frozen['source_header_hex'])
    start, count, nonce = (frozen['start_nonce'], frozen['nonce_count'],
                           frozen['expected_nonce'])
    assert len(original) == 80 and count == 8 and nonce == 0xffffffff
    assert frozen['expected_header_hex'] == (original[:76] + nonce.to_bytes(4, 'little')).hex()
    assert frozen['target_be_hex'] == bytes.fromhex(frozen['expected_digest_raw_hex'])[::-1].hex()
    assert len(frozen['candidate_digests_raw_hex']) == count

    # The independent Python computation must match each PRE-FROZEN digest.
    # Do not generate the expected result dynamically from the CLI under test.
    for offset, expected in enumerate(frozen['candidate_digests_raw_hex']):
        header = original[:76] + (start + offset).to_bytes(4, 'little')
        actual = v110(header).hex()
        assert actual == expected, (start + offset, actual, expected)
    hashes = [int.from_bytes(bytes.fromhex(d), 'little')
              for d in frozen['candidate_digests_raw_hex']]
    target = int(frozen['target_be_hex'], 16)
    assert target == hashes[-1] and all(other > target for other in hashes[:-1])

    found = (f'status=found nonce={nonce} digest_raw={frozen["expected_digest_raw_hex"]}\n'
             f'header_hex={frozen["expected_header_hex"]}\n')
    base = [binary, frozen['source_header_hex']]
    tests = 0

    def check(target_number, beginning, quantity, code, stdout, *flags, delay=None):
        nonlocal tests
        env = os.environ.copy()
        if delay is not None:
            env['PEPEPOW_TEST_HASH_DELAY_MS'] = str(delay)
        args = base + [f'{target_number:064x}', str(beginning), str(quantity), *flags]
        result = subprocess.run(args, text=True, capture_output=True, timeout=5, env=env)
        if result.returncode != code or result.stdout != stdout or result.stderr:
            raise AssertionError((args, result.returncode, result.stdout, result.stderr))
        tests += 1

    usage = ('Usage: pepepow-offline-search HEADER_HEX TARGET_BE_HEX START COUNT '
             '[--replace-job] [--max-ms N]\n'
             'Offline only. COUNT=1..10000; --max-ms=1..60000 (default 30000); exit 4=timeout.\n')
    check(target, start, count, 0, found)
    check(target + 1, start, count, 0, found, '--max-ms', '60000')
    check(target - 1, start, count, 1, 'status=exhausted\n')
    check(target, start, count - 1, 1, 'status=exhausted\n')
    check(target, start, count, 3, 'status=stale\n', '--replace-job')
    check(target, start, count, 3, 'status=stale\n', '--max-ms', '30000', '--replace-job')
    check(target, start, count + 1, 2, usage)  # overflows uint32 nonce
    check(target, start, 0, 2, usage)
    check(target, start, 10001, 2, usage)
    check(target, start, count, 2, usage, '--max-ms', '0')
    check(target, start, count, 2, usage, '--max-ms', '60001')
    check(target, start, count, 2, usage, '--max-ms')
    check(target, start, count, 2, usage, '--max-ms', '1', '--max-ms', '1')
    check(target, start, count, 2, usage, '--replace-job', '--replace-job')
    check(target, start, count, 2, usage, '--unexpected')
    if synthetic_timeout:
        # Deterministic timeout injection is available only in the local mock.
        # The pinned Core must never be assumed to respond to this environment variable.
        check(target, start, count, 4, 'status=timeout\n', '--max-ms', '1', delay=25)
    print(f'PASS: {tests} CLI cases; {count} independently checked frozen full digests, '
          f'nonce {nonce}; verify the actual linked reference separately')


if __name__ == '__main__':
    main()
