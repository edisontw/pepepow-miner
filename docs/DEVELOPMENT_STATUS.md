# PEPEPOW Miner — Current development handoff

**Snapshot: 2026-11-23 (Asia/Taipei).** The tested code baseline is main `565d7002678905acdaa4eb21c82b1c7fb60dc4dc`. Re-fetch main, open PRs, and checks before relying on this snapshot.

## Integrated implementation

- [PR #14](https://github.com/edisontw/pepepow-miner/pull/14) integrated the pinned Core/CMake KAT, independent Python HooHash V110 implementation, full-digest corpus tools, uint256 little-endian target comparison, bounded offline CPU search, and offline Stratum replay. Earlier [#2](https://github.com/edisontw/pepepow-miner/pull/2), [#6](https://github.com/edisontw/pepepow-miner/pull/6), [#11](https://github.com/edisontw/pepepow-miner/pull/11), and [#13](https://github.com/edisontw/pepepow-miner/pull/13) were superseded.
- [PRs #5](https://github.com/edisontw/pepepow-miner/pull/5), [#7](https://github.com/edisontw/pepepow-miner/pull/7), [#9](https://github.com/edisontw/pepepow-miner/pull/9), [#10](https://github.com/edisontw/pepepow-miner/pull/10), [#16](https://github.com/edisontw/pepepow-miner/pull/16), [#17](https://github.com/edisontw/pepepow-miner/pull/17), and [#18](https://github.com/edisontw/pepepow-miner/pull/18) are merged.
- [PR #19](https://github.com/edisontw/pepepow-miner/pull/19) added native Windows Server 2022 Debug/Release independent parity CI and was squash merged as `565d7002678905acdaa4eb21c82b1c7fb60dc4dc`. Its exact PR-head and post-merge main runs both passed. At this snapshot, no PRs are open.
- [Issue #1](https://github.com/edisontw/pepepow-miner/issues/1) remains open and is the G0 acceptance tracker.

## Pinned sources and validation evidence

- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OpenLibm v0.8.7: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- Windows Server 2022 x64 GitHub-hosted native UCRT64 Debug and Release parity each compared 10,000 full 32-byte digests against the independent Python V110 implementation, matched 10 Explorer-derived headers, rejected an empty corpus, and rejected a deliberately corrupted final digest byte. Exact main-head evidence is [run 45181410916](https://github.com/edisontw/pepepow-miner/actions/runs/45181410916); PR-head evidence is [run 45179968956](https://github.com/edisontw/pepepow-miner/actions/runs/45179968956). The complete environment, commands, checksums, and boundaries are recorded in [Windows native G0 parity evidence](VALIDATION_EVIDENCE/G0/windows-native-parity-2026-11-23.md).
- The existing Ubuntu 22.04/24.04 × Debug/Release run remains [45075278541](https://github.com/edisontw/pepepow-miner/actions/runs/45075278541), on the prior code baseline `2ff357a8687f46f2ac6d593a8502bfa60a56d76b`. PR #19 changed CI/evidence tooling and a Windows-only OpenLibm build patch; it did not change HooHash or header hashing code.
- Windows Server CI is not Windows 10/11 device acceptance. The recorded Windows 11 owner result is partial: native Release build, frozen full 32-byte KAT, and an eight-nonce offline search passed on 2026-11-22. It does not replace the full Windows Debug/Release parity matrix or formal G0 sign-off.

## Formal gates

**G0 — NOT PASSED.** Full-digest Windows Server CI and prior Ubuntu matrix evidence are available, but the 2026-11-04 source manifest still inventories an older Core revision and needs current per-file blob/license reconciliation; license/NOTICE and redistribution readiness remain pending. Windows 11 full native G0 evidence and final checklist sign-off also remain open. A successful build is not a license clearance.

**G1 — OFFLINE PROTOTYPE ONLY; NOT PASSED.** Bounded CPU nonce search, mock/localhost Stratum replay, stale-job controls, and cooperative `--max-ms` deadline handling are implemented. The deadline cannot interrupt a single synchronous hash already in progress. No production pool connection, live share submission, released binary, or supported-miner claim has been authorized or demonstrated.

**G2–G5 — NOT ACCEPTED.** GUI, packaging, and GPU support are not production features.

## Next reviewable work

1. Complete the per-file source, provenance, license and NOTICE audit tracked by Issue #1. Preserve source notices and do not infer redistribution permission from CI.
2. Complete and record the remaining Windows 11 native G0 checklist without repeating the already-recorded Release/KAT/eight-nonce owner checks.
3. Keep subsequent G1 work offline and tied to a specific acceptance gap.

English is required for project documentation. Keep this handoff concise; track work by commit and PR rather than hourly append-only logs.
