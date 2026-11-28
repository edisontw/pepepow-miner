# Development roadmap

Updated 2026-11-28 (Asia/Taipei). This status snapshot uses main at `634146739c154c0cfb7558991a6e3436e9b2dff2`, before PR #41 integration. PR #39 recorded G0 Technical Validation PASS and was merged as `ad432a7f6bbe640cc1e645f258691fff8151b8c4`. PRs #35–#38 and #40 are integrated with normal merge ancestry: CUDA correctness and bounded offline search, plus the opt-in bounded CPU live-session CLI. The CUDA Stratum bridge is in PR #41 and has physical Windows 11 RTX 3060 loopback evidence; no live GPU share is recorded. See the [development status](DEVELOPMENT_STATUS.md), [formal G0 evidence](VALIDATION_EVIDENCE/G0/formal-g0-technical-validation-2026-11-27.md), [historical CPU/POOL report](VALIDATION_EVIDENCE/G1/windows11-cpu-pool-acceptance-2026-11-27.md), [CPU protocol evidence](VALIDATION_EVIDENCE/G1/transport-reconnect-cpu-workers-2026-11-25.md), [CPU stability report](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-ucrt64-gcc-2026-11-25.md), [CUDA Stratum loopback evidence](VALIDATION_EVIDENCE/G4/cuda-stratum-loopback-windows11-2026-11-28.md), and [distribution compliance gate](../THIRD_PARTY_NOTICES.md).

Main includes an explicitly started, bounded CPU live-session CLI. Its one Windows 11 CPU/POOL run is historical: 1,000 answered replies, 992 accepted, 8 generic rejected replies, and 0 Pool errors. Preserve this record; do not repeat that test. G1 remains NOT PASSED while Linux, SOLO, live reconnect/failover, and other required configurations remain pending. The CLI has one worker by default and supports bounded worker selection. G0 Technical Validation is PASS; Distribution License Compliance is a separate, pending pre-release gate. The Owner-authorized payout configuration remains local and is omitted from repository evidence. Windows 11 physical acceptance remains 84/84 on its documented head; Windows 10 is Owner equivalence without a separate physical run.

| Gate | Verifiable deliverable | Acceptance criteria | Status at this snapshot |
| --- | --- | --- | --- |
| G0 Technical Validation | Pinned CPU hash harness, full digest vectors, reproducible source/build provenance, and native reports | At least 10,000 independently checked full-digest cases, negative controls, and required native Windows/Linux technical evidence | **PASS.** Linux 22.04/24.04 Debug/Release and full-digest evidence are recorded; Windows 11 physical acceptance is 84/84, with Windows 10 accepted by Owner equivalence and no separate physical run. See the [formal G0 technical sign-off](VALIDATION_EVIDENCE/G0/formal-g0-technical-validation-2026-11-27.md). |
| Distribution License Compliance | Exact-package license texts, notices, provenance, SBOM, and runtime requirements | Complete applicable notices/SBOM for each exact source or binary package before distribution | **Pending; separate from G0 Technical Validation.** HooHash development uses the public Hoosat MIT commit and pinned Core MIT headers as its current basis; preserve the previous GPL headers and transition history. Package-specific notices remain before release. See [distribution compliance](../THIRD_PARTY_NOTICES.md). |
| G1 CPU CLI | Stratum, job and target, nonce, reconnect, JSON events | Actual accepted shares on authorized platforms, passing protocol and fault fixtures | Main has bounded loopback support and an opt-in bounded public-session CLI. Preserve the historical Windows 11 CPU/POOL run (1,000 replies: 992 accepted, 8 generic rejected). G1 remains NOT PASSED while Linux, SOLO, live reconnect/failover, and other required configurations remain unvalidated. |
| G2 Desktop MVP | Start/stop, manual resource profiles, diagnostics, settings, system tray, Traditional Chinese and English | Clean install, sleep and disconnect recovery, 24h observation per OS | Not started |
| G3 Beta release | Packaging, checksums, sources and licenses, instructions, support matrix | Reproducible build, no sensitive data in configuration or diagnostics | Not started |
| G4 NVIDIA | CUDA, monitoring, CPU recheck for every candidate | Windows/Linux parity, actual shares, 24h driver observation | Main includes staged CUDA correctness/search work and a Stratum bridge with native RTX 3060 local loopback evidence. No real GPU pool shares are recorded; Linux native GPU acceptance, actual shares, and G4 sign-off remain pending. |
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
| G1-02 | G1-01 | CPU CLI, bounded reconnect and same-mode failover, JSONL evidence, at least 1,000 answered shares per major OS/mode configuration under explicit authorization and resource limits | Main has bounded loopback retries/failover and the opt-in public CPU CLI. The historical Windows 11 CPU/POOL run recorded 1,000 replies (992 accepted, 8 unclassified generic rejections) under explicit limits. Live reconnect/failover and other OS/mode configurations remain pending; G1 is NOT PASSED. See the [sanitized report and JSONL evidence](VALIDATION_EVIDENCE/G1/windows11-cpu-pool-acceptance-2026-11-27.md).
| G2-00 | G1 accepted | Native Qt packaging and licensing spike. Evaluate Tauri under the same criteria if Qt fails. Record selection evidence. | Not started |
| G2-01 | G2-00 | Minimal wizard, start/stop, readiness, Share Doctor, system tray, diagnostics preview, Traditional Chinese and English, 24h on each of the three target OS configurations | Not started |
| DIST-01 | Exact release configuration | Generate and review applicable license texts, notices, provenance, runtime requirements, and SBOM for each source/binary package | Pending until a package is proposed; does not gate local development, offline testing, or G0 Technical Validation. |
| G3-01 | G2 passed | Reproducible packaging, install/uninstall, checksums, SBOM and notices, support matrix, sensitive-data checks, accurate signature status | Not started |
| G4-01 / G5-01 | G0 passed, GPU toolchains and hardware; retain mode-specific CPU prerequisites | CUDA single-GPU first, AMD design ADR, full parity per OS/device/driver, CPU verification for every candidate, actual shares, stability observation | CUDA offline coverage and Windows RTX 3060 Stratum loopback are recorded. Real CUDA POOL/SOLO shares, cross-platform native CUDA parity, and G4 acceptance remain pending. AMD is deferred. |

G1 target fixtures must account for pool float division and clamping versus the floor formula in research. Test target−1, target, and target+1. Do not assume the Bitcoin baseline. Keep mock byte-order and target helpers independent from the miner under test. Each gate PR records source SHAs, commands, raw logs, negative results, and pending limitations.

G3 is Beta. State verified configurations and gaps. Report 02's **72h final soak** is a stable-release gate. Test actual release configurations and check invalid shares, memory trends, and fault recovery. Track 24h and 72h evidence separately. Claim a signed release only with signature evidence.

## Execution

Recheck current main and repository state each round. Choose one verifiable deliverable and report passed, failed, and pending results with evidence. Plans and scaffolding do not complete acceptance.

Proceed G0 → G1 → G2 → G3, with separate GPU gates. Resolve independently diagnosable issues and record specific blockers involving hardware, external access, or product decisions.

## Owner decisions

The current Owner approval is recorded in [Issue #1 comment 7239984084](https://github.com/edisontw/pepepow-miner/issues/1#issuecomment-7239984084). It authorizes PRs #35–#41 to merge after review, exact-head checks, and applicable validation; G0 sign-off is complete. It also authorizes bounded CPU/NVIDIA POOL/SOLO tests after G0, using the existing local payout configuration and verified endpoints. No real CUDA pool shares are yet recorded. Preserve the historical CPU result and omit payout data from repository evidence. Issue #1 remains open; no release is claimed.

The 2026-11-25 platform decisions remain in effect:

- PR #22, #24, and #25 are approved for integration after their respective review and validation gates; all three are now merged. PR #24 was revalidated after #22 merged, and PR #25 after #24 merged.
- Windows 11 physical technical acceptance passed 84/84 on the previously validated main head. Windows 10 is PASS by Owner equivalence; no separate physical Windows 10 execution was performed. This is not new device acceptance for the combined worktree.
- Use the current development and distribution policy in [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md): the public Hoosat MIT evidence is the HooHash development basis, with the older GPL headers and MIT transition retained as provenance; exact-package notices remain before distribution.
- Production pool connections, real-share testing, and binary release are now authorized after their applicable gates pass; none is represented as completed.

The [G0 validation plan](G0_TECHNICAL_VALIDATION.md) defines the source and full KAT work.
