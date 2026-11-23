# PEPEPOW Miner documentation index

**Updated 2026-11-23.** The tested code baseline is main `565d7002678905acdaa4eb21c82b1c7fb60dc4dc`. This is a pre-release offline validation prototype, not a supported miner.

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
| [Native evidence](VALIDATION_EVIDENCE/G0/) | Archived focused validation evidence |
| [Offline CLI usage](../tools/README.md) | KAT/search/replay build and test commands |
| [Coding agent rules](../AGENTS.md) | Security, evidence, and English documentation policy |

## Current boundaries

- **G0 — NOT PASSED.** Pinned Core/OpenLibm and independent reference checks are in place. Ubuntu 22.04/24.04 Debug/Release evidence is recorded in [run 45075278541](https://github.com/edisontw/pepepow-miner/actions/runs/45075278541). Native Windows Server 2022 Debug/Release parity is recorded in [run 45181410916](https://github.com/edisontw/pepepow-miner/actions/runs/45181410916) and [the evidence report](VALIDATION_EVIDENCE/G0/windows-native-parity-2026-11-23.md). Per-file source license/NOTICE clearance, Windows 11 full G0 evidence, and formal sign-off remain pending; Issue #1 stays open.
- **G1 — OFFLINE PROTOTYPE ONLY; NOT PASSED.** Bounded CPU search, cooperative deadline, offline Stratum replay, and localhost preflight/deadline controls are implemented. There is no production pool/share authorization.
- [PR #19](https://github.com/edisontw/pepepow-miner/pull/19) added Windows-native parity CI and is merged. PRs #5, #7, #9, #10, #14, #16, #17, and #18 are also merged; no PRs were open at this snapshot.
- **G2–G5 — NOT ACCEPTED.** GUI, packaging, and GPU releases remain out of scope.

The original chronological handoff is retained in [Git history at the pre-cleanup main commit](https://github.com/edisontw/pepepow-miner/blob/ebe03bed5ced88c4e11070b2e87eca57c05fce7b/docs/DEVELOPMENT_STATUS.md). Research notes and citation tokens are not independent consensus evidence.
