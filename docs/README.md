# PEPEPOW Miner documentation index

**Main snapshot: 2026-11-26.** Current `main` is `a505730696936117b2fa67c7a02b7ae4f1443856`. PR #30 integrated the reviewed #27, #28, and #29 heads with merge ancestry; PRs #31, #32, and #33 were then merged in dependency order. The project remains a pre-release validation prototype, not a supported miner.

## Canonical reading order

| Document | Purpose |
| --- | --- |
| [Project README](../README.md) | Product scope and release safety boundary |
| [Development handoff](DEVELOPMENT_STATUS.md) | Current implementation, evidence, and acceptance blockers |
| [Roadmap](ROADMAP.md) | G0–G5 acceptance criteria and phase status |
| [G0 validation plan](G0_TECHNICAL_VALIDATION.md) | Full-digest parity and platform acceptance gates |
| [Source audit](G0_SOURCE_AUDIT.md) | Pinned sources, candidate provenance, and license review |
| [Implementation notes](G0_IMPLEMENTATION_NOTES.md) | Historical G0 harness and test records |
| [Research ledger](RESEARCH_LEDGER.md) | Source review and unresolved assumptions |
| [Architecture decisions](ARCHITECTURE.md) | CPU/Stratum/GUI/GPU architecture |
| [CUDA Stratum integration](CUDA_STRATUM_INTEGRATION.md) | Opt-in bounded CUDA child process, cancellation, full-digest CPU candidate recheck, and stale-result suppression |
| [Source manifest](sources/g0-source-manifest.json) | Source revision and blob inventory |
| [OpenLibm Windows compile inventory](sources/openlibm-windows-compile-inventory.json) | Full pinned Windows portable-C translation-unit and header-signal records |
| [Provenance and license assessment](sources/G0_PROVENANCE_LICENSE_ASSESSMENT.md) | Technical source evidence separated from unresolved redistribution review |
| [Windows 11 owner acceptance](G0_WINDOWS11_OWNER_ACCEPTANCE.md) | Native Debug/Release full-parity procedure for the Owner host |
| [Windows 10/11 physical acceptance runbook](WINDOWS10_11_PHYSICAL_ACCEPTANCE.md) | Codex Local PowerShell runner for exact-main native G0/G1 technical evidence |
| [Windows 11 native G0/G1 evidence](VALIDATION_EVIDENCE/G0/windows11-native-g0-g1-2026-11-24.md) | Native Windows 11 Debug/Release parity and offline G1 test record |
| [G1 transport, reconnect, and worker evidence](VALIDATION_EVIDENCE/G1/transport-reconnect-cpu-workers-2026-11-25.md) | Exact-head Linux/Windows Server CI and local regression record for PRs #22, #24, and #25 |
| [HooHash worker audit](VALIDATION_EVIDENCE/G1/hoohash-v110-worker-audit-2026-11-25.md) | Pinned-source concurrency inspection, targeted digest parity, TSan scope, and local benchmark |
| [Third-party notices and distribution decision](../THIRD_PARTY_NOTICES.md) | Conservative notice handling, per-dependency release gates, and current package status |
| [Linux Debug dependency inventory](sources/linux-debug-build-dependency-inventory.json) | Actual Debug compile inputs, included headers, linked archive members, and dynamic dependencies |
| [Linux Release dependency inventory](sources/linux-reference-build-dependency-inventory.json) | Actual Release compile inputs, included headers, linked archive members, and dynamic dependencies |
| [Windows 11 CPU benchmark evidence](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-2026-11-25.md) | Reproducible offline hashrate, resource measurements, worker parity, and current integration limits |
| [Windows 11 UCRT64 benchmark and stability report](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-ucrt64-gcc-2026-11-25.md) | 60-second thread matrix, 30-minute bounded stability run, 256-worker smoke test, stale-job switching, profiling evidence, and local regression results |
| [CUDA Stratum process bridge evidence](VALIDATION_EVIDENCE/G4/cuda-stratum-process-bridge-windows11-2026-11-28.md) | Windows 11 Debug/Release CPU/mock contract, compile checks, and explicit GPU/pool test limits |
| [CUDA Stratum nonce cursor evidence](VALIDATION_EVIDENCE/G4/cuda-stratum-nonce-cursor-windows11-2026-11-28.md) | Windows 11 Debug/Release nonce coverage at UINT32_MAX and atomic extranonce2 rollover contract |
| [Native evidence archive](VALIDATION_EVIDENCE/G0/) | Archived focused validation evidence |
| [Offline CLI usage](../tools/README.md) | KAT/search/replay build and test commands |
| [Coding agent rules](../AGENTS.md) | Security, evidence, and English documentation policy |

## Current boundaries

- **G0 Technical Validation — NOT YET SIGNED OFF.** Ubuntu 22.04/24.04 Debug/Release, Windows Server 2022 Debug/Release, and the historical Windows 11 84/84 evidence remain recorded; Windows 10 remains Owner equivalence without a separate physical run. Reconcile the technical checklist before sign-off. Distribution license compliance is a separate gate and does not block G0 technical work. See [G0 validation](G0_TECHNICAL_VALIDATION.md) and [distribution notices](../THIRD_PARTY_NOTICES.md). Issue #1 stays open.
- **G1 — OFFLINE PROTOTYPE ONLY; NOT PASSED.** CPU loopback, reconnect, failover, worker, and benchmark evidence is integrated from PR #30. Owner-authorized live pool/share acceptance remains gated on G0 and has not been run.
- [PR #22](https://github.com/edisontw/pepepow-miner/pull/22), [#24](https://github.com/edisontw/pepepow-miner/pull/24), and [#25](https://github.com/edisontw/pepepow-miner/pull/25) are merged. [PR #23](https://github.com/edisontw/pepepow-miner/pull/23) was closed after its frame-deadline change was verified in #22.
- [PR #21](https://github.com/edisontw/pepepow-miner/pull/21) carries the Core source reconciliation and OpenLibm source inventory. It does not approve redistribution or close Issue #1. [PR #19](https://github.com/edisontw/pepepow-miner/pull/19) added Windows-native parity CI. Earlier PRs #5, #7, #9, #10, #14, #16, #17, and #18 are also merged.
- **G2–G5 — NOT ACCEPTED.** GUI and packaging remain unstarted. CUDA PRs [#31](https://github.com/edisontw/pepepow-miner/pull/31), [#32](https://github.com/edisontw/pepepow-miner/pull/32), and [#33](https://github.com/edisontw/pepepow-miner/pull/33) add the offline probe, correctness harness, and BLAKE3 stage. Draft [PR #41](https://github.com/edisontw/pepepow-miner/pull/41) adds a process bridge, cooperative cancellation, full-digest CPU candidate recheck, and CPU/mock contract tests. No full GPU HooHash parity or G4 acceptance is claimed. AMD remains unstarted.

The original chronological handoff is retained in [Git history at the pre-cleanup main commit](https://github.com/edisontw/pepepow-miner/blob/ebe03bed5ced88c4e11070b2e87eca57c05fce7b/docs/DEVELOPMENT_STATUS.md). Research notes and citation tokens are not independent consensus evidence.
