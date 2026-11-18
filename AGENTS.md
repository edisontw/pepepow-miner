# AI Coding Agent Instructions — PEPEPOW Miner

These instructions apply to the entire repository. Base every change on the current `main` branch, reproducible tests, and explicit owner authorization. Do not present planned features as implemented features.

## Documentation language

**Use English for all newly created and updated repository documentation**, including README files, Markdown documents, agent handoffs, commit messages, pull request titles/descriptions, and issue descriptions. Translate any existing non-English passage when substantively editing that document; do not rewrite unrelated historical research documents solely to change their language. The planned product may still offer a Traditional Chinese user interface. Keep code identifiers and original evidence intact.

## Required reading at the start of each work session

1. `README.md` — product scope and safety principles.
2. `docs/README.md` — documentation index and existing or unconfirmed research files.
3. `docs/ROADMAP.md` — canonical source of milestone status.
4. `docs/G0_TECHNICAL_VALIDATION.md` — acceptance criteria for the current first gate.
5. `docs/research/03_PRODUCT_DEVELOPMENT_PLAN.md` — product and engineering plan.
6. `docs/research/01_CORE_TECHNICAL_RESEARCH.md` and `docs/research/02_ARCHITECTURE_PERFORMANCE.md` — both research documents are already on `main`; read them and independently check their sources before beginning relevant work.

If research claims conflict, record the source text, source reference, commit, verification approach, and unresolved questions first. Never change the consensus algorithm directly based on research assertions. Research documents are not protocol authorities; use traceable Core/pool sources and reproducible tests.

For the architecture resulting from the research review, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): C/C++ with CMake is the adopted planned core; Qt is a conditional first choice pending packaging/licensing investigation, Rust/Tauri is an alternative, and AMD requires a real-hardware ADR. Also read [docs/RESEARCH_LEDGER.md](docs/RESEARCH_LEDGER.md). Follow the G0 → G1 → G2 → G3 acceptance order in `docs/ROADMAP.md`. Research-style citation markers are not resolvable on GitHub; add verifiable source URLs, commits, file paths, or test evidence.

## Current development gate

**G0 comes first. Do not claim the miner is usable until G0 has passed acceptance.** The next minimum verifiable deliverables are:

- Verify the current Core/pool source SHAs, implementation paths, licenses, and redistribution requirements.
- Obtain an independent expected digest and establish the first known-answer test (KAT) for an 80-byte header → full 32-byte digest.
- Build a native CPU harness that exits nonzero on mismatches and invalid input; deliberately altering the final digest byte must cause failure.
- Expand incrementally to at least 10,000 parity cases and native evidence on Windows 11 x64 and Ubuntu 22.04/24.04 x64.

One KAT, a scaffold, or a green CI result without real platform tests cannot mark G0 as passed. G1 Stratum/accepted shares, G2 GUI, and G4/G5 GPU support must not be claimed until their respective gates are satisfied.

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
