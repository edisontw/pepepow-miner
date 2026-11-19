"""Exercise the compiled C frontend with a synthetic reference; NOT hash parity."""
import pathlib
import subprocess
import sys

exe = pathlib.Path(sys.argv[1]).resolve()
header = bytes(range(80))
expected = bytes(header[i] ^ header[i + 32] ^ header[i + 48] for i in range(32))
h, d = header.hex(), expected.hex()
last = (expected[:-1] + bytes([expected[-1] ^ 1])).hex()
first = (bytes([expected[0] ^ 1]) + expected[1:]).hex()
cases = [
    ("match", [h, d], 0),
    ("uppercase", [h.upper(), d.upper()], 0),
    ("last-byte-mismatch", [h, last], 1),
    ("first-byte-mismatch", [h, first], 1),
    ("truncated-header", [h[:-2], d], 2),
    ("oversize-header", [h + "00", d], 2),
    ("odd-header", [h[:-1], d], 2),
    ("invalid-header-hex", ["gg" + h[2:], d], 2),
    ("header-whitespace", [" " + h[1:], d], 2),
    ("truncated-digest", [h, d[:-2]], 2),
    ("oversize-digest", [h, d + "00"], 2),
    ("invalid-digest-hex", [h, "zz" + d[2:]], 2),
    ("missing-digest", [h], 2),
    ("missing-all", [], 2),
    ("extra-argument", [h, d, "extra"], 2),
    ("empty-header", ["", d], 2),
    ("empty-digest", [h, ""], 2),
]
for name, args, code in cases:
    run = subprocess.run([str(exe), *args], text=True, capture_output=True, timeout=10)
    assert run.returncode == code, (name, run.returncode, run.stdout, run.stderr)
    if code == 2:
        assert "SYNTHETIC_REFERENCE" not in run.stderr, name
    else:
        assert "SYNTHETIC_REFERENCE_NOT_HOOHASH" in run.stderr, name
        assert "actual=" + d in run.stdout, name
        assert "expected=" + args[1].lower() in run.stdout, name
        assert ("MISMATCH" in run.stderr) == (code == 1), name
    print(f"PASS {name}: exit={code}")
print(f"PASS {len(cases)} frontend contract cases; HooHash parity NOT tested")
