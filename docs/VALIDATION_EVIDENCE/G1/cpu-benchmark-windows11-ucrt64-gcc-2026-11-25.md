# CPU benchmark and stability evidence — Windows 11 UCRT64 — 2026-11-25

## Scope and method

These measurements run the pinned PEPEPOW Core HooHash V110 implementation against a deterministic synthetic 80-byte header and an all-zero target. The executable is network-free; no pool was contacted and no share was submitted. Results describe local hashing only and do not predict accepted work or production-pool hashrate.

The 60-second matrix contains three runs at each thread count, in interleaved rounds: 1, 2, 4, 6, 8, 10, and 12. Each run was bounded to 60 seconds. The runner recorded measured H/s, process CPU time normalized to the 12 logical CPU capacity, current and peak process working set, startup time, and deadline-stop latency. The original 10-second JSONL baseline remains unchanged in [the historical report](cpu-benchmark-windows11-2026-11-25.md) and [its raw JSONL](cpu-benchmark-windows11-2026-11-25.jsonl); its Clang/Zig toolchain differs, so rates are not directly comparable.

## Environment

- OS: physical Windows 11 Pro, build 26300. The Owner's 84/84 physical acceptance was not rerun.
- CPU: AMD Ryzen 5 7600X, 6 physical cores, 12 logical CPUs; installed RAM 33,519,362,048 bytes.
- Toolchain: native MSYS2 UCRT64 x86-64, GCC 16.2.0, CMake 4.5.1, Ninja 1.13.2, native x64 Python 3.13.17.
- OpenSSL: 4.1.0, dynamic linkage through the UCRT64 `libcrypto.dll.a` import library. The native Stratum tests used real OpenSSL; no test-only SHA stub was used.
- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`; OpenLibm: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- OpenLibm portable-C patch SHA-256: `f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2`.
- Release FP flags retained: `-fno-fast-math -ffp-contract=off`; OpenLibm additionally used `-fno-rounding-math`. No consensus algorithm or FP optimization was changed.
- The 60-second matrix used benchmark source commit `28f05800553e3689a8531010d7114ebf9e7a0953`. The 30-minute stability and 256-worker runs used the final worker-allocation-failure fix at `b147a2b7919b13263afa6ba1c0a3e19a5203d777`.

## 60-second thread-scaling results

Each cell is the median of three runs; bracketed values are the measured minimum and maximum. Working set is the median peak process working set across the three runs.

| Threads | Hashrate H/s, median [min, max] | CPU utilization (% of 12 logical CPUs), median | Peak working set, median (MiB) | Startup, median (ms) | Stop latency, median (ms) |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 38,771.16 [38,675.62, 38,829.04] | 8.32 | 5.31 | 0.0253 | 0.0198 |
| 2 | 72,643.58 [71,449.36, 72,795.61] | 16.60 | 5.49 | 0.1882 | 0.0298 |
| 4 | 123,952.40 [123,911.14, 124,272.42] | 31.33 | 5.59 | 0.2746 | 0.0361 |
| 6 | 182,924.89 [182,179.80, 183,561.59] | 48.05 | 5.70 | 0.3983 | 0.0373 |
| 8 | 241,120.10 [240,914.85, 241,540.26] | 63.62 | 5.80 | 0.4629 | 0.0413 |
| 10 | 298,152.56 [293,247.82, 298,265.61] | 79.80 | 5.91 | 0.5286 | 0.0410 |
| 12 | 340,326.84 [330,655.95, 343,247.43] | 94.14 | 6.02 | 0.6698 | 0.0445 |

On this machine and build, the median 12-thread result was 8.78 times the median single-thread result. This is a measured local ratio, not a cross-machine estimate.

Raw executable JSON is in [the 21-line JSONL](cpu-benchmark-windows11-ucrt64-gcc-20261125-60s.jsonl); per-run annotations are in [run details](cpu-benchmark-windows11-ucrt64-gcc-20261125-60s-run-details.jsonl), and the machine-readable aggregates are in [the summary](cpu-benchmark-windows11-ucrt64-gcc-20261125-60s-summary.json). The sanitized toolchain and hardware inventory is in [the environment record](cpu-benchmark-windows11-ucrt64-gcc-20261125-environment.json).

Windows reported the same 4701 MHz value through `Win32_Processor` at each sample; it is not per-core live clock telemetry. Temperature and power sensors were unavailable. No temperature or power conclusions are drawn.

## 30-minute offline stability run

The final Release executable ran for 1800 seconds with 12 workers and exited with code 0. It computed 568,154,427 hashes at 315,641.34 H/s, used 87.43% of the reported 12-logical-CPU capacity, and stopped 0.0388 ms after the duration boundary. The peak working set was 6,397,952 bytes. The process was absent after exit.

Across 60 resource samples, process handle count was 119 in both the first-five and last-five medians. Working-set median changed from 6,176,768 to 6,275,072 bytes (+98,304); private-bytes median changed by +8,192 bytes. Total process thread counts observed were 13, 14, and 15; later samples stabilized at 13. These samples show no sustained upward trend in handles, memory, or threads for this run; they are not a leak proof. Raw data: [monitor JSONL](cpu-benchmark-windows11-ucrt64-gcc-20261125-stability-30m-monitor.jsonl), [30-minute result and trend summary](cpu-benchmark-windows11-ucrt64-gcc-20261125-stability-30m-summary.json).

The measured 30-minute rate was lower than the 60-second 12-thread median. The difference is recorded without attributing a cause: temperature and power telemetry were unavailable.

## Worker scaling, job replacement, and profiling

- A bounded 2-second Release smoke run created a 256-worker pool on the 12-logical-CPU machine. It reached 260 process threads and 851 handles at peak, used 20,111,360 bytes of working set, completed 673,783 hashes, and exited cleanly. This is a resource/cleanup smoke test, not a useful throughput comparison. [Raw result](cpu-benchmark-windows11-ucrt64-gcc-20261125-256-worker-smoke.json).
- The CLI accepted `--threads all`, displayed the 1–256 worker and 1–1800 second bounds, and rejected `--threads 257` with exit code 2. Worker creation handles both `std::system_error` and `std::bad_alloc`, joining any partially created workers during cleanup.
- A loopback end-to-end test performed 33 consecutive stale-job replacements while reusing the same eight-worker pool. Every replacement reported `job_replaced`, rejected the stale candidate, and stayed below the test's 2-second cancellation bound. Debug latency min/median/max was 1.08/1.57/1.85 ms; Release was 1.04/1.40/1.46 ms. [Structured results](cpu-benchmark-windows11-stale-switch-e2e-20261125.json).
- A Windows process thread-CPU snapshot after 376.5 seconds found the 12 active hash workers accounted for 99.996% of process CPU time; worker CPU-time spread was 0.478%. This indicates balanced worker activity, not a function-level profile. [Snapshot](cpu-benchmark-windows11-thread-cpu-profile-20261125.json).
- Windows Performance Recorder could not start because Windows returned `0xc5585011` (“Failed to enable the policy to profile system performance”). WPA and xperf were not installed. A separate `-pg` build generated no gprof function samples or call entries. Function-level hotspots and lock-wait time therefore remain unmeasured; see the [profiler availability record](cpu-benchmark-windows11-profiler-availability-20261126.txt).

Source inspection found hashes run outside the worker mutex, which is used for task publication, completion, and condition-variable waiting. The pinned Core function keeps a local 64×64 FP64 matrix (about 32 KiB) and does not heap-allocate it per hash. With no measured hotspot and no prepared-matrix Core API, this round does not justify a scratch-buffer or worker scheduling optimization. Any future Core API or algorithm work still requires full digest parity.

## Regression results on the final branch source

Fresh separate Windows Debug and Release CMake build directories used the real UCRT64 OpenSSL integration and pinned Core/OpenLibm. Both configurations passed:

- 10,000 independent full 32-byte HooHash V110 parity comparisons and all 10 Explorer vectors.
- Six independent Python reference tests; empty-input rejection and deliberate final-byte digest mutation rejection.
- Root CTest: 5/5 in Debug and 5/5 in Release, including full, partial, and exhausted-uint32 nonce accounting. Job-state CTest: 22/22 in each configuration.
- Offline Stratum replay, TCP loopback, transport deadlines, reconnect/backoff, failover, the updated 33-switch test, and child-process cleanup.
- Worker nonce coverage, disjoint range/duplicate checks, overflow and `UINT32_MAX`; benchmark CLI JSON, argument bounds, and cooperative stop.

The current exact-head Linux and Windows Server 2022 Debug/Release Actions status is tracked on [Draft PR #29](https://github.com/edisontw/pepepow-miner/pull/29). Hosted CI is not Windows 11 device acceptance. No separate physical Windows 10 run was performed; Windows 10 remains PASS by Owner equivalence. The existing Windows 11 84/84 physical acceptance was not repeated.

## Boundaries

This evidence does not establish formal G0 or G1 acceptance, legal clearance, supported mining compatibility, accepted production shares, GPU support, or release readiness. Preserve upstream LICENSE/NOTICE and provenance; no repository-wide license was selected. No production pool was contacted, no real share was submitted, and no binary package was released.
