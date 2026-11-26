# Offline KAT frontend

`kat.c` accepts exactly two arguments: an 80-byte header in hex and a 32-byte
expected digest in **hoohashv110 function-output order**. It calls the linked
reference once, compares all 32 bytes and prints both digests. Exit codes:
0 match, 1 mismatch, 2 usage/hex/length error. It performs no mining or networking.

The reference library is not bundled yet. Never pass the pool candidate's
canonical/display string without independently establishing the byte mapping.
A successful synthetic contract test is not a HooHash KAT.

## Real reference build (pinned Core/OpenLibm; offline only)

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
Earlier CI and native Linux validation exercised CMake; check the exact checkout and build flags for new evidence.

## Offline Stratum replay with native Core search

Configure the pinned Core/OpenLibm build documented in [the current handoff](../docs/DEVELOPMENT_STATUS.md), with
`-DPEPEPOW_NATIVE_STRATUM_SEARCH=ON`. This adds the native replay and its
integration test. It requires OpenSSL Crypto for coinbase and Merkle SHA256d.
The bounded nonce search uses the same Core HooHash V110 code as
`pepepow-offline-search`.

```sh
cmake --build build/reference --parallel 4
ctest --test-dir build/reference --output-on-failure
timeout 45s build/reference/offline_stratum_replay mock.worker \
  --search 0 3 00000001 < tests/vectors/stratum/pool-loopback.jsonl
```

The JSONL transcript supplies correlated subscribe and authorize replies,
difficulty, and notify. After EOF the replay searches the latest authorized
job with the supplied extranonce2. The optional final argument overrides the
wire difficulty scale. It defaults to 65536 for the pinned PEPEPOW pool.
Use the scale configured by the source pool when replaying another transcript.
Search accepts 1–10,000 nonces within
uint32 and has a 30-second cooperative deadline. Exit 1 indicates transcript
failure or exhausted search. Exit 2 indicates invalid input or unavailable
work. Exit 3 indicates a candidate could not be registered. Exit 4 indicates
the search deadline elapsed. The replay writes mock request frames to stdout
and opens no sockets.

The integration fixture checks the full header and digest, mock response
correlation, difficulty applying on the next notify, and candidate rejection
after same-ID replacement or reconnect. This exercises the fixed-order pool
dialect with local fixture data.

The committed [loopback transcript](../tests/vectors/stratum/pool-loopback.jsonl)
contains the four server frames captured on an OS-assigned 127.0.0.1 port from
`edisontw/pool-pepepow@c1298d7b40e5e9fd39f76af179c6e10a1c313147`.
The service used the test RPC template from `tests/test_stratum_ingress.py`
with the nonuniform prevhash from its notify-capture test, fixed test session
and extranonce values, and effective difficulty 0.0015. The wire difficulty
was 98.304. The pool's submit-side coinbase-dialect and header helpers produced
the expected header, and the independent Python V110 implementation computed
the expected digest. The fixture covers the pinned pool's actual wire serializer
with test data and contains no production traffic.

```sh
timeout 30s python3 tests/check_pool_loopback_replay.py \
  build/reference/offline_stratum_replay build/reference/pepepow-kat
```

## Offline loopback CPU miner MVP

`offline_stratum_miner` joins the local TCP transport to the fixed-dialect
controller, header builder, bounded CPU search, target comparison, and mock
submit accounting. It creates an IPv4 socket for `127.0.0.1` internally; the
CLI accepts one or two numeric loopback ports. Hostnames, DNS, TLS, and public-pool endpoints are outside this tool’s scope.
Run it only with a mock Stratum server bound to loopback.

Build against the pinned Core and OpenLibm inputs used above:

```sh
cmake -S . -B build/reference -DCMAKE_BUILD_TYPE=Release \
  -DHARNESS_CONTRACT_TESTS_ONLY=OFF \
  -DPEPEPOW_NATIVE_STRATUM_SEARCH=ON \
  -DPEPEPOW_CORE_SOURCE_DIR=/absolute/path/to/pinned-core \
  -DPEPEPOW_CORE_EXPECTED_SHA=bc08108ceccd957c79040dc7fedc1d56d8d47f97 \
  -DOPENLIBM_INCLUDE_DIR=/absolute/path/to/openlibm/include \
  -DOPENLIBM_LIBRARY=/absolute/path/to/openlibm-build/libopenlibm.a
cmake --build build/reference --parallel 4
ctest --test-dir build/reference --output-on-failure \
  -R 'native_stratum_search|offline_stratum_miner_loopback'
```

The end-to-end fixture starts a mock server on an OS-assigned `127.0.0.1`
port, replays the pinned pool wire fixture, sends a replacement while a real
Core search is running, checks the replacement candidate's complete 80-byte
header and 32-byte digest, and returns one accepted and one rejected mock
reply. It asserts stale-candidate accounting and exercises a real-Core 1 ms
cooperative timeout with exit code 4. Run it directly with:

```sh
timeout 30s python3 tests/test_offline_stratum_miner_e2e.py \
  build/reference/offline_stratum_miner
```

The CLI syntax is:

```text
offline_stratum_miner PORT LOGIN START COUNT EXTRANONCE2
  [--responses N] [--max-ms N] [--scale N] [--threads N]
  [--retries N] [--fallback-port PORT] [--backoff-ms N] [--mode POOL|SOLO]
```

`COUNT` is 1–10,000 and the inclusive nonce range must fit uint32. `--max-ms`
defaults to 30,000 and is capped at 60,000. `--threads` accepts 1–8 and
defaults to one. Workers reuse their threads across jobs, search disjoint nonce
ranges, and return the lowest candidate whose preceding range was checked.
The deadline and job replacement
are cooperative between nonce hashes; neither can interrupt one synchronous
Core hash call. Subscribe, authorize, and submit each have a two-second
operation budget across all received frames.

Reconnect defaults to zero retries. `--retries` accepts 0–10. The first retry
wait defaults to 100 ms and doubles on each retry up to 2,000 ms. If configured,
the fallback port must differ from the primary and the retry count must be
positive. Attempts alternate between the two ports while retaining the same
`LOGIN` and configured mode (default `POOL`). The mock server defines the
endpoint's actual behavior. A submit with an ambiguous delivery outcome is
counted only if a valid reply arrived, and the miner suppresses replay of that
job and header for the remainder of the process.

JSONL stdout reports each full candidate, mock response, stale
candidate, and final counters. Each search emits its completed hash count,
elapsed time, active worker count, local rate for that search, accumulated rate,
job and connection generation, and completion reason. A reported accepted/rejected share is only the
local mock server's reply and is not pool interoperability evidence. Exit 0
means the requested number of mock share replies arrived; 1 is transport,
protocol, or exhausted-work failure; 2 is invalid input or work; 4 is the
search deadline; 5 is local socket initialization failure; and 130 is signal-driven shutdown.

## Opt-in CUDA test tools

The root CPU build remains CUDA-free. The standalone [CUDA tools guide](gpu/README.md)
covers the offline device inventory probe and a separately enabled correctness
harness that uses the pinned CPU oracle. The latter currently tests a small
nonce-coverage kernel only; it does not implement or claim HooHash CUDA parity.
See the [native Windows RTX 3060 evidence](../docs/validation/cuda-correctness-harness-windows-rtx3060-2026-11-26.md)
for the Debug/Release build and real-device test results.
