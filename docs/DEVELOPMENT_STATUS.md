# PEPEPOW Miner — Current development status

**Snapshot: 2026-11-19 (Asia/Taipei).** This is the concise active handoff. The original chronological G0/G1 run log is preserved without edits in [the historical archive](archive/DEVELOPMENT_STATUS_LOG_THROUGH_20261119.md). Re-query GitHub HEAD, PRs, and exact-head checks before making changes.

## Current authority and branches

- Repository: [edisontw/pepepow-miner](https://github.com/edisontw/pepepow-miner).
- `main` at this snapshot: `8c762af16901fc7c826cc400e3098cbc4e4530ca`. No G1 code merged.
- G1 offline prototype: branch `g1-offline-cpu-search`, commit `5de6d64aab6696918ab37bbdfd5d5164e33fc041`. This is a four-parent integration commit, not a minimal PR diff.
- G0: [PR #2](https://github.com/edisontw/pepepow-miner/pull/2), head `0404468510bc67360db6edb493bc58381c0943ea`; [PR #11](https://github.com/edisontw/pepepow-miner/pull/11), head `1f9f8133fab15356f2fb4698b08718136f84d590`, stacked on #2. Preserve the unique nonce-generator correction and `db8588d` atomic TSV-publication fix.
- [PR #6](https://github.com/edisontw/pepepow-miner/pull/6) supplies the explicit uint256 little-endian target comparator. PRs #5/#7 and #9/#10 remain separate workstreams; #8's offline Stratum lifecycle was previously merged into main.
- No PR has yet been opened for `g1-offline-cpu-search`. No exact-head PR Actions run was returned for commit `5de6d64` when this snapshot was checked.

## Verified scope and evidence boundaries

**G0: NOT FULLY ACCEPTED.** Earlier native Linux evidence records 10,000 independent Python-versus-Core full 32-byte parity comparisons with zero mismatches in Release and Debug. Native Ubuntu 22.04/24.04 and Windows 10 22H2 / Windows Server 2025 results have distinct evidence. The Windows 11 workstation validation is **owner-approved deferred**, not passed. Do not infer Windows 11 evidence from Windows Server or OS-unidentified Windows x64 results. Formal G0 acceptance and release/redistribution disposition remain open.

Pinned consensus reference inputs:

- PePe-core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OpenLibm 0.8.7: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- Preserve full input, full 32-byte digest, byte-order mapping, platform/toolchain, and the first mismatch if any regression appears.

**G1: OFFLINE PROTOTYPE VALIDATED ON LINUX ONLY.** The owner reported testing `5de6d64` locally on native Linux: eight offline CPU search fixture cases, 41 Python regression tests, 22 native Stratum tests, and 10 chain fixture KATs, all passing. The native results were not independently rerun during this documentation cleanup. The offline CLI takes an explicit 80-byte header, target, start nonce, and a bounded count of 1–10,000; it uses the pinned Core reference and target comparator. Tests cover the reported first acceptable nonce `4294967295`, equality / strict-failure target boundaries, overflow rejection, and same-ID job replacement with stale candidate rejection.

The current test derives the expected nonce/digest from the independent Python implementation at runtime; a separately frozen expected-result artifact is still desirable. CLI count bounds exist, but a separate wall-clock timeout is not yet implemented. Network job-to-header/target assembly, pool integration, Windows-native G1 verification, accepted shares, full G0 acceptance, and release eligibility are **not** established.

## Next small deliverables

1. Keep `main` unchanged. Inspect the multi-parent G1 history; prepare a reviewable Draft PR that distinguishes G1-specific changes from #2/#11/#6 and already merged Stratum code.
2. Add one frozen independent fixture with exact header, nonce, full digest, target, and provenance. Rerun focused Linux native tests; retain actual command/output and exact tested SHA.
3. Add an explicit wall-clock limit to the bounded offline CLI, then test zero/max count, nonce wrap, target equality, no hit, and stale-job paths.
4. Only after independent fixtures and native checks, advance **offline** job-to-header/target integration. Do not connect production pools, submit shares, merge PRs, publish binaries, or process wallet secrets without separate authorization.

## Documentation rules

Keep active handoffs short and dated. Preserve historic validation evidence in the archive rather than rewriting it as current truth. Update [the documentation index](README.md), [roadmap](ROADMAP.md), and [offline CLI instructions](../tools/README.md) when capability changes. All new repository prose and commit messages are in English.
