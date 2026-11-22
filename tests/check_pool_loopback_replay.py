"""Replay raw frames from the pinned pool's loopback fixture."""
import json
import subprocess
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: check_pool_loopback_replay.py REPLAY KAT")
    replay, kat = sys.argv[1:]
    directory = Path(__file__).parent / "vectors/stratum"
    transcript = (directory / "pool-loopback.jsonl").read_bytes()
    expected = json.loads((directory / "pool-loopback-expected.json").read_text())
    _, _, difficulty, notify = map(json.loads, transcript.splitlines())
    assert difficulty["params"][0] / expected["wire_scale"] == expected["effective_difficulty"]
    job, *_, ntime, _clean = notify["params"]
    start = expected["start_nonce"]
    nonce = expected["expected_nonce"]
    args = [replay, "mock.worker", "--search", str(start)]
    params = ["mock.worker", job, expected["extranonce2"], ntime, f"{nonce:08x}"]
    mock_submit = {"id": 3, "method": "mining.submit", "params": params}

    def run(count, data=transcript, *scale):
        result = subprocess.run(
            args + [str(count), expected["extranonce2"], *scale],
            input=data, capture_output=True, timeout=10,
        )
        frames = list(map(json.loads, result.stdout.splitlines()))
        return result, [frame for frame in frames if frame["method"] == "mining.submit"]

    def check(result, submits, status, expected_submits):
        assert (result.returncode, submits) == (status, expected_submits), result

    count = nonce - start + 1
    check(*run(count - 1), 1, [])
    check(*run(count), 0, [mock_submit])
    check(*run(count, transcript, "1"), 1, [])
    # Explicit default wire scale must match the implicit default.
    check(*run(count, transcript, str(expected["wire_scale"])), 0, [mock_submit])
    unscaled = transcript.replace(b"[98.304]", b"[0.0015]")
    check(*run(count, unscaled, "1"), 0, [mock_submit])
    # Invalid scales must fail before any mock submit can be emitted.
    # In particular, NaN/Infinity must not leak into target conversion.
    for invalid_scale in ("0", "-1", "0.5", "nan", "inf", "-inf", "1junk", "1e309"):
        result, submits = run(count, transcript, invalid_scale)
        assert result.returncode == 2, (invalid_scale, result)
        assert submits == [], (invalid_scale, submits)
        assert result.stdout == b"", (invalid_scale, result.stdout)
        assert b"Invalid wire difficulty scale" in result.stderr, (
            invalid_scale, result.stderr
        )
    result = subprocess.run(
        [kat, expected["header_hex"], expected["digest_raw_hex"]],
        capture_output=True, text=True, timeout=5,
    )
    assert result.returncode == 0, result
    assert int.from_bytes(bytes.fromhex(expected["digest_raw_hex"]), "little") <= int(
        expected["target_be_hex"], 16
    )
    print(f"PASS: captured pool wire difficulty, first nonce {nonce}, and full native KAT")


if __name__ == "__main__":
    main()
