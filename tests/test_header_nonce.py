"""Offline PEPEPOW header nonce-layout contract checks (not hash parity)."""
import unittest

HEADER_BYTES = 80
NONCE_OFFSET = 76


def replace_nonce(header: bytes, nonce: int) -> bytes:
    """Replace the last four header bytes with an unsigned LE uint32 nonce."""
    if len(header) != HEADER_BYTES:
        raise ValueError("header must be exactly 80 bytes")
    if type(nonce) is not int or not 0 <= nonce <= 0xFFFFFFFF:
        raise ValueError("nonce must be a uint32 integer")
    return header[:NONCE_OFFSET] + nonce.to_bytes(4, "little")


class HeaderNonceTests(unittest.TestCase):
    def setUp(self):
        self.header = bytes(range(80))

    def test_nonce_boundaries_and_endianness(self):
        for nonce in (0, 1, 255, 256, 0xFFFFFFFF):
            with self.subTest(nonce=nonce):
                result = replace_nonce(self.header, nonce)
                self.assertEqual(len(result), 80)
                self.assertEqual(result[:76], self.header[:76])
                self.assertEqual(result[76:80], nonce.to_bytes(4, "little"))
        self.assertEqual(replace_nonce(self.header, 256)[76:80], b"\x00\x01\x00\x00")

    def test_all_low_nonce_bytes(self):
        self.assertEqual({replace_nonce(self.header, n)[76] for n in range(256)}, set(range(256)))

    def test_invalid_inputs(self):
        for header, nonce in ((b"", 0), (bytes(79), 0), (bytes(81), 0), (self.header, -1),
                              (self.header, 0x100000000), (self.header, True), (self.header, 1.0)):
            with self.subTest(length=len(header), nonce=nonce):
                with self.assertRaises(ValueError):
                    replace_nonce(header, nonce)


if __name__ == "__main__":
    unittest.main()
