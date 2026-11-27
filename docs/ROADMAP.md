# Development roadmap

Updated 2026-11-27 (Asia/Taipei). Current `main` is `dfccd6fd2aeb3525cc41e27428537571642da6b7`; PR #30 integrated #27–#29 with merge ancestry, and CUDA PRs #31–#33 were merged in dependency order. Main push validation passed the CUDA device-probe, correctness-contract, and BLAKE3-stage workflows. PRs #35–#37 are open and stacked; the bounded-search draft PR #38 is based on #37 and its exact-head Actions passed 4/4. See the [development status](DEVELOPMENT_STATUS.md), [formal G0 evidence reconciliation](VALIDATION_EVIDENCE/G0/formal-g0-technical-validation-2026-11-27.md), [G1 implementation evidence](VALIDATION_EVIDENCE/G1/transport-reconnect-cpu-workers-2026-11-25.md), [CPU benchmark report](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-ucrt64-gcc-2026-11-25.md), and [distribution compliance gate](../THIRD_PARTY_NOTICES.md).

The current CPU CLI includes bounded transport deadlines, reconnect and same-mode loopback failover, and an opt-in worker pool; it explicitly rejects public endpoints. The integrated benchmark supports 1–256 workers while retaining one worker by default. Its local Windows regression, benchmark, and stability evidence is in the [UCRT64 CPU report](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-ucrt64-gcc-2026-11-25.md). G0 Technical Validation is PASS. Distribution License Compliance is a separate, pending pre-release gate and does not block local builds or G0. G1 live acceptance is authorized. The Owner confirmed the Pool Mining endpoint `stratum+tcp://pool.pepepow.net:39333` and supplied the test payout address; live testing remains pending implementation of bounded public-session support and evidence capture. The previously recorded Windows 11 acceptance passed 84/84 and Windows 10 remains Owner equivalence without a separate physical run.

| Gate | Verifiable deliverable | Acceptance criteria | Status at this snapshot |
| --- | --- | --- | --- |
| G0 Technical Validation | Pinned CPU hash harness, full digest vectors, reproducible source/build provenance, and native reports | At least 10,000 independently checked full-digest cases, negative controls, and required native Windows/Linux technical evidence | **PASS.** Linux 22.04/24.04 Debug/Release and full-digest evidence are recorded; Windows 11 physical acceptance is 84/84, with Windows 10 accepted by Owner equivalence and no separate physical run. See the [formal G0 technical sign-off](VALIDATION_EVIDENCE/G0/formal-g0-technical-validation-2026-11-27.md). |
| Distribution License Compliance | Exact-package license texts, notices, provenance, SBOM, and runtime requirements | Complete applicable notices/SBOM for each exact source or binary package before distribution | **Pending; separate from G0 Technical Validation.** HooHash development uses the public Hoosat MIT commit and pinned Core MIT headers as its current basis; preserve the previous GPL headers and transition history. Package-specific notices remain before release. See [distribution compliance](../THIRD_PARTY_NOTICES.md). |
| G1 CPU CLI | Stratum, job and target, nonce, reconnect, JSON events | Actual accepted shares on authorized platforms, passing protocol and fault fixtures | Bounded CPU CLI, worker pool, loopback, benchmark, accounting, and stale-job tests are integrated. Live acceptance is authorized; endpoint/mode and test address are confirmed. First implement bounded public-session support, then capture live evidence under explicit CPU/time caps. |
| G2 Desktop MVP | Start/stop, manual resource profiles, diagnostics, settings, system tray, Traditional Chinese and English | Clean install, sleep and disconnect recovery, 24h observation per OS | Not started |
| G3 Beta release | Packaging, checksums, sources and licenses, instructions, support matrix | Reproducible build, no sensitive data in configuration or diagnostics | Not started |
| G4 NVIDIA | CUDA, monitoring, CPU recheck for every candidate | Windows/Linux parity, actual shares, 24h driver observation | Main has the device probe, correctness harness, and BLAKE3 stage. PRs #35–#37 add pinned-Core PRNG/matrix and experimental full-digest parity; draft PR #38 adds offline bounded search with CPU candidate recheck. Matrix/search work remains experimental; G4 is NOT PASSED. |
| G5 AMD | OpenCL/HIP design and device matrix | Per-platform and per-device acceptance | Not started |

## G1 protocol acceptance

- Verify header80 fields and byte order, Stratum prevhash conversion, nonce and extranonce ranges, and uniqueness.
- Verify the PEPEPOW difficulty baseline, inclusive target boundaries, and the target associated with each job.
- Verify the lifecycle for set_difficulty, clean_jobs, previous and duplicate jobs, disconnect, and reconnect.
- Preserve the source of all replies and rejection categories. An accepted pool reply does not prove block discovery or payment.
- Fail over only within the same mode. A POOL disconnect must never switch to SOLO or change the payout address.
- Report difficulty-weighted accepted work separately from local hashrate, with sample size and duration.
- Require zero invalid and low-difficulty shares in controlled tests. Record live stale shares separately.
- Collect at least 1,000 answered shares for each major configuration before assessing rejection rates. A controlled low-difficulty environment is acceptable for low hashrate.
- Mining requires explicit user initiation and bounded work. Mainnet block discovery is outside timed acceptance.

## First-release scope

Include native CPU operation, CLI and GUI, Stratum POOL and pool-based SOLO, payout address checking, manual resource profiles, same-mode reconnect and basic failover, and event-driven diagnostics. Collect separate interoperability evidence for each POOL or SOLO endpoint and mark unverified modes unavailable.

G2 scope excludes automatic activity detection, numeric health scores, automatic efficiency tuning, profit tracking, SQLite, HTTP APIs, and always-on services. Research report 03 does not expand first-release scope.

Defer direct wallet/RPC SOLO, GPU clock and voltage controls, automated trading, multi-coin support, remote writes, and cloud farm control.

## Work packages

These are work-package IDs. Use [Issue #1](https://github.com/edisontw/pepepow-miner/issues/1) for the first deliverable.

| ID | Dependency | Deliverable and acceptance | Status at this snapshot |
| --- | --- | --- | --- |
| G0-01 / Issue #1 | None | Pinned CPU reference, independent full KAT, C/CMake harness, nonzero exit on last-byte mutation, reproducible technical provenance | Complete. Distribution notices are a separate gate and do not block technical G0. See [Issue #1](https://github.com/edisontw/pepepow-miner/issues/1) and the [formal G0 technical sign-off](VALIDATION_EVIDENCE/G0/formal-g0-technical-validation-2026-11-27.md). |
| G0-02 | G0-01 | At least 10,000 cases, all low eight nonce bits covered, cache and FP64 diagnostics, native Windows 11 and Ubuntu 22.04/24.04 evidence | Complete. The independent parity corpus, Ubuntu 22.04/24.04 Debug/Release, Windows Server regression, Windows 11 84/84, and Windows 10 Owner equivalence are recorded. G0 Technical Validation is PASS. |
| G1-01 | G0 passed | Fixed-dialect fixtures, header and target, independent mock, job/difficulty races, fragmented and multiple JSON frames, oversized and malformed input, error shapes, TLS failure where applicable | Transport deadlines, subscribe/authorize/submit timeouts, disconnect cleanup, stale response rejection, and loopback success are covered. Linux and Windows Server 2022 Debug/Release CI pass. Owner Windows 11 acceptance passed 84/84 and Windows 10 is accepted by equivalence without a separate physical run; broader authorized protocol interoperability and formal G1 acceptance remain pending. |
| G1-02 | G1-01 | CPU CLI, bounded reconnect and same-mode failover, JSONL evidence, at least 1,000 answered shares per major OS/mode configuration under explicit authorization and resource limits | Bounded retries, interruptible exponential backoff, fresh handshake, same-mode loopback failover, session generations, stale-work rejection, and duplicate-submit prevention are implemented and locally tested. Owner confirmed `stratum+tcp://pool.pepepow.net:39333` (Pool Mining) and supplied a test address; public DNS endpoint support and bounded live evidence remain to be implemented. |
| G2-00 | G1 accepted | Native Qt packaging and licensing spike. Evaluate Tauri under the same criteria if Qt fails. Record selection evidence. | Not started |
| G2-01 | G2-00 | Minimal wizard, start/stop, readiness, Share Doctor, system tray, diagnostics preview, Traditional Chinese and English, 24h on each of the three target OS configurations | Not started |
| DIST-01 | Exact release configuration | Generate and review applicable license texts, notices, provenance, runtime requirements, and SBOM for each source/binary package | Pending until a package is proposed; does not gate local development, offline testing, or G0 Technical Validation. |
| G3-01 | G2 passed | Reproducible packaging, install/uninstall, checksums, SBOM and notices, support matrix, sensitive-data checks, accurate signature status | Not started |
| G4-01 / G5-01 | G0/G1 passed, GPU toolchains and hardware | CUDA single-GPU first, AMD design ADR, full parity per OS/device/driver, CPU verification for every candidate, actual shares, 24h observation | Main contains the device probe, correctness harness, and BLAKE3 stage. PRs #35–#37 and draft #38 extend offline CUDA coverage; G4 remains NOT PASSED. AMD design remains not started. |

G1 target fixtures must account for pool float division and clamping versus the floor formula in research. Test target−1, target, and target+1. Do not assume the Bitcoin baseline. Keep mock byte-order and target helpers independent from the miner under test. Each gate PR records source SHAs, commands, raw logs, negative results, and pending limitations.

G3 is Beta. State verified configurations and gaps. Report 02's **72h final soak** is a stable-release gate. Test actual release configurations and check invalid shares, memory trends, and fault recovery. Track 24h and 72h evidence separately. Claim a signed release only with signature evidence.

## Execution

Recheck current main and repository state each round. Choose one verifiable deliverable and report passed, failed, and pending results with evidence. Plans and scaffolding do not complete acceptance.

Proceed G0 → G1 → G2 → G3, with separate GPU gates. Resolve independently diagnosable issues and record specific blockers involving hardware, external access, or product decisions.

## Owner decisions

The Owner decisions of 2026-11-26 are recorded in [Issue #1 comment 7192595038](https://github.com/edisontw/pepepow-miner/issues/1#issuecomment-7192595038). PR #30 was authorized and merged after review/CI; PRs #31–#33 were authorized and merged in dependency order. G0 Technical Validation sign-off, G1 pool/share testing, and release after applicable gates are authorized. Owner approval does not replace exact-package notices or third-party requirements. The Owner subsequently confirmed the LAB Pool Pool Mining endpoint and provided the test payout address. Issue #1 remains open; no live pool test or release is claimed here.

The 2026-11-25 platform decisions remain in effect:

- PR #22, #24, and #25 are approved for integration after their respective review and validation gates; all three are now merged. PR #24 was revalidated after #22 merged, and PR #25 after #24 merged.
- Windows 11 physical technical acceptance passed 84/84 on the previously validated main head. Windows 10 is PASS by Owner equivalence; no separate physical Windows 10 execution was performed. This is not new device acceptance for the combined worktree.
- Use the current development and distribution policy in [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md): the public Hoosat MIT evidence is the HooHash development basis, with the older GPL headers and MIT transition retained as provenance; exact-package notices remain before distribution.
- Production pool connections, real-share testing, and binary release are now authorized after their applicable gates pass; none is represented as completed.

The [G0 validation plan](G0_TECHNICAL_VALIDATION.md) defines the source and full KAT work.
