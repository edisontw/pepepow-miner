# PEPEPOW Miner

## Development status and release policy

**Under development (Pre-release).** This project is still being developed and technically validated. **HooHash V110 consensus parity has not yet passed the G0 acceptance gate.** This repository must not be treated as a working, safe-to-use, or released miner. Do not rely on it for live mining.

**CI resource policy:** Avoid redundant runs for the same changes; use path filters, cancellation of outdated runs for the same pull request, and reasonable per-job timeouts. Run focused checks first, reserving full cross-platform verification for relevant changes and release gates. Standard GitHub-hosted runners may be free for public repositories, but compute resources must still be used responsibly.

A PEPEPOW-specific native miner for Windows and Linux, designed to be friendly to everyday computers and to explain mining performance clearly.

**Current status: offline-only validation prototype.** Pinned-Core CPU nonce search and Stratum replay are in source control following [PR #14](https://github.com/edisontw/pepepow-miner/pull/14); G0/G1 acceptance is still open. No production-ready miner or downloadable release is available.

## Product direction

- Native Windows x64 and Linux x64 CPU core, with a headless CLI.
- Everyday-use mode: initial manual resource profiles and clear start/stop controls; automatic activity detection and adaptive throttling deferred.
- Health diagnostics: distinguish connection, authorization, job reception, accepted shares, and rejection reasons.
- Transparent performance metrics: report local hashrate, difficulty-weighted work, pool estimates, and data timestamps separately.
- Keep POOL and pool-based SOLO modes distinct, with phased integration of block discovery, maturity, and payout tracking.
- Traditional Chinese and English user interfaces, local settings, and previewable diagnostic exports.

These features are not claimed to be individually novel. The intended differentiation is PEPEPOW-specific integration, native usability, and verifiable reporting.

## Documentation

- [Documentation index and next steps](docs/README.md): research document locations, the G0 validation entry point, and current documentation status.
- [Architecture decisions](docs/ARCHITECTURE.md): core, GUI, GPU, and job-lifecycle choices.
- [Research verification ledger](docs/RESEARCH_LEDGER.md): pinned source SHAs, confirmed differences, and outstanding questions.
- [Product development plan (03)](docs/research/03_PRODUCT_DEVELOPMENT_PLAN.md): differentiation, technical assessment, milestones, and acceptance criteria.
- [AI coding agent instructions](AGENTS.md): required reading, G0 priorities, and evidence standards.

## First milestone

Complete [G0 technical validation](docs/G0_TECHNICAL_VALIDATION.md) to confirm that the HooHash V110 implementation matches PEPEPOW consensus before treating the Stratum CPU CLI, desktop UI, and GPU backends as supported.

Passing G0 requires full 32-byte digest comparisons, at least 10,000 header/nonce test cases, real on-chain headers, and native Windows/Linux build and execution evidence. A test scaffold or a single passing test case does not satisfy this gate.

## Architecture decisions

| Layer | Decision | Status |
| --- | --- | --- |
| G0 hash harness | C reference adapter + CMake | Planned approach adopted; licensing and parity still need verification |
| G1 host / Stratum / scheduling | C++20; immutable job snapshots | Planned approach adopted; not yet accepted as a complete implementation |
| G2 desktop UI | Qt 6 + C++, sharing a controller; headless core independent of Qt | Conditional first choice; packaging and licensing spike pending |
| Configuration and events | Versioned JSON / JSONL | Planned approach adopted; schema pending G1 |
| NVIDIA / AMD | CUDA first; choose HIP or OpenCL for AMD based on actual hardware | Separate G4/G5 gates; not validated |
| Rust/Tauri, SQLite, service/API | Alternatives or later-stage requirements | Not required for the initial release |

Follow the [architecture decisions](docs/ARCHITECTURE.md); do not present a technology choice as implemented support. The native Windows target must not depend on WSL.

## Roadmap and evidence

See the [roadmap](docs/ROADMAP.md). Milestone status must be based on actual test evidence, not estimated progress.

## User trust

Only a payout address should be requested; the miner must not handle seed phrases or private keys. Mining must start only after explicit user action. Startup-on-boot and background operation must be visible and user-controllable. Telemetry and remote control are disabled by default in the initial design.

Zero developer fees for the first-party core remain a proposal, not a final decision. A release must disclose miner fees and pool fees separately. Upstream and dependency licenses must be preserved and reviewed file by file before selecting a redistribution license.

Planning baseline: feasibility review dated 2026-11-04. External project versions, compatibility, and performance must be rechecked during implementation.
