# Offline loopback CPU miner evidence — 2026-11-24

Status: **focused Linux prototype evidence only; G0 and G1 remain NOT PASSED**.
No production pool was contacted and all share replies came from a mock server
bound to `127.0.0.1`.

## Source and toolchain

- Miner base: `f8f9325ec9cb5c12ca0058ca2858536198f4b616` (post-PR #21 `main`).
- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`, clean checkout.
- OpenLibm: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`, clean checkout.
- OpenLibm static archive: SHA-256 `c81c4594e53d459d516c81a8820dcb8307a61353c7459f822c42ed82dba6c5f3`.
- Host: Debian GNU/Linux 13.6, x86_64 Linux kernel `6.18.57`.
- GCC/G++: `14.2.0-19`; CMake: `4.4.5`; OpenSSL Crypto: `3.5.7`.
- Python: `3.12.14`.
- OpenLibm was rebuilt from its pinned checkout with `BUILD_SHARED_LIBS=OFF`
  and `CMAKE_BUILD_TYPE=Release`. The Core target links the absolute archive
  above; it does not resolve `libm` as a substitute. Core flags were
  `-O3 -DNDEBUG -std=gnu11 -fno-fast-math -ffp-contract=off` with
  `HAVE_OPENLIBM=1`.

## Build commands

```sh
/workspace/g1-tools/cmake/data/bin/cmake -S /workspace/openlibm \
  -B /workspace/openlibm-build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF
/workspace/g1-tools/cmake/data/bin/cmake --build /workspace/openlibm-build \
  --clean-first --parallel 4
/workspace/g1-tools/cmake/data/bin/cmake -S /workspace/g1-cpu-miner-mvp \
  -B /workspace/g1-cpu-miner-mvp/build/reference \
  -DCMAKE_BUILD_TYPE=Release \
  -DHARNESS_CONTRACT_TESTS_ONLY=OFF \
  -DPEPEPOW_NATIVE_STRATUM_SEARCH=ON \
  -DPEPEPOW_CORE_SOURCE_DIR=/workspace/core \
  -DPEPEPOW_CORE_EXPECTED_SHA=bc08108ceccd957c79040dc7fedc1d56d8d47f97 \
  -DOPENLIBM_INCLUDE_DIR=/workspace/openlibm/include \
  -DOPENLIBM_LIBRARY=/workspace/openlibm-build/libopenlibm.a
/workspace/g1-tools/cmake/data/bin/cmake --build \
  /workspace/g1-cpu-miner-mvp/build/reference --parallel 4
```

The pinned OpenLibm build completed. GCC emitted warnings in upstream `lgamma`
and long-double sources (`-Wmaybe-uninitialized` and one unused helper); these
did not fail the build. No source workaround or system math library was used.

## Focused results

| Command | Result |
| --- | --- |
| `ctest --test-dir build/reference --output-on-failure` | 2/2 PASS: native Stratum search and loopback CPU miner integration |
| `tests/test_offline_stratum_miner_e2e.py build/reference/offline_stratum_miner` | PASS: only `127.0.0.1`; cooperative stale cancellation; complete expected 80-byte header and 32-byte Core digest; one accepted and one rejected mock response; 1 ms real-Core timeout exits 4; invalid port/range/count/timeout/scale/duplicate options exit 2 |
| `tests/check_pool_loopback_replay.py build/reference/offline_stratum_replay build/reference/pepepow-kat` | PASS: pinned pool wire fixture, first valid nonce 2, full native KAT |
| `tests/check_offline_cpu_search.py build/reference/pepepow-offline-search` | PASS: 15 CLI cases and 8 independent frozen full digests, including nonce `4294967295` |
| `ctest --test-dir build/job-state-debug --output-on-failure` | 22/22 PASS: job state, fixed-dialect decoders, framing, replay lifecycle, and socket loopback |
| `git diff --check` | PASS |

The focused CTest and the explicit end-to-end script exited 0. The CLI reports
the same fixture header and raw digest as
`tests/vectors/stratum/pool-loopback-expected.json`. The scripted accepted and
rejected outcomes are local mock responses, not pool interoperability evidence.

An exploratory Release build of the separate `tests/job_state` project failed
at compile time because existing assert-only locals become unused under
`NDEBUG` with `-Werror`. Its Debug build and all 22 tests passed. The root
Release build and its Release loopback integration test passed.

## Limits

The worker checks cancellation and deadline between nonce hashes. A single
synchronous pinned-Core hash call cannot be interrupted. This evidence is
Debian Linux only; it does not add Windows G1 evidence, an authorized pool
session, live accepted shares, reconnect/failover, or release approval.
