"""Offline Stratum prevhash word-endian regression fixture (no network/mining).

Reference: pool-pepepow apps/pool-core/stratum_protocol.py
format_prevhash_for_stratum(), source snapshot c1298d7b40e5e9fd39f76af179c6e10a1c313147.
This is a standalone protocol fixture, not a miner implementation or G0 hash KAT.
"""
import json
import unittest
from pathlib import Path


def stratum_prevhash(display_hash: str) -> str:
    """Reverse bytes inside each 4-byte word, preserving word order."""
    if len(display_hash) != 64:
        raise ValueError("prevhash must be exactly 32 bytes")
    try:
        raw = bytes.fromhex(display_hash)
    except ValueError as exc:
        raise ValueError("prevhash must be hex") from exc
    return b"".join(raw[i:i + 4][::-1] for i in range(0, 32, 4)).hex()


class PrevhashFixture(unittest.TestCase):
    def test_word_reversal_not_full_reversal(self):
        source = bytes(range(32)).hex()
        expected = "03020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c"
        self.assertEqual(stratum_prevhash(source), expected)
        self.assertNotEqual(stratum_prevhash(source), bytes(range(32))[::-1].hex())

    def test_round_trip(self):
        value = bytes(range(32)).hex()
        self.assertEqual(stratum_prevhash(stratum_prevhash(value)), value)

    def test_invalid(self):
        for value in ("", "00" * 31, "00" * 33, "zz" * 32, "0" * 63 + "g"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    stratum_prevhash(value)

    def test_captured_pool_notify_matches_header(self):
        fixture_dir = Path(__file__).parent / "vectors" / "stratum"
        expected = json.loads((fixture_dir / "pool-loopback-expected.json").read_text())
        frames = (fixture_dir / "pool-loopback.jsonl").read_text().splitlines()
        notify = json.loads(frames[3])
        self.assertEqual(notify["method"], "mining.notify")
        # Header bytes 4..35 encode the raw previous-block hash.
        header = bytes.fromhex(expected["header_hex"])
        self.assertEqual(len(header), 80)
        raw_prevhash = header[4:36]
        self.assertEqual(stratum_prevhash(raw_prevhash.hex()), notify["params"][1])
        self.assertNotEqual(raw_prevhash.hex(), notify["params"][1])


if __name__ == "__main__":
    unittest.main()
