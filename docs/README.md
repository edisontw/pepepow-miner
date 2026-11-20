# PEPEPOW Miner documentation index

**Updated 2026-11-20.** Read live GitHub main and PR heads before relying on this dated index. This is a **pre-release offline validation prototype**, not a supported miner.

## Canonical reading order

| Document | Purpose |
| --- | --- |
| [Project README](../README.md) | Product scope and release safety boundary |
| [Development handoff](DEVELOPMENT_STATUS.md) | Concise G0/G1 implementation, evidence, and current blockers |
| [Roadmap](ROADMAP.md) | G0–G5 acceptance criteria and phase status |
| [G0 validation plan](G0_TECHNICAL_VALIDATION.md) | Full-digest parity and platform acceptance gates |
| [Source audit](G0_SOURCE_AUDIT.md) | Pinned sources, candidate provenance and licenses |
| [Implementation notes](G0_IMPLEMENTATION_NOTES.md) | Historical G0 harness and test records |
| [Research ledger](RESEARCH_LEDGER.md) | Source review and unresolved assumptions |
| [Architecture decisions](ARCHITECTURE.md) | CPU/Stratum/GUI/GPU architecture |
| [Source manifest](sources/g0-source-manifest.json) | Source revision and blob inventory |
| [Native evidence](VALIDATION_EVIDENCE/G0/) | Archived focused validation evidence |
| [Offline CLI usage](../tools/README.md) | KAT/search/replay build and test commands |
| [Coding agent rules](../AGENTS.md) | Security, evidence and English documentation policy |

The original research reports remain available under `docs/research/`:
[01 Core technical research](research/01_CORE_TECHNICAL_RESEARCH.md),
[02 Architecture and performance](research/02_ARCHITECTURE_PERFORMANCE.md), and
[03 Product development plan](research/03_PRODUCT_DEVELOPMENT_PLAN.md).
Research claims and imported citation tokens are not independent consensus evidence.

## Historical handoff archive

The former ~149 KB chronological status document, with historical CI, source changes and test claims, is retained verbatim in [Git history at the pre-cleanup main commit](https://github.com/edisontw/pepepow-miner/blob/ebe03bed5ced88c4e11070b2e87eca57c05fce7b/docs/DEVELOPMENT_STATUS.md). Replacing the active file does not remove historical evidence or committed fixtures.

## Current boundaries

- **G0:** the pinned Core harness, independent Python checks, real-chain fixtures, and earlier reported Linux 10,000-case parity exist, but formal acceptance is **NOT PASSED**. Windows 11 hardware verification is owner-deferred.
- **G1:** bounded offline CPU search, cooperative deadline and mock/loopback Stratum replay merged via [PR #14](https://github.com/edisontw/pepepow-miner/pull/14). No production pool/share or OS support claim.
- **Unmerged work:** [PR #5](https://github.com/edisontw/pepepow-miner/pull/5) prevhash, [#7](https://github.com/edisontw/pepepow-miner/pull/7) nBits, [#9](https://github.com/edisontw/pepepow-miner/pull/9) and [#10](https://github.com/edisontw/pepepow-miner/pull/10) localhost preflight/deadline; review independently, not as part of #14.
- **G2–G5:** desktop/UI/GPU release gates not passed.
