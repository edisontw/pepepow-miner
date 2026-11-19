# PEPEPOW Miner roadmap and acceptance gates

**Status snapshot: 2026-11-19.** Actual work is tracked in [the concise development handoff](DEVELOPMENT_STATUS.md). Read live GitHub refs before relying on this snapshot. A successful offline fixture does **not** imply accepted production shares, validated pool connectivity, or a releasable miner.

| Gate | Required deliverable and acceptance evidence | Status |
| --- | --- | --- |
| **G0 — consensus validation** | Provenance, reviewed licenses/notices, independently established complete 32-byte HooHash V110 outputs on ≥10,000 headers/nonces, on-chain vectors, native Windows/Linux evidence | **Partially validated; formal acceptance OPEN.** Existing Linux/native-platform parity evidence is not a Windows 11 pass |
| **G1 — native CPU CLI** | Correct header/target/job construction, bounded CPU search, Stratum, stale/reconnect handling, fault fixtures, observable share replies per approved mode/OS | **Offline bounded search prototype tested on Linux** at `5de6d64`; full job/pool path and platform acceptance OPEN |
| **G2 — desktop MVP** | Explicit start/stop, profiles, bilingual diagnostics, native installation, sleep/recovery checks and ≥24h configured soak | Not started |
| **G3 — beta distribution** | Reproducible packages, licenses/notices, checksums, supported-OS matrix, confidentiality checks | Not started |
| **G4 — NVIDIA** | CUDA, independent CPU confirmation per candidate, native parity/driver and 24h evidence | Not started |
| **G5 — AMD** | Hardware-dependent OpenCL/HIP decision and OS/device/driver parity | Not started |

## G0: evidence must stay separated by platform

The pinned Core is `bc08108ceccd957c79040dc7fedc1d56d8d47f97` and OpenLibm 0.8.7 is `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`. Native Linux Release/Debug tests reported 10,000 complete independent digests with zero mismatches, with intermediate/worker probes. Evidence also exists for Windows 10 22H2 and Windows Server 2025; neither substitutes for native Windows 11 workstation validation. Windows 11 testing is owner-approved **deferred** so offline work may continue. For original runs, hashes and limitations, see [historical log](archive/DEVELOPMENT_STATUS_LOG_THROUGH_20261119.md). Formal G0 sign-off remains OPEN.

Do not mistake fixture self-consistency or only the first digest bytes for independent consensus proof. Record full header, target, all 32 digest bytes, upstream revision, compiler/FP options, exact command and mismatch reproducer.

## G1: completed pieces and remaining gates

**Implemented / reported offline:**
- Existing offline Stratum controller, job generation/invalidation, framing and protocol replay fixtures on main (PR #8 already merged).
- Independent target comparison helper remains in Draft [PR #6](https://github.com/edisontw/pepepow-miner/pull/6), and source/KAT work in [PR #2](https://github.com/edisontw/pepepow-miner/pull/2) / stacked [PR #11](https://github.com/edisontw/pepepow-miner/pull/11).
- `g1-offline-cpu-search` commit `5de6d64`: explicit 80-byte header/target input, nonce count 1–10,000, overflow rejection, stale-job check, first acceptable digest search. Owner-reported native Linux checks: offline 8/8, Python 41/41, native Stratum 22/22, chain KAT 10/10. These are **local historical results**, not CI evidence for this documentation branch.

**Still required:**
1. Make G1 a reviewable Draft PR without silently importing unrelated G0 history or modifying `main`.
2. Freeze an independently derived expected header/nonce/digest/target artifact; rerun Linux focused tests after any change.
3. Add a separate wall-clock timeout/cancel mechanism. A count cap alone does not enforce elapsed-time bounds.
4. Complete real Stratum notify → validated job header/merkle/target → CPU search integration. Handle endian mappings, floating-point difficulty rounding/clamping, difficulty changes, clean/non-clean jobs, same-ID updates, extranonce invalidation, stale/reconnect behavior, overflow, malformed frames, invalid replies and timeout.
5. Keep [PR #9](https://github.com/edisontw/pepepow-miner/pull/9) and stacked [PR #10](https://github.com/edisontw/pepepow-miner/pull/10) localhost handshake/timeout work distinct to avoid duplicate code.
6. Before G1 acceptance, validate each actual native OS and intended pool mode with explicit owner-approved controlled network tests. Collect ≥1,000 **replied shares** for each major configuration before drawing rejection-rate conclusions; record invalid/low-difficulty/stale reasons separately. No live pool testing in the current offline-only authorization.

For target boundaries test **target−1**, **target**, and **target+1** (including equality) with an independently sourced comparator. Do not assume Bitcoin difficulty constants apply directly to PEPEPOW. A pool reply marked `accepted` is neither a discovered block nor a payout.

## Product and operational boundaries

- Native CPU CLI is first; headless core must not depend on WSL, Qt, or a GPU.
- Keep POOL and pool-based SOLO modes distinct. Failover must not silently change mode, payout address, or fee.
- Local hashrate, difficulty-weighted work and pool estimates must remain separate and time-stamped.
- Mining requires explicit user action and resource limits; no covert, unattended or unlimited mining.
- Do not request wallet private keys or seed phrases. Do not enable remote control, telemetry, automatic mining, overclocking, or voltage controls by default.
- SQLite/API, adaptive activity detection, wallet/RPC SOLO and AMD backend are deferred.
- G2 uses ≥24h per actual native OS configuration; later stable release validation uses ≥72h soak, never treat 24h as 72h.

## Immediate next task

Keep the G1 offline proof reproducible and its PR diff small. Preserve historical evidence and all open PR dependencies. Continue with fixed offline vectors and explicit elapsed-time limits before enabling any networking or releases. Follow [AGENTS.md](../AGENTS.md) for English repository documentation and evidence standards.
