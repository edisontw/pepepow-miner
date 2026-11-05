import unittest
from generate_g0_headers import make_header


class SyntheticHeadersTest(unittest.TestCase):
    def test_layout_and_nonce_coverage(self):
        headers = [make_header(i) for i in range(10000)]
        self.assertTrue(all(len(h) == 80 for h in headers))
        self.assertTrue(all(h[:4] == bytes.fromhex("00400020") for h in headers))
        self.assertEqual({h[76] for h in headers[:256]}, set(range(256)))
        self.assertEqual([int.from_bytes(h[76:80], "little") for h in headers[256:261]], [0, 1, 255, 256, 0xffffffff])
        self.assertEqual(len(set(headers)), len(headers))

    def test_reproducibility_and_bounds(self):
        self.assertEqual(make_header(0), make_header(0))
        with self.assertRaises(ValueError):
            make_header(-1)
        with self.assertRaises(ValueError):
            make_header(2**32)
        # The generator accepts the full uint32 index domain; timestamp wraps
        # modulo 2**32 rather than overflowing Python int.to_bytes().
        boundary = 2**32 - 1
        header = make_header(boundary)
        self.assertEqual(len(header), 80)
        self.assertEqual(int.from_bytes(header[68:72], "little"), (1700000000 + boundary) & 0xffffffff)
        self.assertEqual(int.from_bytes(header[76:80], "little"), boundary)


if __name__ == "__main__":
    unittest.main()
