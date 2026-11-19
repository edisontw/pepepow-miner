# PEPEPOW Miner documentation index

**Updated: 2026-11-19.** Read the live GitHub `main` and PR heads before acting on any dated snapshot. This repository is **pre-release**: offline prototype and differential tests do not establish release readiness, accepted shares, or full G0 acceptance.

## Start here

| Document | Purpose |
| --- | --- |
| [Project README](../README.md) | Product scope, supported claims, and release safety |
| [Current development status](DEVELOPMENT_STATUS.md) | **Short authoritative handoff**: current G0/G1 evidence, branches, pending work |
| [Roadmap](ROADMAP.md) | Acceptance gates G0–G5 and their actual status |
| [G0 validation plan](G0_TECHNICAL_VALIDATION.md) | Formal full-digest / native-platform requirements |
| [Architecture decisions](ARCHITECTURE.md) | Core, CLI, GUI, GPU, and job-lifecycle design |
| [Research verification ledger](RESEARCH_LEDGER.md) | Source/commit provenance and unresolved assumptions |
| [Source audit](G0_SOURCE_AUDIT.md) | Pinned sources and candidate vector provenance |
| [G0 implementation notes](G0_IMPLEMENTATION_NOTES.md) | Early harness implementation and historical evidence boundaries |
| [Source manifest](sources/g0-source-manifest.json) | Exact upstream revisions / blobs |
| [Recorded native evidence](VALIDATION_EVIDENCE/G0/) | Original focused test records, not automatically current HEAD evidence |
| [Offline CLI and KAT usage](../tools/README.md) | Current offline build/run interfaces and limits |
| [Coding agent instructions](../AGENTS.md) | Security, documentation language, and reproducibility rules |

## Research reports (preserved)

1. [Core technical research](research/01_CORE_TECHNICAL_RESEARCH.md)
2. [Architecture and performance research](research/02_ARCHITECTURE_PERFORMANCE.md)
3. [Product development plan](research/03_PRODUCT_DEVELOPMENT_PLAN.md)

The three original research reports are historical source material, **not acceptance evidence**. Some imported citation tokens are not GitHub-resolvable; verify material claims against pinned upstream sources. Do not rewrite the original reports just to change their language.

## Historical status and cleanup policy

The former ~149 KB chronological `DEVELOPMENT_STATUS.md` was retired from the active tree. Its complete original content remains available in [the previous Git revision](https://github.com/edisontw/pepepow-miner/blob/5de6d64aab6696918ab37bbdfd5d5164e33fc041/docs/DEVELOPMENT_STATUS.md), which records historical heads, CI runs, failures, fixes and test claims. Do not mistake these historical entries for current HEAD evidence. Use this concise handoff for new work instead of reinstating hourly logs.

No reproducibility-critical code, KAT fixture, source manifest, or validation evidence should be deleted as "temporary" without proving that it has been superseded and is not referenced by a test or open PR. Local build outputs, generated TSVs, caches, and logs belong outside version control (see `.gitignore`).

## Current scope boundary

- **G0:** substantial 10,000-vector full-digest Linux and native-platform evidence exists, but formal gate acceptance remains open. Windows 10, Windows Server, and deferred Windows 11 evidence are distinct.
- **G1:** branch `g1-offline-cpu-search` has a Linux-validated **offline-only** bounded CPU search proof of concept; it is not a live miner, and it has no PR or release at this snapshot.
- **G2–G5:** no accepted desktop/GPU release capabilities.
