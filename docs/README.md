# PEPEPOW Miner documentation index

**Updated 2026-11-25.** This index is based on main `7284aa22cafd79e3521291307eca8f5b9a575744`, including merged PRs #22, #24, and #25. This remains a pre-release offline validation prototype, not a supported miner.

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
| [Source manifest](sources/g0-source-manifest.json) | Source revision and blob inventory |
| [OpenLibm Windows compile inventory](sources/openlibm-windows-compile-inventory.json) | Full pinned Windows portable-C translation-unit and header-signal records |
| [Provenance and license assessment](sources/G0_PROVENANCE_LICENSE_ASSESSMENT.md) | Technical source evidence separated from unresolved redistribution review |
| [Windows 11 owner acceptance](G0_WINDOWS11_OWNER_ACCEPTANCE.md) | Native Debug/Release full-parity procedure for the Owner host |
| [Windows 10/11 physical acceptance runbook](WINDOWS10_11_PHYSICAL_ACCEPTANCE.md) | Codex Local PowerShell runner for exact-main native G0/G1 technical evidence |
| [Windows 11 native G0/G1 evidence](VALIDATION_EVIDENCE/G0/windows11-native-g0-g1-2026-11-24.md) | Native Windows 11 Debug/Release parity and offline G1 test record |
| [G1 transport, reconnect, and worker evidence](VALIDATION_EVIDENCE/G1/transport-reconnect-cpu-workers-2026-11-25.md) | Exact-head Linux/Windows Server CI and local regression record for PRs #22, #24, and #25 |
| [HooHash worker audit](VALIDATION_EVIDENCE/G1/hoohash-v110-worker-audit-2026-11-25.md) | Pinned-source concurrency inspection, targeted digest parity, TSan scope, and local benchmark |
| [Native evidence archive](VALIDATION_EVIDENCE/G0/) | Archived focused validation evidence |
| [Offline CLI usage](../tools/README.md) | KAT/search/replay build and test commands |
| [Coding agent rules](../AGENTS.md) | Security, evidence, and English documentation policy |

## Current boundaries

- **G0 — NOT PASSED.** Pinned Core/OpenLibm and independent reference checks are recorded. PR #25 exact-head CI reran the independent 10,000-case full-digest parity matrix. Per-file license/NOTICE decisions, compiled dependency/link evidence, and formal Owner sign-off remain pending. The Owner chose a conservative compliance approach for separate resolution; Issue #1 stays open.
- **G1 — OFFLINE PROTOTYPE ONLY; NOT PASSED.** PRs #22, #24, and #25 add absolute deadlines, bounded reconnect and same-mode loopback failover, plus a configurable worker pool. Local mock regressions and exact-head Linux/Windows Server 2022 Debug/Release CI pass. Physical Windows 10/11 acceptance of this code, authorized production interoperability, and real-share testing are pending or deferred by Owner decision.
- [PR #22](https://github.com/edisontw/pepepow-miner/pull/22), [#24](https://github.com/edisontw/pepepow-miner/pull/24), and [#25](https://github.com/edisontw/pepepow-miner/pull/25) are merged. [PR #23](https://github.com/edisontw/pepepow-miner/pull/23) was closed after its frame-deadline change was verified in #22.
- [PR #21](https://github.com/edisontw/pepepow-miner/pull/21) carries the Core source reconciliation and OpenLibm source inventory. It does not approve redistribution or close Issue #1. [PR #19](https://github.com/edisontw/pepepow-miner/pull/19) added Windows-native parity CI. Earlier PRs #5, #7, #9, #10, #14, #16, #17, and #18 are also merged.
- **G2–G5 — NOT ACCEPTED.** GUI, packaging, and GPU releases remain out of scope.

The original chronological handoff is retained in [Git history at the pre-cleanup main commit](https://github.com/edisontw/pepepow-miner/blob/ebe03bed5ced88c4e11070b2e87eca57c05fce7b/docs/DEVELOPMENT_STATUS.md). Research notes and citation tokens are not independent consensus evidence.
