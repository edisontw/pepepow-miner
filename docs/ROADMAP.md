# Development roadmap

Updated 2026-11-24 (Taiwan time), following the [PR #21 G0 source inventory merge](https://github.com/edisontw/pepepow-miner/pull/21) and the Linux pinned-Core offline CPU miner integration. For current evidence and open PRs, consult the [concise development handoff](DEVELOPMENT_STATUS.md).

The pinned Core harness, independent Python V110 prototype, uint256 comparator, and offline CPU search/Stratum replay now exist on main. Native Windows 11 Debug/Release technical evidence is recorded in [the validation report](VALIDATION_EVIDENCE/G0/windows11-native-g0-g1-2026-11-24.md); platform interoperability, formal G0/G1 acceptance, and approved live pool integration remain unverified.

| Gate | Verifiable deliverable | Acceptance criteria | Status at this snapshot |
| --- | --- | --- | --- |
| G0 Technical validation | Source pins, license inventory, CPU hash harness, vectors and report | At least 10,000 independently checked full-digest cases and required native Windows/Linux evidence | Linux, Windows Server, and Windows 11 Debug/Release technical parity evidence available; current Core blobs and Windows OpenLibm translation units inventoried; legal/NOTICE review and formal Owner sign-off pending |
| G1 CPU CLI | Stratum, job and target, nonce, reconnect, JSON events | Actual accepted shares on authorized platforms, passing protocol and fault fixtures | Local-only TCP CLI now joins the fixed-dialect job builder, pinned-Core CPU search, target check, mock submission, response accounting, job-replacement cancellation, and stale-candidate accounting; Linux Release and Windows 11 Debug/Release offline evidence pass. See [Linux evidence](VALIDATION_EVIDENCE/G1/offline-loopback-cpu-miner-2026-11-24.md) and [Windows evidence](VALIDATION_EVIDENCE/G0/windows11-native-g0-g1-2026-11-24.md). G0, reconnect/failover, broader protocol acceptance, authorized interoperability, and formal acceptance remain pending |
| G2 Desktop MVP | Start/stop, manual resource profiles, diagnostics, settings, system tray, Traditional Chinese and English | Clean install, sleep and disconnect recovery, 24h observation per OS | Not started |
| G3 Beta release | Packaging, checksums, sources and licenses, instructions, support matrix | Reproducible build, no sensitive data in configuration or diagnostics | Not started |
| G4 NVIDIA | CUDA, monitoring, CPU recheck for every candidate | Windows/Linux parity, actual shares, 24h driver observation | Not started |
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
| G0-01 / Issue #1 | None | Source and license manifest, independent full KAT, C/CMake harness, nonzero exit on last-byte mutation, actual commands | Current Core source blobs are reconciled; Windows OpenLibm translation units and header signals are recorded in PR #21. Redistribution decisions and final notices remain pending. |
| G0-02 | G0-01 | At least 10,000 cases, all low eight nonce bits covered, cache and FP64 diagnostics, native Windows 11 and Ubuntu 22.04/24.04 evidence | Linux, Windows Server, and Windows 11 x64 Debug/Release full-digest technical parity is recorded. Per-file source provenance and license decisions plus Owner sign-off remain pending. |
| G1-01 | G0 passed | Fixed-dialect fixtures, header and target, independent mock, job/difficulty races, fragmented and multiple JSON frames, oversized and malformed input, error shapes, TLS failure where applicable | Linux and Windows 11 native local mock job-to-search-to-submit flows verify full header/digest, replacement cancellation, mock accepted/rejected replies, and stale accounting with pinned Core. Bounded Windows fault fixtures also cover connection/response failures. G0 prerequisite, reconnect/failover, broader protocol acceptance, and authorized platform coverage remain pending. |
| G1-02 | G1-01 | CPU CLI, bounded reconnect and same-mode failover, JSONL evidence, at least 1,000 answered shares per major OS/mode configuration under explicit authorization and resource limits | Offline-only loopback CLI prototype started. Reconnect/failover and authorized endpoint/platform share evidence have not started. |
| G2-00 | G1 passed | Native Qt packaging and licensing spike. Evaluate Tauri under the same criteria if Qt fails. Record selection evidence. | Not started |
| G2-01 | G2-00 | Minimal wizard, start/stop, readiness, Share Doctor, system tray, diagnostics preview, Traditional Chinese and English, 24h on each of the three target OS configurations | Not started |
| G3-01 | G2 passed | Reproducible packaging, install/uninstall, checksums, SBOM and notices, support matrix, sensitive-data checks, accurate signature status | Not started |
| G4-01 / G5-01 | G0/G1 passed, GPU toolchains and hardware | CUDA single-GPU first, AMD design ADR, full parity per OS/device/driver, CPU verification for every candidate, actual shares, 24h observation | Not started |

G1 target fixtures must account for pool float division and clamping versus the floor formula in research. Test target−1, target, and target+1. Do not assume the Bitcoin baseline. Keep mock byte-order and target helpers independent from the miner under test. Each gate PR records source SHAs, commands, raw logs, negative results, and pending limitations.

G3 is Beta. State verified configurations and gaps. Report 02's **72h final soak** is a stable-release gate. Test actual release configurations and check invalid shares, memory trends, and fault recovery. Track 24h and 72h evidence separately. Claim a signed release only with signature evidence.

## Execution

Recheck current main and repository state each round. Choose one verifiable deliverable and report passed, failed, and pending results with evidence. Plans and scaffolding do not complete acceptance.

Proceed G0 → G1 → G2 → G3, with separate GPU gates. Resolve independently diagnosable issues and record specific blockers involving hardware, external access, or product decisions.

The [G0 validation plan](G0_TECHNICAL_VALIDATION.md) defines the source and full KAT work.
