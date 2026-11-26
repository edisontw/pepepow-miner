# Offline CPU benchmark — Windows 11 — 2026-11-25

## Scope

This is a local, offline throughput measurement of the pinned PEPEPOW Core HooHash V110 reference. It uses one deterministic synthetic 80-byte header and a zero target, so each valid digest is rejected and the worker pool continues hashing. The benchmark opens no network connection and does not contact a pool or submit a share. These measurements describe local hash execution; they are not accepted-work or production-pool hashrate.

The benchmark reports measured hashes divided by elapsed wall time, process CPU time normalized by logical CPU capacity, process working set, process-start-to-worker-pool-ready time, and duration-stop latency. The timed stop latency is measured after the requested deadline. Current and peak working set are process-wide samples, not per-thread allocations. `--threads all` selected all 12 logical CPUs.

## Environment and build

- OS: Windows 11 Pro, build 26300, physical machine
- CPU: AMD Ryzen 5 7600X, 6 physical cores / 12 logical CPUs
- Installed memory: 33,519,362,048 bytes
- Compiler: Clang 20.1.2 through Zig 0.15.2; CMake 3.31.6; Ninja 1.11.1.3
- Configuration: Release, `-O3 -DNDEBUG`; Core/OpenLibm compiled with `-fno-fast-math` and `-ffp-contract=off`; OpenLibm additionally used `-fno-rounding-math` and strict floating-point exception behavior
- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`
- OpenLibm: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`
- Source baseline: `main` `7284aa22cafd79e3521291307eca8f5b9a575744`; measurements were run from the local offline CPU development branch based on that SHA
- Header: fixed 80 bytes, `0b30557a9fc4e90e33587da2c7ec11365b80a5caef14395e83a8cdf2173c6186abd0f51a3f6489aed3f81d42678cb1d6fb20456a8fb4d9fe23486d92b7dc01264b7095badf04294e7398bde2072c5176`
- Target: all zero bytes; starting nonce: 0; each 10,000-nonce batch is assigned contiguously and the final partial batch is accounted for without retry

## Measured results

Each row is a separate 10-second invocation. Values are the executable's measured JSON output, retained in [the raw results](cpu-benchmark-windows11-2026-11-25.jsonl).

| Threads | Hashes | H/s | Process CPU (% of 12-CPU capacity) | Working set current / peak (bytes) | Startup (ms) | Deadline stop latency (ms) | Complete batches + final hashes |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 349,160 | 34,915.99546 | 8.3203 | 4,308,992 / 4,308,992 | 0.0191 | 0.0013 | 34 + 9,160 |
| 2 | 684,978 | 68,497.53560 | 16.6145 | 4,751,360 / 4,751,360 | 0.0576 | 0.0386 | 68 + 4,978 |
| 4 | 1,329,404 | 132,939.92540 | 32.3957 | 4,870,144 / 4,870,144 | 0.1309 | 0.0357 | 132 + 9,404 |
| 8 | 2,189,457 | 218,945.05410 | 61.0675 | 5,079,040 / 5,079,040 | 0.1322 | 0.0295 | 218 + 9,457 |
| 12 (`all`) | 3,059,325 | 305,931.16000 | 95.7939 | 5,300,224 / 5,300,224 | 0.4531 | 0.0438 | 305 + 9,325 |

On this one machine and build, the 12-thread result was 8.76 times the single-thread rate and used 95.8% of the reported logical CPU capacity. This is an observed local baseline, not a forecast for other CPUs or pool work.

## Correctness and focused test results

The offline benchmark and its CLI tests were built and run natively on the Windows 11 host. CTest passed `cpu_worker_pool` and `cpu_benchmark_cli` (2/2). The pool test compared all 32 bytes of each digest against the pinned Core result with 1, 2, 4, 8, and 12 concurrent workers; it covered contiguous non-overlapping nonce ranges through `UINT32_MAX`, exact-target equality, target rejection, and overflow rejection. The CLI test covered help, invalid input, `--threads all`, measured-rate arithmetic, JSON metrics, duration stop, and Ctrl+Break stop. The latter test observed 1.34 ms from the parent's interrupt request to child exit.

Additional local checks on the same toolchain passed: the offline search CLI suite (15 cases, 8 independently checked frozen full digests, including nonce `4294967295`), six independent Python V110 tests, ten Explorer full-digest KAT vectors, and the malformed/empty/mutated-digest negative controls. These are focused local checks, not a rerun of the completed 84/84 Windows acceptance.

The exact-head GitHub CI for this new CPU benchmark branch has not run because this branch has not been pushed or opened as a PR. The existing PR #28 CI is for its separate head `c3c1929aa43ff3d38ad5fb2a0488179ac7a7b5d3` and does not validate this branch.

## CPU implementation findings

- The worker pool reuses worker threads. Its mutex protects job publication and completion; workers perform hashing outside that critical section. This source inspection did not measure mutex contention.
- The pinned `hoohashv110` implementation has a local `double mat[64][64]` matrix (about 32 KiB) and does not heap-allocate it per hash. The nonce is cleared while deriving the matrix seed, so matrix setup is invariant across nonces for the same header. The current Core API does not expose a prepared matrix/context. A per-thread heap scratch buffer would therefore not remove a current per-hash heap allocation; reusing a prepared job matrix may merit later investigation, but would require a carefully reviewed interface and full 32-byte parity evidence. No upstream Core code was changed.
- The benchmark's serial batch dispatch and cooperative cancellation do not simulate concurrent Stratum job replacement. Stale-job switching and lock-contention latency were not measured by this run.
- Nonce coverage is contiguous and disjoint within each issued range. The pool test reaches the `uint32` boundary; the 10-second throughput run itself did not exhaust the nonce space.

## Remaining validation

The full offline Stratum loopback, reconnect, failover, and end-to-end worker suite was not built on this machine because the Windows environment has no OpenSSL development headers or libraries (`openssl/sha.h` is unavailable). This does not invalidate the focused OpenSSL-free benchmark and worker tests; it leaves the broader integration suite pending. The new branch also needs exact-head Linux and Windows CI after it is published for review. No production-pool or real-share test is authorized or claimed.

Formal G0 and G1 status remains **NOT PASSED**. The Owner's Windows 11 physical acceptance is 84/84 PASS; Windows 10 is PASS by Owner equivalence with no separate physical Windows 10 run. These acceptance decisions are not a legal clearance or formal G0/G1 sign-off.
