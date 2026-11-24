# PEPEPOW Miner — Current development handoff

**Snapshot: 2026-11-24 (Asia/Taipei).** This handoff is based on main after the PR #21 merge `f8f9325ec9cb5c12ca0058ca2858536198f4b616`. Re-fetch main, open PRs, and checks before relying on this snapshot.

## Integrated implementation

- [PR #14](https://github.com/edisontw/pepepow-miner/pull/14) integrated the pinned Core/CMake KAT, independent Python HooHash V110 implementation, full-digest corpus tools, uint256 little-endian target comparison, bounded offline CPU search, and offline Stratum replay. Earlier [#2](https://github.com/edisontw/pepepow-miner/pull/2), [#6](https://github.com/edisontw/pepepow-miner/pull/6), [#11](https://github.com/edisontw/pepepow-miner/pull/11), and [#13](https://github.com/edisontw/pepepow-miner/pull/13) were superseded.
- [PRs #5](https://github.com/edisontw/pepepow-miner/pull/5), [#7](https://github.com/edisontw/pepepow-miner/pull/7), [#9](https://github.com/edisontw/pepepow-miner/pull/9), [#10](https://github.com/edisontw/pepepow-miner/pull/10), [#16](https://github.com/edisontw/pepepow-miner/pull/16), [#17](https://github.com/edisontw/pepepow-miner/pull/17), and [#18](https://github.com/edisontw/pepepow-miner/pull/18) are merged.
- [PR #19](https://github.com/edisontw/pepepow-miner/pull/19) added native Windows Server 2022 Debug/Release independent parity CI and was squash merged as `565d7002678905acdaa4eb21c82b1c7fb60dc4dc`. Its exact PR-head and post-merge main runs both passed.
- [PR #21](https://github.com/edisontw/pepepow-miner/pull/21) was squash-merged as `f8f9325ec9cb5c12ca0058ca2858536198f4b616`. It reconciles the 13-file Core manifest to the tested Core pin and adds the generated 254-file Windows portable-C OpenLibm inventory, conservative notice signals, and provenance/Owner-validation instructions. These audit records do not clear redistribution or formal G0 acceptance.
- The offline CPU miner MVP now has a single local-only TCP CLI that connects the fixed Stratum controller, real pinned-Core search, and mock submit accounting. Its focused loopback test verifies replacement cancellation, a complete frozen header/digest, one accepted mock reply, one rejected mock reply, a real-Core cooperative timeout, and stale-candidate accounting. The Linux Release commands and results are recorded in [G1 loopback evidence](VALIDATION_EVIDENCE/G1/offline-loopback-cpu-miner-2026-11-24.md). It does not accept a remote host or connect to a production pool.
- [Issue #1](https://github.com/edisontw/pepepow-miner/issues/1) remains open and is the G0 acceptance tracker.

## Pinned sources and validation evidence

- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OpenLibm v0.8.7: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- Windows Server 2022 x64 GitHub-hosted native UCRT64 Debug and Release parity each compared 10,000 full 32-byte digests against the independent Python V110 implementation, matched 10 Explorer-derived headers, rejected an empty corpus, and rejected a deliberately corrupted final digest byte. Exact main-head evidence is [run 45181410916](https://github.com/edisontw/pepepow-miner/actions/runs/45181410916); PR-head evidence is [run 45179968956](https://github.com/edisontw/pepepow-miner/actions/runs/45179968956). The complete environment, commands, checksums, and boundaries are recorded in [Windows native G0 parity evidence](VALIDATION_EVIDENCE/G0/windows-native-parity-2026-11-23.md).
- The existing Ubuntu 22.04/24.04 × Debug/Release run remains [45075278541](https://github.com/edisontw/pepepow-miner/actions/runs/45075278541), on the prior code baseline `2ff357a8687f46f2ac6d593a8502bfa60a56d76b`. PR #19 changed CI/evidence tooling and a Windows-only OpenLibm build patch; it did not change HooHash or header hashing code.
- The Windows portable-C inventory derives from artifacts of run [45181410916](https://github.com/edisontw/pepepow-miner/actions/runs/45181410916). Debug and Release each have 254 unique translation units with identical path sets. The full file rows and checksums are in [the generated inventory](sources/openlibm-windows-compile-inventory.json); its source/license boundary is documented in [the provenance assessment](sources/G0_PROVENANCE_LICENSE_ASSESSMENT.md).
- Windows Server CI is not Windows 10/11 device acceptance. The recorded Windows 11 owner result is partial: native Release build, frozen full 32-byte KAT, and an eight-nonce offline search passed on 2026-11-22. It does not replace the full Windows Debug/Release parity matrix or formal G0 sign-off.
- The remaining Windows 11 owner procedure is [G0 Windows 11 acceptance](G0_WINDOWS11_OWNER_ACCEPTANCE.md). Do not repeat the already-recorded standalone Release/KAT/eight-nonce checks.

## Formal gates

**G0 — NOT PASSED.** Current Core source blobs are reconciled to the tested pin, and Windows portable-C translation units are inventoried. Hoohash file headers conflict with the upstream root-license change; OpenLibm, BLAKE3, and pool-derived source notices still need redistribution review. Historical Windows artifacts lack dependency files and a final linker map. Windows 11 full native G0 evidence and final checklist sign-off remain open. A successful build is not a license clearance.

**G1 — OFFLINE PROTOTYPE ONLY; NOT PASSED.** The local-only CPU CLI performs subscribe/authorize, fixed-dialect work decoding, header construction, pinned-Core HooHash search, target comparison, mock submission, response accounting, job-replacement cancellation, and bounded response-count shutdown. Linux Release loopback evidence is a local mock only. The deadline and cancellation are cooperative between nonce hashes and cannot interrupt a single synchronous hash already in progress. Reconnect/failover, broader protocol fault coverage, authorized platform interoperability, and G0 acceptance remain pending. No production pool connection, live share, released binary, or supported-miner claim has been authorized or demonstrated.

**G2–G5 — NOT ACCEPTED.** GUI, packaging, and GPU support are not production features.

## Next reviewable work

1. Record the remaining Owner/rightsholder decisions and platform-specific dependency/link evidence tracked by Issue #1.
2. Complete and record the remaining Windows 11 native G0 checklist without repeating the already-recorded Release/KAT/eight-nonce owner checks.
3. Extend the local CPU CLI fault matrix and lifecycle controls without introducing remote endpoints; keep G1 acceptance pending.

English is required for project documentation. Keep this handoff concise; track work by commit and PR rather than hourly append-only logs.
