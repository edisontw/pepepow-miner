# HooHash V110 CPU worker audit

## Reviewed source and build

- PEPEPOW Core `bc08108ceccd957c79040dc7fedc1d56d8d47f97`, OpenLibm v0.8.7 `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`. The Windows portable-C patch hashes to `f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2` in CRLF form and `9f37e2bb0e5c8160353bf7a624829eb2ffd14175495ae2e1a13a0c77739e0bb1` in the repository's LF form.
- Core `src/crypto/hoohash/hoohash.c` keeps its xoshiro state, matrix, intermediate digests, and BLAKE3 contexts within each call. Reviewed BLAKE3 translation units are `blake3.c`, `blake3_dispatch.c`, and `blake3_portable.c`. The pinned dispatcher directly invokes portable routines and returns SIMD degree 1. No mutable dispatch cache or one-time initialization is used on this path.
- The Linux miner reference archive imports `sin`, `cos`, `exp`, `floor`, and `sqrt`. Symbol inspection of the linked executable and pinned archive identifies the OpenLibm implementations. Reviewed the trigonometric kernels and argument reduction (`k_sin.c`, `k_cos.c`, `e_rem_pio2.c`, `k_rem_pio2.c`) and the corresponding public wrappers. Tables are constant and scratch storage is call-local. `e_exp.c` defines the volatile file-local `twom1000` once and only reads it. Linux `amd64/e_sqrt.S` uses `sqrtsd` in the caller's FP environment. The portable-C `src/e_sqrt.c` path uses locals and probes the current rounding direction with floating-point operations.
- Core compilation retains `HAVE_OPENLIBM=1`, GCC `-fno-fast-math -ffp-contract=off`, and the existing MSVC `/fp:strict` option. The reviewed routines use the thread's FP environment and can raise FP exception flags. They do not change the rounding mode. The CLI uses the platform default FP environment and the test checks rounding and all 32 digest bytes in each thread. OpenSSL constructs the coinbase and merkle header on the controller thread before dispatch.

## Targeted evidence

On Debian 13 x86-64 with GCC 14.2.0 and CMake 3.31.6, both native CTest cases passed in Debug and Release. The worker test checks 32 full digests for each of 1, 2, 4, and 8 workers, inclusive target equality, target rejection, exact nonce coverage through `UINT32_MAX`, and overflow rejection. Loopback tests cover generation replacement, stale candidates and replies, recovery, failover, duplicate suppression, and program shutdown during pending I/O, backoff, and active search. Signal cases run on POSIX.

GCC ThreadSanitizer passed the targeted native worker test with C/C++ instrumentation and an instrumented pinned OpenLibm C build. Linux `sqrt` remains a single uninstrumented register-only assembly instruction. The audit applies to this pinned path and compilation configuration. Windows hosted and physical evidence, compiled dependency-header and linked-member inventories for acceptance, license and NOTICE decisions, and formal sign-off are tracked separately.

## Release microbenchmark

Run `test_native_stratum_search --benchmark` on the Release build. Each entry exhausts three disjoint 10,000-nonce batches with a zero target.

| Workers | Hashes | Seconds | Local H/s |
| ---: | ---: | ---: | ---: |
| 1 | 30,000 | 1.161 | 25,851 |
| 2 | 30,000 | 0.599 | 50,059 |
| 4 | 30,000 | 0.615 | 48,817 |
| 8 | 30,000 | 0.588 | 51,050 |

The AMD EPYC 9V74 cloud container exposes three logical CPUs with an aggregate cgroup quota of two CPUs. The similar rates above two workers are specific to this run under that quota. These are local hash counts for a fixed synthetic header workload.
