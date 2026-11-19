# Offline KAT frontend

`kat.c` accepts exactly two arguments: an 80-byte header in hex and a 32-byte
expected digest in **hoohashv110 function-output order**. It calls the linked
reference once, compares all 32 bytes and prints both digests. Exit codes:
0 match, 1 mismatch, 2 usage/hex/length error. It performs no mining or networking.

The reference library is not bundled yet. Never pass the pool candidate's
canonical/display string without independently establishing the byte mapping.
A successful synthetic contract test is not a HooHash KAT.

## Real reference build (pending)

```sh
cmake -S . -B build/reference -DPEPEPOW_REFERENCE_LIBRARY=/absolute/path/to/reviewed-reference-library
cmake --build build/reference
```

The supplied library must export the C ABI
`void hoohashv110(const void *, size_t, uint8_t[32])`.
Resolve its platform dependencies and runtime library search paths explicitly.
Record its source SHA, compiler/flags, openlibm configuration and independent
expected-vector provenance before recording any correctness result.
CMake configuration fails when the library path is absent; no test double is
silently substituted. The library path alone does not certify its provenance.

## Frontend contract tests (synthetic only)

```sh
cc -std=c11 -Wall -Wextra -Werror -O0 tools/kat.c tests/harness/synthetic_reference.c -o build/kat-contract-test
python tests/harness/test_contract.py build/kat-contract-test
```

Create `build/` first. On Windows use a native C compiler and an `.exe` suffix.
Repeat with `-O2`. The script requires Python 3 and no third-party Python packages.
It checks 17 cases, including the last digest byte, invalid lengths/hex, empty
and extra arguments. Invalid input must be rejected before the reference runs.

Alternative CMake build:
```sh
cmake -S . -B build/contract -DHARNESS_CONTRACT_TESTS_ONLY=ON
cmake --build build/contract
python tests/harness/test_contract.py build/contract/kat-contract-test
```
Use the actual executable path for multi-configuration generators.
CMake itself has not yet been exercised in the current local environment.

## Offline Stratum replay with native Core search

Configure the pinned Core/OpenLibm build as described in PR #13, with
`-DPEPEPOW_NATIVE_STRATUM_SEARCH=ON`. This adds the native replay and its
integration test. It requires OpenSSL Crypto for coinbase and Merkle SHA256d.
The bounded nonce search uses the same Core HooHash V110 code as
`pepepow-offline-search`.

```sh
cmake --build build/reference --parallel 4
ctest --test-dir build/reference --output-on-failure
timeout 45s build/reference/offline_stratum_replay mock.worker \
  --search 0 7 12345678 < transcript.jsonl
```

The JSONL transcript supplies correlated subscribe and authorize replies,
difficulty, and notify. After EOF the replay searches the latest authorized
job with the supplied extranonce2. Search accepts 1–10,000 nonces within
uint32 and has a 30-second cooperative deadline. Exit 1 indicates transcript
failure or exhausted search. Exit 2 indicates invalid input or unavailable
work. Exit 3 indicates a candidate could not be registered. Exit 4 indicates
the search deadline elapsed. The replay writes mock request frames to stdout
and opens no sockets.

The integration fixture checks the full header and digest, mock response
correlation, difficulty applying on the next notify, and candidate rejection
after same-ID replacement or reconnect. This exercises the fixed-order pool
dialect with local fixture data.
