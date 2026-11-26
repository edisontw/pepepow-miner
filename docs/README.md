# PEPEPOW Miner documentation index

**Integration snapshot: 2026-11-26 (local only).** This worktree applies PR #28, then #27, then #29 over live main `7284aa22cafd79e3521291307eca8f5b9a575744`. All three remain open and unmerged; PR #29 is still draft. PRs #22, #24, and #25 are already merged. This remains a pre-release offline validation prototype, not a supported miner.

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
| [Third-party notices and distribution decision](../THIRD_PARTY_NOTICES.md) | Conservative notice handling, per-dependency release gates, and current package status |
| [Linux Debug dependency inventory](sources/linux-debug-build-dependency-inventory.json) | Actual Debug compile inputs, included headers, linked archive members, and dynamic dependencies |
| [Linux Release dependency inventory](sources/linux-reference-build-dependency-inventory.json) | Actual Release compile inputs, included headers, linked archive members, and dynamic dependencies |
| [Windows 11 CPU benchmark evidence](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-2026-11-25.md) | Reproducible offline hashrate, resource measurements, worker parity, and current integration limits |
| [Windows 11 UCRT64 benchmark and stability report](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-ucrt64-gcc-2026-11-25.md) | 60-second thread matrix, 30-minute bounded stability run, 256-worker smoke test, stale-job switching, profiling evidence, and local regression results |
| [Native evidence archive](VALIDATION_EVIDENCE/G0/) | Archived focused validation evidence |
| [Offline CLI usage](../tools/README.md) | KAT/search/replay build and test commands |
| [Coding agent rules](../AGENTS.md) | Security, evidence, and English documentation policy |

## Current boundaries

- **G0 — NOT PASSED.** Pinned Core/OpenLibm and independent reference checks include the 10,000-case full-digest parity matrix. The conservative license/NOTICE handling policy and actual Linux Debug/Release compile, include, and link inventories are recorded. Windows Server 2022 dependency artifacts are available from [PR #27 CI](https://github.com/edisontw/pepepow-miner/actions/runs/45601241135); see [the distribution decision](../THIRD_PARTY_NOTICES.md). Rightsholder questions and formal Owner sign-off remain open; Issue #1 stays open.
- **G1 — OFFLINE PROTOTYPE ONLY; NOT PASSED.** The local integration tree combines PRs #28, #27, and #29. PR #29 adds the offline CPU benchmark and 1–256 worker limit while retaining one worker by default. Exact-head Linux/Windows CI and local regression evidence for the individual PRs are linked from [PR #29](https://github.com/edisontw/pepepow-miner/pull/29). The previously recorded Windows 11 physical acceptance passed 84/84 and Windows 10 is PASS by Owner equivalence without a separate physical run; these are not new device acceptance for this combined tree. Authorized production interoperability and real-share testing remain deferred.
- [PR #22](https://github.com/edisontw/pepepow-miner/pull/22), [#24](https://github.com/edisontw/pepepow-miner/pull/24), and [#25](https://github.com/edisontw/pepepow-miner/pull/25) are merged. [PR #23](https://github.com/edisontw/pepepow-miner/pull/23) was closed after its frame-deadline change was verified in #22.
- [PR #21](https://github.com/edisontw/pepepow-miner/pull/21) carries the Core source reconciliation and OpenLibm source inventory. It does not approve redistribution or close Issue #1. [PR #19](https://github.com/edisontw/pepepow-miner/pull/19) added Windows-native parity CI. Earlier PRs #5, #7, #9, #10, #14, #16, #17, and #18 are also merged.
- **G2–G5 — NOT ACCEPTED.** GUI, packaging, and GPU releases remain out of scope.

The original chronological handoff is retained in [Git history at the pre-cleanup main commit](https://github.com/edisontw/pepepow-miner/blob/ebe03bed5ced88c4e11070b2e87eca57c05fce7b/docs/DEVELOPMENT_STATUS.md). Research notes and citation tokens are not independent consensus evidence.
