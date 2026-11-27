# PEPEPOW Miner documentation index

**Main snapshot: 2026-11-27.** Current `main` is `dfccd6fd2aeb3525cc41e27428537571642da6b7`. PR #30 integrated the reviewed #27, #28, and #29 heads with merge ancestry; PRs #31, #32, and #33 were then merged in dependency order. PRs #35–#37 are open and stacked; draft PR #38 adds offline bounded CUDA search and has exact-head Actions 4/4. Draft PR #39 records the G0/distribution-gate split; draft PR #40 adds bounded live CPU Pool support and one Windows 11 Pool test. The project remains a pre-release validation prototype, not a supported miner.

## Canonical reading order

| Document | Purpose |
| --- | --- |
| [Project README](../README.md) | Product scope and release safety boundary |
| [Development handoff](DEVELOPMENT_STATUS.md) | Current implementation, evidence, and acceptance blockers |
| [Roadmap](ROADMAP.md) | G0–G5 acceptance criteria and phase status |
| [G0 validation plan](G0_TECHNICAL_VALIDATION.md) | G0 Technical Validation acceptance and its separate distribution boundary |
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
| [Distribution License Compliance](../THIRD_PARTY_NOTICES.md) | Exact-package notices, provenance, SBOM, and pre-release status |
| [Linux Debug dependency inventory](sources/linux-debug-build-dependency-inventory.json) | Actual Debug compile inputs, included headers, linked archive members, and dynamic dependencies |
| [Linux Release dependency inventory](sources/linux-reference-build-dependency-inventory.json) | Actual Release compile inputs, included headers, linked archive members, and dynamic dependencies |
| [Windows 11 CPU benchmark evidence](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-2026-11-25.md) | Reproducible offline hashrate, resource measurements, worker parity, and current integration limits |
| [Windows 11 UCRT64 benchmark and stability report](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-ucrt64-gcc-2026-11-25.md) | 60-second thread matrix, 30-minute bounded stability run, 256-worker smoke test, stale-job switching, profiling evidence, and local regression results |
| [Native evidence archive](VALIDATION_EVIDENCE/G0/) | Archived focused validation evidence |
| [Offline CLI usage](../tools/README.md) | KAT/search/replay build and test commands |
| [Coding agent rules](../AGENTS.md) | Security, evidence, and English documentation policy |

## Current boundaries

- **G0 Technical Validation — PASS.** Ubuntu 22.04/24.04 native Debug/Release parity and Windows Server validation passed; the prior Windows 11 physical acceptance remains 84/84 and Windows 10 remains Owner equivalence without a separate physical run. See the [formal technical sign-off](VALIDATION_EVIDENCE/G0/formal-g0-technical-validation-2026-11-27.md). **Distribution License Compliance remains pending before any package release**, as described in the separate [distribution gate](../THIRD_PARTY_NOTICES.md); technical PASS is not package clearance.
- **G1 — NOT PASSED.** Main contains CPU loopback, reconnect, failover, worker, and benchmark coverage. Draft PR #40 adds opt-in bounded public-session support and records one Windows 11 CPU/POOL run with 1,000 answered replies (992 accepted, 8 generic rejected). This is one configuration only; the generic rejections cannot be classified from the Pool replies. Linux, SOLO, live reconnect/failover, and the other required configurations remain unvalidated. The evidence is [recorded on PR #40](https://github.com/edisontw/pepepow-miner/blob/929d8b9f40cccfb8be6bde45c3213c4ac31bac55/docs/VALIDATION_EVIDENCE/G1/windows11-cpu-pool-acceptance-2026-11-27.md); it is not integrated into main.
- [PR #22](https://github.com/edisontw/pepepow-miner/pull/22), [#24](https://github.com/edisontw/pepepow-miner/pull/24), and [#25](https://github.com/edisontw/pepepow-miner/pull/25) are merged. [PR #23](https://github.com/edisontw/pepepow-miner/pull/23) was closed after its frame-deadline change was verified in #22.
- [PR #21](https://github.com/edisontw/pepepow-miner/pull/21) carries the Core source reconciliation and OpenLibm source inventory. It does not approve redistribution or close Issue #1. [PR #19](https://github.com/edisontw/pepepow-miner/pull/19) added Windows-native parity CI. Earlier PRs #5, #7, #9, #10, #14, #16, #17, and #18 are also merged.
- **G2–G5 — NOT ACCEPTED.** GUI and packaging remain unstarted. CUDA PRs #35–#37 add pinned-Core PRNG/matrix and experimental full-digest parity; draft PR #38 adds bounded offline search with CPU candidate recheck. They do not implement a Stratum-integrated production GPU miner or pass G4. AMD remains unstarted.

The original chronological handoff is retained in [Git history at the pre-cleanup main commit](https://github.com/edisontw/pepepow-miner/blob/ebe03bed5ced88c4e11070b2e87eca57c05fce7b/docs/DEVELOPMENT_STATUS.md). Research notes and citation tokens are not independent consensus evidence.
