# PEPEPOW Miner — Current development status

**Status snapshot: 2026-11-28 (Asia/Taipei), main before the CUDA Stratum bridge integration.** Main at `634146739c154c0cfb7558991a6e3436e9b2dff2` includes PR #39, #35–#38, and #40, each merged with normal merge ancestry after its applicable review and exact-head checks. G0 Technical Validation is PASS. Distribution License Compliance is a separate pre-release gate. The CUDA Stratum bridge has physical Windows 11 RTX 3060 loopback evidence; no real CUDA pool share is recorded.

## Integrated implementation

- [PR #22](https://github.com/edisontw/pepepow-miner/pull/22) merged as `718b71a45aa6b24c2ab9ae82a964ebac014bbba9`. It integrates the local-only pinned-Core CPU miner and transport reliability fixes. The miner uses absolute monotonic frame and request deadlines, bounded subscribe/authorize/submit waits, generation invalidation and cleanup, and cooperative cancellation. All three Codex review findings were resolved. Codex reported no major issues on the final reviewed head `37d263554233559635ae8f44ae776434bfcc0233`.
- [PR #23](https://github.com/edisontw/pepepow-miner/pull/23) was closed after its frame-deadline patch was incorporated into PR #22. Original commit `fad8e1ba49cdf50a34114e2a22f8fa5cf7ab9319` and replacement cherry-pick `3aa4ca702c1c720ee13e76ea1210ee3e14123446` have identical stable patch IDs. The PR branch and review history remain available.
- [PR #24](https://github.com/edisontw/pepepow-miner/pull/24) merged as `238e5342bfd3d2825339f65464b45c244432bbf3`. Loopback-only reconnect uses bounded attempts, interruptible exponential backoff, fresh subscribe/authorize, connection generations, stale-response rejection, duplicate-submit prevention, and same-mode failover across two configured loopback ports.
- [PR #25](https://github.com/edisontw/pepepow-miner/pull/25) merged as `9c6f34f51a9f70d45e454eacbf1fe3388e61cf27`; PR #30 subsequently integrated the offline benchmark, worker limit of 1–256, `--threads all`, nonce range partitioning, and benchmark CLI. One worker remains the default. The [HooHash worker audit](VALIDATION_EVIDENCE/G1/hoohash-v110-worker-audit-2026-11-25.md) records source inspection and concurrency limits.
- PR #39 merged as `ad432a7f6bbe640cc1e645f258691fff8151b8c4`, recording G0 Technical Validation PASS and keeping exact-package notices/SBOM in the separate distribution gate.
- CUDA PR #35 merged as `3c91dbce7ea5bb0ca243ebdc4c62f9e1216a223d`, #36 as `cd02a41d917b98fdabc074f1ac4525afc34272a5`, #37 as `c3109df901c9c4ef4b05828dd7f83f84029fdac3`, and #38 as `9a4f95492d32d47660734b9dce9a6f3650d7ca85`. These add staged CUDA matrix/full-digest validation and offline bounded search with mandatory full-digest CPU candidate recheck; they do not pass G4.
- PR #40 merged as `634146739c154c0cfb7558991a6e3436e9b2dff2`, adding the explicit opt-in bounded CPU live-session CLI and its sanitized historical Windows 11 POOL evidence. The single run had 1,000 answered replies, 992 accepted, 8 generic rejects, and 0 Pool errors; it is not a G1 pass.
- [Issue #1](https://github.com/edisontw/pepepow-miner/issues/1) remains open for G1 and the remaining product/release gates.

The integrated socket suite uses numeric IPv4 loopback endpoints and local mock servers. PR #40 adds the explicitly started public CPU session; the historical 1,000-reply result is preserved in its sanitized report and is not repeated. This one Windows 11 POOL configuration is not G1 acceptance.

## Current owner acceptance and integrated main

- Physical Windows 11 Pro technical acceptance remains 84/84 on its documented historical head `7284aa22cafd79e3521291307eca8f5b9a575744`. The Owner accepts Windows 10 by equivalence; no separate physical Windows 10 execution was performed. Do not repeat the completed Windows acceptance absent a relevant regression.
- PR #30 merged as `78664ab5c7a5d560cfc9b0f833189379e6fdcf32`, preserving the #27–#29 merge ancestry. PR #31 merged as `0e262b67ae7c25f85e8c0b010235bf1beb2e4d1c`, PR #32 as `f2b08f9239dd6b45b49c4efc150b9f7b597913e1`, and PR #33 as `a505730696936117b2fa67c7a02b7ae4f1443856`.
- Main exact-head Actions all passed: Linux parity [45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621), Windows Server parity/dependency evidence [45824850719](https://github.com/edisontw/pepepow-miner/actions/runs/45824850719), frontend [45824850726](https://github.com/edisontw/pepepow-miner/actions/runs/45824850726), CUDA device probe [45846602967](https://github.com/edisontw/pepepow-miner/actions/runs/45846602967), CUDA correctness contract [45846603147](https://github.com/edisontw/pepepow-miner/actions/runs/45846603147), and CUDA BLAKE3 stage [45846602978](https://github.com/edisontw/pepepow-miner/actions/runs/45846602978).
- The [formal G0 Technical Validation sign-off](VALIDATION_EVIDENCE/G0/formal-g0-technical-validation-2026-11-27.md) reconciles CPU evidence and preserves Windows 11 84/84 plus Windows 10 Owner equivalence. Distribution notices are tracked separately in [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md); a technical PASS is not package clearance. Issue #1 remains open for G1 and later gates.

## Offline CPU benchmark development

The CPU benchmark work is integrated through PR #30. It adds a deterministic offline benchmark, supports 1–256 worker threads and `--threads all`, exposes benchmark mode through the offline miner CLI, and adds nonce-partition and benchmark CLI tests. The public [README](../README.md) remains user-oriented.

The native Windows development environment now uses the same MSYS2 UCRT64 ABI as the repository workflow, with dynamic OpenSSL integration and separate clean Debug/Release build directories. The final branch source passed the 10,000-case complete-digest parity checks, Explorer vectors, independent Python reference tests, negative controls, root and job-state CTest, offline replay, loopback, deadline, reconnect/backoff, failover, worker-pool, benchmark CLI, and 33 consecutive stale-job replacement checks. The exact-head Linux and Windows Server 2022 Debug/Release Actions runs and Codex review are tracked on PR #29. Do not rerun the completed Windows 11 84/84 physical acceptance.

Three 60-second runs per thread count (1/2/4/6/8/10/12) measured 38,771.16 H/s median at one thread and 340,326.84 H/s at twelve threads on the Windows 11 Ryzen 5 7600X. A bounded 30-minute, 12-thread run completed 568,154,427 hashes at 315,641.34 H/s and exited cleanly. A 256-worker two-second resource smoke test also exited cleanly. Full raw data, environment, resource samples, stale-switch timings, and profiling limitations are recorded in the [UCRT64 benchmark and stability report](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-ucrt64-gcc-2026-11-25.md). No measured function-level profile justified a worker-pool optimization; no consensus or floating-point behavior was changed.

## Pinned sources and exact-head validation

- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OpenLibm v0.8.7: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- The Windows portable-C OpenLibm patch is unchanged. Its Windows CRLF SHA-256 is `f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2`.

The final feature heads passed Linux Debug/Release and the relevant exact-head GitHub Actions checks.

- PR #22 head `37d263554233559635ae8f44ae776434bfcc0233`: [pinned-Core Linux workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45563709908), [Windows Server 2022 Debug/Release workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45563710032), and [Linux/Windows job-state matrix](https://github.com/edisontw/pepepow-miner/actions/runs/45563709920) passed. Root CTest passed 2/2 per configuration, job-state CTest passed 22/22, and local preflight CTest passed 16/16.
- PR #24 head `c06c292bd6ea13db85eaf7b860fddbacc0e7456b`: [pinned-Core Linux workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45588376421) and [Windows Server 2022 Debug/Release workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45588376319) passed. The Windows G1 step ran in both configurations.
- PR #25 head `a077384f429bdf55efde5669c73dd51d484050b4`: [pinned-Core Linux workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45589391208), [independent 10,000-case full-digest parity](https://github.com/edisontw/pepepow-miner/actions/runs/45589391352), [Windows Server 2022 Debug/Release workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45589391104), and [frontend contract workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45589391051) passed. The Windows G1 step ran in both configurations. The focused GCC ThreadSanitizer test passed 1/1.

The final CPU-worker Linux Debug/Release run passed root CTest 2/2 in each configuration. The job-state and preflight suites each passed 22/22 and 16/16 in Debug and Release. The proposed benchmark branch extends worker tests and the CLI to 256 as a hard cap while retaining one worker by default.

The Release microbenchmark measured local hashing of synthetic headers under a cloud cgroup quota of two CPUs. It is not an estimate of production-pool effective hashrate. The [worker audit](VALIDATION_EVIDENCE/G1/hoohash-v110-worker-audit-2026-11-25.md) records one measured run and its environment.

Detailed fault coverage and exact-head links are in [G1 transport and reconnect evidence](VALIDATION_EVIDENCE/G1/transport-reconnect-cpu-workers-2026-11-25.md).

## Formal gates and remaining decisions

**G0 Technical Validation — PASS.** Pinned-source provenance, Linux dependency inventories, full-digest parity, negative controls, Ubuntu 22.04/24.04 Debug/Release, and the preserved Windows 11 84/84 result are recorded in the [formal sign-off](VALIDATION_EVIDENCE/G0/formal-g0-technical-validation-2026-11-27.md). Windows 10 is Owner equivalence, with no separate physical run.

**Distribution License Compliance — PENDING before distribution.** The Owner adopts the public Hoosat MIT commit and pinned Core MIT HooHash headers as the current HooHash development basis. Preserve the old GPL headers and license-change history. Complete applicable notices and an SBOM for the exact release package; this gate does not block local development or technical G0. See [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

**G1 — NOT PASSED.** Main's local mocks cover deadlines, reconnect, failover, worker ranges, cancellation, stale results, and bounded shutdown. The integrated opt-in CPU CLI has one historical Owner-authorized Windows 11 CPU/POOL run: 1,000 answered replies in 1,008.07 seconds, one CPU thread, a 1,800-second cap, 992 accepted, 8 generic rejected (`result=false,error=null`), and 0 Pool error replies. The generic rejections cannot be classified. Preserve this result without repeating it. Linux, SOLO, live reconnect/failover, and the remaining required configurations are not validated. See the [sanitized report and full JSONL evidence](VALIDATION_EVIDENCE/G1/windows11-cpu-pool-acceptance-2026-11-27.md). One synchronous Core hash cannot be interrupted after it starts, so CPU cancellation is cooperative between hashes.

**G2–G5 — NOT ACCEPTED.** GUI work and distributable Windows/Linux packages are deferred. No binaries were released.

The CUDA Stratum bridge's physical Windows 11 RTX 3060 loopback validates local CUDA initialization, bounded search, full-digest CPU recheck, and mock submission. It is not a real-pool GPU test. CUDA POOL/SOLO shares, Linux native GPU evidence, broader CPU/CUDA CLI acceptance, and stability/soak remain pending.

The Owner's platform decisions preserve Windows 11 physical technical acceptance as PASS (84/84) on its documented head and Windows 10 as PASS by equivalence; no separate physical Windows 10 run was performed. G0 Technical Validation is signed off on the recorded CPU evidence. Distribution License Compliance, LAB Pool/share acceptance, and later product gates remain open. Linux Debug/Release inventories and hosted Windows Server dependency artifacts remain provenance evidence, not package clearance. Keep Issue #1 open until its remaining acceptance conditions are met.
