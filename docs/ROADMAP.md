# Development roadmap

Updated 2026-11-28 (Asia/Taipei). Main snapshot `dfccd6fd2aeb3525cc41e27428537571642da6b7` includes PR #30 and CUDA PRs #31–#33, plus the formal G0 evidence reconciliation. The Owner's latest direction separates G0 technical validation from distribution license compliance and accepts the cited MIT HooHash evidence for local development. See the [development status](DEVELOPMENT_STATUS.md), [CUDA Stratum integration](CUDA_STRATUM_INTEGRATION.md), [G0 evidence reconciliation](VALIDATION_EVIDENCE/G0/formal-g0-audit-2026-11-26.md), and [distribution notices](../THIRD_PARTY_NOTICES.md).

The local-only CPU CLI includes bounded transport deadlines, reconnect and same-mode loopback failover, and an opt-in worker pool. The integrated benchmark supports 1–256 workers while retaining one worker by default. Its local Windows evidence is in the [UCRT64 CPU report](VALIDATION_EVIDENCE/G1/cpu-benchmark-windows11-ucrt64-gcc-2026-11-25.md). Formal G0 technical sign-off and G1 acceptance remain incomplete; G0 is not gated on distribution review. Windows 11 passed the recorded 84/84 acceptance and Windows 10 remains Owner equivalence without a separate physical run.

| Gate | Verifiable deliverable | Acceptance criteria | Status at this snapshot |
| --- | --- | --- | --- |
| G0 Technical validation | Pinned sources, full-digest CPU harness, fixed vectors and native platform evidence | At least 10,000 independently checked full-digest cases and required native Windows/Linux evidence | Existing CPU parity and platform evidence are recorded; formal technical checklist reconciliation and sign-off remain pending. Distribution license compliance is tracked separately and does not block technical G0. |
| G1 CPU CLI | Stratum, job and target, nonce, reconnect, JSON events | Actual accepted shares on authorized platforms, passing protocol and fault fixtures | Bounded CPU CLI, worker pool, loopback, benchmark, accounting, and stale-job tests are integrated. Draft PR #40 records one bounded Windows 11 CPU/POOL run with 1,000 answered replies (992 accepted, 8 generic rejects, 0 Pool errors); broader G1 acceptance remains pending. |
| G2 Desktop MVP | Start/stop, manual resource profiles, diagnostics, settings, system tray, Traditional Chinese and English | Clean install, sleep and disconnect recovery, 24h observation per OS | Not started |
| G3 Beta release | Packaging, checksums, sources and licenses, instructions, support matrix | Reproducible build, no sensitive data in configuration or diagnostics | Not started |
| G4 NVIDIA | CUDA, monitoring, CPU recheck for every candidate | Windows/Linux parity, actual shares, 24h driver observation | Draft PRs #35–#41 provide an experimental full-digest bounded search and opt-in Stratum process bridge. A native Windows RTX 3060 local Stratum loopback passed with full-digest CPU recheck; broad platform parity, live shares, and full G4 acceptance remain incomplete. |
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
| G0-01 / Issue #1 | None | Pinned source manifest, independent full KAT, C/CMake harness, nonzero exit on last-byte mutation, reproducible commands | Technical evidence is integrated; reconcile current evidence against the G0 checklist and record the technical sign-off. Distribution notices are handled separately. |
| G0-02 | G0-01 | At least 10,000 cases, all low eight nonce bits covered, cache and FP64 diagnostics, native Windows 11 and Ubuntu 22.04/24.04 evidence | Independent 10,000-case parity plus Ubuntu 22.04/24.04, Windows Server Debug/Release and preserved Windows 11 acceptance are recorded. Reconcile the technical evidence checklist; distribution compliance is separate. |
| G1-01 | G0 passed | Fixed-dialect fixtures, header and target, independent mock, job/difficulty races, fragmented and multiple JSON frames, oversized and malformed input, error shapes, TLS failure where applicable | Transport deadlines, subscribe/authorize/submit timeouts, disconnect cleanup, stale response rejection, and loopback success are covered. Linux and Windows Server 2022 Debug/Release CI pass. Owner Windows 11 acceptance passed 84/84 and Windows 10 is accepted by equivalence without a separate physical run; broader authorized protocol interoperability and formal G1 acceptance remain pending. |
| G1-02 | G1-01 | CPU CLI, bounded reconnect and same-mode failover, JSONL evidence, at least 1,000 answered shares per major OS/mode configuration under explicit authorization and resource limits | Bounded retries, interruptible exponential backoff, fresh handshake, same-mode loopback failover, session generations, stale-work rejection, and duplicate-submit prevention are implemented and locally tested. Draft PR #40 records one bounded Windows 11 CPU/POOL configuration with 1,000 answered replies; other required platform/mode evidence and formal G1 acceptance remain pending. Issue #1 remains open. |
| G2-00 | G1 accepted | Native Qt packaging and licensing spike. Evaluate Tauri under the same criteria if Qt fails. Record selection evidence. | Not started |
| G2-01 | G2-00 | Minimal wizard, start/stop, readiness, Share Doctor, system tray, diagnostics preview, Traditional Chinese and English, 24h on each of the three target OS configurations | Not started |
| G3-01 | G2 passed | Reproducible packaging, install/uninstall, checksums, SBOM and notices, support matrix, sensitive-data checks, accurate signature status | Not started |
| G4-01 / G5-01 | G0/G1 passed, GPU toolchains and hardware | CUDA single-GPU first, AMD design ADR, full parity per OS/device/driver, CPU verification for every candidate, actual shares, 24h observation | PR #41 adds a CPU-tested bounded-search process bridge, full-digest CPU candidate recheck, job cancellation/failover loopbacks, and a directly tested live Stratum nonce cursor with UINT32_MAX exhaustion and atomic extranonce2 overflow handling. CUDA matrix/hash parity and full G4 acceptance remain pending. AMD design remains not started. |

G1 target fixtures must account for pool float division and clamping versus the floor formula in research. Test target−1, target, and target+1. Do not assume the Bitcoin baseline. Keep mock byte-order and target helpers independent from the miner under test. Each gate PR records source SHAs, commands, raw logs, negative results, and pending limitations.

G3 is Beta. State verified configurations and gaps. Report 02's **72h final soak** is a stable-release gate. Test actual release configurations and check invalid shares, memory trends, and fault recovery. Track 24h and 72h evidence separately. Claim a signed release only with signature evidence.

## Execution

Recheck current main and repository state each round. Choose one verifiable deliverable and report passed, failed, and pending results with evidence. Plans and scaffolding do not complete acceptance.

Proceed G0 → G1 → G2 → G3, with separate GPU gates. Resolve independently diagnosable issues and record specific blockers involving hardware, external access, or product decisions.

## Owner decisions

The [2026-11-26 Issue #1 decision](https://github.com/edisontw/pepepow-miner/issues/1#issuecomment-7192595038) authorized PR #30 and PRs #31–#33 subject to review and CI; those PRs were integrated. The Owner's 2026-11-28 development direction accepts the cited MIT HooHash evidence as the current local-development basis and separates technical G0 from package distribution compliance. Draft PR #40 documents one bounded Windows 11 CPU/POOL run; it is supplemental historical evidence and does not establish G1 acceptance or authorize another live test. Further pool/share testing and release remain subject to their applicable gates. Issue #1 remains open; no release is claimed here.

The 2026-11-25 platform decisions remain in effect:

- PR #22, #24, and #25 are approved for integration after their respective review and validation gates; all three are now merged. PR #24 was revalidated after #22 merged, and PR #25 after #24 merged.
- Windows 11 physical technical acceptance passed 84/84 on the previously validated main head. Windows 10 is PASS by Owner equivalence; no separate physical Windows 10 execution was performed. This is not new device acceptance for the combined worktree.
- Preserve original license texts, historical GPL headers, MIT notices, source pins, checksums, and provenance as described in [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md); prepare package-specific notices before distribution.
- Production pool connections, real-share testing, and binary release are now authorized after their applicable gates pass; none is represented as completed.

The [G0 validation plan](G0_TECHNICAL_VALIDATION.md) defines the source and full KAT work.
