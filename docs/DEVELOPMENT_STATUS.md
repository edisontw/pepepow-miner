# PEPEPOW Miner — Current development status

**Snapshot: 2026-11-25 (Asia/Taipei).** The feature implementation baseline is PR #25 merge `9c6f34f51a9f70d45e454eacbf1fe3388e61cf27`; later commits update documentation and audit evidence. The public README remains the product-facing overview.

## Integrated implementation

- [PR #22](https://github.com/edisontw/pepepow-miner/pull/22) merged as `718b71a45aa6b24c2ab9ae82a964ebac014bbba9`. It integrates the local-only pinned-Core CPU miner and transport reliability fixes. The miner uses absolute monotonic frame and request deadlines, bounded subscribe/authorize/submit waits, generation invalidation and cleanup, and cooperative cancellation. All three Codex review findings were resolved. Codex reported no major issues on the final reviewed head `37d263554233559635ae8f44ae776434bfcc0233`.
- [PR #23](https://github.com/edisontw/pepepow-miner/pull/23) was closed after its frame-deadline patch was incorporated into PR #22. Original commit `fad8e1ba49cdf50a34114e2a22f8fa5cf7ab9319` and replacement cherry-pick `3aa4ca702c1c720ee13e76ea1210ee3e14123446` have identical stable patch IDs. The PR branch and review history remain available.
- [PR #24](https://github.com/edisontw/pepepow-miner/pull/24) merged as `238e5342bfd3d2825339f65464b45c244432bbf3`. Loopback-only reconnect uses bounded attempts, interruptible exponential backoff, fresh subscribe/authorize, connection generations, stale-response rejection, duplicate-submit prevention, and same-mode failover across two configured loopback ports.
- [PR #25](https://github.com/edisontw/pepepow-miner/pull/25) merged as `9c6f34f51a9f70d45e454eacbf1fe3388e61cf27`. The CPU worker pool supports 1–8 workers and retains one worker by default. It partitions nonce ranges without overlap, handles the uint32 upper boundary, cancels stale work cooperatively, and reports local work metrics. The [HooHash worker audit](VALIDATION_EVIDENCE/G1/hoohash-v110-worker-audit-2026-11-25.md) records source inspection and the limits of the concurrency evidence.
- [Issue #1](https://github.com/edisontw/pepepow-miner/issues/1) remains open as the G0 acceptance tracker.

All socket integration tests use numeric IPv4 loopback endpoints and local mock servers. No production pool was contacted and no real share was submitted.

## Pinned sources and exact-head validation

- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OpenLibm v0.8.7: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- The Windows portable-C OpenLibm patch is unchanged. Its Windows CRLF SHA-256 is `f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2`.

The final feature heads passed Linux Debug/Release and the relevant exact-head GitHub Actions checks.

- PR #22 head `37d263554233559635ae8f44ae776434bfcc0233`: [pinned-Core Linux workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45563709908), [Windows Server 2022 Debug/Release workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45563710032), and [Linux/Windows job-state matrix](https://github.com/edisontw/pepepow-miner/actions/runs/45563709920) passed. Root CTest passed 2/2 per configuration, job-state CTest passed 22/22, and local preflight CTest passed 16/16.
- PR #24 head `c06c292bd6ea13db85eaf7b860fddbacc0e7456b`: [pinned-Core Linux workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45588376421) and [Windows Server 2022 Debug/Release workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45588376319) passed. The Windows G1 step ran in both configurations.
- PR #25 head `a077384f429bdf55efde5669c73dd51d484050b4`: [pinned-Core Linux workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45589391208), [independent 10,000-case full-digest parity](https://github.com/edisontw/pepepow-miner/actions/runs/45589391352), [Windows Server 2022 Debug/Release workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45589391104), and [frontend contract workflow](https://github.com/edisontw/pepepow-miner/actions/runs/45589391051) passed. The Windows G1 step ran in both configurations. The focused GCC ThreadSanitizer test passed 1/1.

The final CPU-worker Linux Debug/Release run passed root CTest 2/2 in each configuration. The job-state and preflight suites each passed 22/22 and 16/16 in Debug and Release. The native worker test compared full 32-byte digests with 1, 2, 4, and 8 workers and covered disjoint range coverage, target equality and rejection, rapid job replacement, stale-candidate rejection, active-work shutdown, and `UINT32_MAX`.

The Release microbenchmark measured local hashing of synthetic headers under a cloud cgroup quota of two CPUs. It is not an estimate of production-pool effective hashrate. The [worker audit](VALIDATION_EVIDENCE/G1/hoohash-v110-worker-audit-2026-11-25.md) records one measured run and its environment.

Detailed fault coverage and exact-head links are in [G1 transport and reconnect evidence](VALIDATION_EVIDENCE/G1/transport-reconnect-cpu-workers-2026-11-25.md).

## Formal gates and remaining decisions

**G0 — NOT PASSED.** The Owner adopted a conservative source/notice policy and Linux x86-64 Debug and Release builds now record actual compiler inputs, headers, archive members, and linker maps in the [Debug inventory](sources/linux-debug-build-dependency-inventory.json) and [Release inventory](sources/linux-reference-build-dependency-inventory.json). Rights questions remain for the Hoosat MIT root LICENSE versus GPL-3.0-or-later HooHash headers, OpenLibm per-file notices, BLAKE3 modifications, and pool-derived test material. Exact-head Windows Server 2022 Debug/Release include/link artifacts are available from [CI run 45600133198](https://github.com/edisontw/pepepow-miner/actions/runs/45600133198); formal Owner sign-off remains pending. No legal clearance is claimed; see [the handling decision](../THIRD_PARTY_NOTICES.md).

**G1 — OFFLINE PROTOTYPE ONLY; NOT PASSED.** Local mocks now cover deadlines, reconnect, failover, worker ranges, cancellation, stale results, and bounded shutdown. Authorized production-pool interoperability and real accepted shares remain untested. One synchronous Core hash cannot be interrupted after it starts, so cancellation is cooperative between hashes.

**G2–G5 — NOT ACCEPTED.** GUI work and distributable Windows/Linux packages are deferred. No binaries were released.

The Owner decisions from 2026-11-25 defer physical Windows 10/11 acceptance as a blocker to offline development, defer formal G0/G1 sign-off, defer production pool/share testing, and defer Windows/Linux package release. The conservative license and NOTICE handling policy is now recorded in [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md); rightsholder clarification and final distribution review remain open. The Linux Debug and Release actual-build inventories are recorded; Windows Server 2022 Debug/Release dependency and link artifacts were collected by PR #27 CI. This was hosted Server CI, not physical Windows 10/11 acceptance. Keep Issue #1 open until its established closure conditions are met.
