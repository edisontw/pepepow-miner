# PEPEPOW Miner — Current development handoff

**Snapshot: 2026-11-20, Asia/Taipei.** GitHub `main` is the source of truth. At this snapshot, main was `ebe03bed5ced88c4e11070b2e87eca57c05fce7b`; re-fetch the actual HEAD, PRs and checks before making changes.

The former ~149 KB chronological handoff remains retrievable without modification in [the pre-cleanup Git revision](https://github.com/edisontw/pepepow-miner/blob/ebe03bed5ced88c4e11070b2e87eca57c05fce7b/docs/DEVELOPMENT_STATUS.md). Do not reintroduce hourly append-only logs. Keep original evidence, tracked fixtures and source manifests in version history.

## Integrated implementation

- [PR #14](https://github.com/edisontw/pepepow-miner/pull/14) was **rebase merged** into main with a linear commit history. It includes the pinned Core/CMake KAT, standalone independent Python HooHash V110, full 32-byte corpus/trace utilities, uint256 little-endian comparison, bounded offline CPU nonce search, and offline Stratum replay.
- Its prior contributors [#2](https://github.com/edisontw/pepepow-miner/pull/2), [#6](https://github.com/edisontw/pepepow-miner/pull/6), [#11](https://github.com/edisontw/pepepow-miner/pull/11), and [#13](https://github.com/edisontw/pepepow-miner/pull/13) are closed as **superseded**, not independently merged.
- Earlier [PR #8](https://github.com/edisontw/pepepow-miner/pull/8) provided the offline Stratum session/controller. Do not duplicate it.
- Still open at this snapshot: #5 prevhash byte-order fixture, #7 compact nBits target decoder, and stacked #9/#10 localhost-only preflight/handshake deadline. These files are not on main; require separate review and focused native evidence.

## Exact reference pins and evidence boundary

- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OpenLibm v0.8.7: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- Pool serializer fixture reference: `edisontw/pool-pepepow@c1298d7b40e5e9fd39f76af179c6e10a1c313147`.
- PR #14 final-head native Linux Release tests were reported passing: 15 offline search CLI cases and 8 independently checked full digests, fixed local-loopback replay/nonce 2, 10/10 chain corpus KATs, 42/42 Python regression tests, 22/22 native job state tests, and 1/1 native Stratum search CTest.
- Four PR-head Actions workflows succeeded at `c24bcb68c29e60d9d61141e79578d937c2b3bb74`. After rebase merge, the G0 synthetic frontend main-push workflow at `ebe03bed5ced88c4e11070b2e87eca57c05fce7b` also succeeded. These CI jobs do **not** collectively establish G0 or G1 acceptance.
- Earlier Linux Debug/Release runs reportedly compared 10,000 independent full 32-byte digests with zero mismatches; the original exact commands, traces, compiler/FP assumptions and provenance are in the archived handoff. This cleanup did **not** rerun them at merged main.

## Formal gates

**G0 — NOT PASSED.** Review and reproduce >=10,000 independent full-digest parity cases, Core/reference provenance, source licenses/notices, platform-specific native execution and negative controls. Never equate a synthetic harness or self-consistency check with independent HooHash parity. Windows 10/Server results are distinct from Windows 11; the Windows 11 workstation run is **owner-deferred, NOT passed**.

**G1 — OFFLINE PROTOTYPE ONLY.** The source includes count-bounded (1–10,000) CPU nonce search, cooperative `--max-ms` deadline, mock Stratum notify/coinbase/Merkle/target reconstruction, pool wire difficulty scale 65536 fixture, and stale/reconnect controls. The deadline does not preempt a single active hash call. No live mining, production pool connection, accepted shares, released binary, or Windows 11 acceptance has been authorized or demonstrated.

**G2–G5 — NOT ACCEPTED.** GUI, packaging and GPU support are not production features.

## Next reviewable work

1. Reproduce the independent 10,000-case parity evidence on exact merged main against pinned Core/OpenLibm in native Linux Release/Debug. Preserve provenance and negative controls; mark unsupported OS platforms pending.
2. Review #5/#7 and #9/#10 separately; do not silently import their old branch history or run live pool tests.
3. Expand G1 offline error/reconnect/deadline fixtures only for a demonstrated gap. Seek owner authorization before any live pool or accepted-share run.

English is required for new or substantively edited project documentation. Keep one concise status document; track work by commit/PR instead of hourly log appendices.
