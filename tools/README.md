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
