# Offline reference and bounded CPU search (pre-release)

This directory contains validation utilities, **not** a production miner. All commands below operate on explicit offline inputs. Do not connect to a production pool, submit shares, or handle wallet credentials.

## Pinned real-reference build

The real-reference build needs clean, separately checked-out sources:

- PePe-core commit `bc08108ceccd957c79040dc7fedc1d56d8d47f97`
- OpenLibm v0.8.7 commit `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`

Build OpenLibm as a local static library; do **not** substitute platform `libm`. With `core/` and `openlibm/` next to this miner checkout:

```sh
cmake -S ../openlibm -B ../openlibm-build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build ../openlibm-build --parallel 2

cmake -S . -B build/reference -DCMAKE_BUILD_TYPE=Release \
  -DHARNESS_CONTRACT_TESTS_ONLY=OFF \
  -DPEPEPOW_CORE_SOURCE_DIR="$(realpath ../core)" \
  -DPEPEPOW_CORE_EXPECTED_SHA=bc08108ceccd957c79040dc7fedc1d56d8d47f97 \
  -DOPENLIBM_INCLUDE_DIR="$(realpath ../openlibm/include)" \
  -DOPENLIBM_LIBRARY="$(realpath ../openlibm-build/libopenlibm.a)"
cmake --build build/reference --parallel 2
```

The build checks the exact Core revision and rejects tracked modifications. The C ABI is `void hoohashv110(const void *, size_t, uint8_t[32])`. You may instead pass a **reviewed** `PEPEPOW_REFERENCE_LIBRARY`, but its path alone does not prove provenance. Do not use synthetic mode for real-hash evidence.

## Offline CPU nonce search

```text
pepepow-offline-search HEADER_HEX TARGET_BE_HEX START COUNT [--replace-job]
```

- `HEADER_HEX`: exactly 80 bytes as 160 hex characters. The CLI overwrites header bytes 76–79 with each candidate nonce, little-endian.
- `TARGET_BE_HEX`: exactly 32 bytes as 64 big-endian hex characters. The CLI reverses it for the existing little-endian uint256 comparator.
- `START`: unsigned 32-bit decimal starting nonce.
- `COUNT`: **1–10,000 inclusive**. A range that would overflow uint32 is rejected.
- `--replace-job`: offline test flag that simulates a same-ID job replacement after a qualifying candidate, returning stale instead of accepting that candidate. It is not real network behavior.

Exit codes: **0** first qualifying digest, **1** search exhausted, **2** invalid arguments or unsafe nonce range, **3** simulated stale candidate. A success line prints the qualifying nonce and the complete 32-byte digest in raw function-output order. The program performs **no network operation**.

The CLI currently caps **candidate count but has no built-in elapsed-time limit**. On Linux, wrap invocations in an explicit foreground timeout; for example:

```sh
timeout --signal=TERM 30s build/reference/pepepow-offline-search \
  "$HEADER_HEX" "$TARGET_BE_HEX" 4294967288 8
python3 tests/check_offline_cpu_search.py build/reference/pepepow-offline-search
```

The second command invokes the independent Python reference against a fixed on-chain header corpus, with a subprocess timeout. It currently derives its expected nonce and digest during the test; a separately frozen expected vector is still a follow-up. The owner reported eight local Linux search checks passing on commit `5de6d64`; this cleanup does not rerun those native tests.

## Real-reference KAT

`pepepow-kat` accepts an 80-byte header hex and an expected 32-byte digest hex in **raw HooHash function-output order**. It compares all 32 bytes and returns 0 match, 1 mismatch, or 2 invalid usage/hex/length. Never pass a pool/RPC display digest until byte order has been independently established.

```sh
build/reference/pepepow-kat "$HEADER_HEX" "$EXPECTED_RAW_DIGEST_HEX"
```

## Synthetic frontend contract tests (not HooHash parity)

```sh
cmake -S . -B build/contract -DHARNESS_CONTRACT_TESTS_ONLY=ON
cmake --build build/contract
python3 tests/harness/test_contract.py build/contract/kat-contract-test
```

These 17 frontend contract cases check parsing, full-byte comparison, invalid inputs and exit codes using a **synthetic** reference. They are not consensus proof. A synthetic configuration must not be mixed with real-reference flags.

For detailed historical evidence and remaining platform acceptance, see [the current status](../docs/DEVELOPMENT_STATUS.md) and [roadmap](../docs/ROADMAP.md).
