# AI Coding Agent Instructions — PEPEPOW Miner

These instructions apply to the entire repository. Recheck the current `main` and the exact head/base of every relevant PR, then work on the appropriate unmerged dependency branch with reproducible tests and explicit owner authorization. Never silently modify `main`. Do not present offline prototypes as supported mining software.

## Documentation language

**Use English for all newly created and updated repository documentation**, including README files, Markdown documents, agent handoffs, commit messages, pull request titles/descriptions, and issue descriptions. Translate any existing non-English passage when substantively editing that document; do not rewrite unrelated historical research documents solely to change their language. The planned product may still offer a Traditional Chinese user interface. Keep code identifiers and original evidence intact.

## Required reading at the start of each work session

1. `README.md` — product scope and safety principles.
2. `docs/DEVELOPMENT_STATUS.md` — concise, current G0/G1 handoff. Its archived chronological predecessor is historical evidence, **not** an instruction to repeat old work.
3. `docs/README.md` — documentation index and preserved historical sources.
4. `docs/ROADMAP.md` — gate criteria and status snapshot.
5. `docs/G0_TECHNICAL_VALIDATION.md` — acceptance criteria for the current first gate.
6. `docs/research/03_PRODUCT_DEVELOPMENT_PLAN.md` — product and engineering plan.
7. `docs/research/01_CORE_TECHNICAL_RESEARCH.md` and `docs/research/02_ARCHITECTURE_PERFORMANCE.md` — both research documents are already on `main`; read them and independently check their sources before beginning relevant work.

If research claims conflict, record the source text, source reference, commit, verification approach, and unresolved questions first. Never change the consensus algorithm directly based on research assertions. Research documents are not protocol authorities; use traceable Core/pool sources and reproducible tests.

For the architecture resulting from the research review, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): C/C++ with CMake is the adopted planned core; Qt is a conditional first choice pending packaging/licensing investigation, Rust/Tauri is an alternative, and AMD requires a real-hardware ADR. Also read [docs/RESEARCH_LEDGER.md](docs/RESEARCH_LEDGER.md). Follow the G0 → G1 → G2 → G3 acceptance order in `docs/ROADMAP.md`. Research-style citation markers are not resolvable on GitHub; add verifiable source URLs, commits, file paths, or test evidence.

## Current development gate

**G0 formal acceptance remains OPEN; no production miner may be claimed.** Significant prior evidence already includes native Linux Release/Debug independent 10,000-header full 32-byte parity, real chain KATs, worker/intermediate traces, and distinct Windows 10 and Windows Server results. Windows 11 is owner-approved deferred and **not** considered tested. Preserve exact source revisions, OpenLibm 0.8.7, intermediate data and mismatch reproducers. Do not repeat previously completed verification without identifying a regression or a missing acceptance requirement.

**G1 offline CPU search is a tested prototype, not a completed G1 gate.** The unmerged `g1-offline-cpu-search` branch at `5de6d64` contains a bounded 1–10,000 nonce search driven by an explicit offline header and target. Owner-reported native Linux checks passed eight search cases, 41 Python regressions, 22 Stratum tests and 10 chain KATs; this is not an exact-head CI result, Windows native validation or live pool evidence. The next focused deliverables are a minimal reviewable Draft PR, frozen independent expected-value fixture, wall-clock timeout, and eventual offline Stratum job-to-header/target integration. Do not duplicate the already merged PR #8 Stratum lifecycle, the PR #6 target comparator, or PR #2/#11 reference/nonce/atomic TSV work.

Never present Linux-only offline evidence as a working, released, or Windows-validated miner. Do not close G0/G1, enable live mining, connect to production pools, submit shares, or merge outstanding PRs without explicit additional authority.

## Non-negotiable boundaries

- Never request, record, or expose private keys, seed phrases, or wallet credentials; only use public payout addresses.
- Never mine covertly during installation, startup, or testing. Live pool testing requires explicit owner approval, with time and resource limits.
- Distinguish POOL from pool-based SOLO; failover must never silently change modes, payout addresses, or fees.
- Do not enable telemetry, remote write APIs, mining at system startup, or overclock/voltage controls by default.
- Do not copy or distribute third-party code before verifying licenses; preserve NOTICE files, license information, source SHAs, and patch records.
- Never invent test results, hashrates, GPU/OS compatibility, accepted shares, blocks, or earnings.

## Workflow and commits

- Choose one minimal verifiable deliverable per iteration. Read existing files before editing and avoid unrelated refactors.
- For each change, report its purpose, affected paths, commands run, test outcomes (passed / failed / pending), unresolved risks, and next step.
- Pin external dependencies with version and integrity information. Each assumption about consensus or byte order needs independent reference evidence.
- Preserve reproducible failing inputs and nonzero exit codes; do not swallow errors, compare only hash prefixes, or manufacture green CI status.
- Do not claim native Windows/Linux tests passed without native machine evidence. Cross-compilation does not replace native execution.
- When relocating research documents, preserve the original texts and filenames and update relative links and `docs/README.md`. Report missing files rather than creating placeholder documents misrepresented as originals.
- Record uncertain architecture decisions in an ADR or issue, distinguishing proposals from adopted decisions.
- Keep CI efficient: use path filters, per-job timeouts, and cancellation of obsolete runs for the same PR, but retain acceptance-critical testing.
