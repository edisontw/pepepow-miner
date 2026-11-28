# PEPEPOW Miner

**A native HooHash V110 miner for PEPEPOW, designed for everyday computers.**

PEPEPOW Miner aims to make mining on Windows and Linux easier to start, understand, and control. The focus is on reliable CPU mining, a straightforward desktop experience, transparent performance reporting, and responsible use of system resources.

[![Status: In development](https://img.shields.io/badge/status-in%20development-orange)](docs/DEVELOPMENT_STATUS.md)
![Algorithm: HooHash V110](https://img.shields.io/badge/algorithm-HooHash%20V110-blue)
![Target: Windows and Linux](https://img.shields.io/badge/target-Windows%20%7C%20Linux-lightgrey)

[PEPEPOW website](https://pepepow.net/) · [Development progress](docs/DEVELOPMENT_STATUS.md) · [Report an issue](https://github.com/edisontw/pepepow-miner/issues)

> **Pre-release:** This project is currently an offline testing prototype. It is not ready for live mining, and no supported downloadable miner is available yet.

## What makes it different?

The following features describe the intended product, not functionality already available.

- **Native Windows and Linux support** — run directly on the operating system, without requiring WSL on Windows.
- **Simple mining controls** — a desktop interface and command-line mode with clear start/stop controls and manual CPU resource profiles.
- **Useful diagnostics** — distinguish pool connection, authorization, job reception, accepted shares, and rejection reasons.
- **Transparent statistics** — separate local hashrate from pool estimates and difficulty-weighted accepted work.
- **Flexible mining modes** — pool mining and pool-based solo mining, with explicit selection and predictable reconnect behavior.
- **Accessible interface** — English and Traditional Chinese, local settings, and diagnostic reports that users can review before sharing.

## Development progress

| Component | Status |
| --- | --- |
| HooHash V110 validation | 10,000-case full-digest and native platform evidence recorded; formal G0 technical sign-off is pending checklist reconciliation. Distribution license review is a separate pre-release gate. |
| CPU mining engine | Bounded offline nonce search implemented |
| Pool connectivity | Offline and local Stratum testing; authorized live acceptance has not been run |
| Windows 10/11 and Linux desktop miner | Planned; full platform acceptance pending |
| NVIDIA GPU (CUDA) | Offline device probe and partial correctness stages exist; full HooHash GPU digest parity is not implemented |
| AMD GPU (HIP or OpenCL) | Planned |

CPU support comes first. GPU acceleration and desktop packaging will follow separate validation stages. See the [roadmap](docs/ROADMAP.md) for the full plan.

## Download and testing

There is no supported public binary release at this stage. The repository currently provides source code and developer tools for offline validation only.

Developers can find reproducible build and offline test instructions in the [CLI tools guide](tools/README.md). Do not connect these experimental tools to a live pool or use them for production mining.

## User privacy and trust

The intended miner will require only a public payout address, never a wallet seed phrase or private key. Mining will start only with explicit user action. Background operation and startup settings will be visible and user-controlled, with telemetry and remote control disabled by default.

Zero developer fees are a proposal, not a confirmed release policy. Any future release will disclose miner fees and pool fees separately.

## Contributing and source licensing

Bug reports, reproducible test results, and contributions based on appropriately licensed public sources are welcome. Please use [GitHub Issues](https://github.com/edisontw/pepepow-miner/issues) for feedback.

The project uses PEPEPOW Core as its consensus reference and validates behavior against an independent implementation. Third-party attribution and license requirements must be respected. Source and dependency redistribution review remains in progress; no final repository-wide redistribution license is asserted here. See the [source provenance and license assessment](docs/sources/G0_PROVENANCE_LICENSE_ASSESSMENT.md) for details.

## Developer resources

- [Documentation index](docs/README.md) — technical guides and research
- [Development status](docs/DEVELOPMENT_STATUS.md) — latest recorded evidence and outstanding work
- [Architecture](docs/ARCHITECTURE.md) and [roadmap](docs/ROADMAP.md) — implementation and validation plans
- [Agent and contributor instructions](AGENTS.md) — development workflow, testing, and safety requirements
