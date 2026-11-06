# PEPEPOW Miner — G0 handoff (2026-11-06)

## Authority and scope
- At handoff, main: `3ab4e56b16140f61f30d1af7ddc324d84040067b` (PR #4 squash-merged). Re-fetch main SHA on every new run; do not assume it remains unchanged.
- README and AGENTS.md describe a planned product, not a functioning miner. Read docs/ROADMAP.md, docs/G0_TECHNICAL_VALIDATION.md, docs/RESEARCH_LEDGER.md and all three docs/research reports before consensus changes.
- G0 is **NOT PASSED**. Independent full 32-byte HooHash V110 expected digest, >=10,000 independent parity cases, native Windows/Linux evidence, and later GPU/pool testing remain outstanding.

## Branches and work
- PR #2: https://github.com/edisontw/pepepow-miner/pull/2 — Core reference/KAT, provisional pool candidate, digest export and comparator; candidate is NOT independently established consensus truth. Latest head `7698f8a9e3f57c748ae44e089cf263137b285a7a`: all three PR CI runs succeeded (Core build https://github.com/edisontw/pepepow-miner/actions/runs/42218656324 , corpus 42218656295, frontend 42218656257). The earlier `f8e1614a` regression failure was fixed by `d29d512a` and the later indentation fix `7698f8a`; do not chase the stale failure.
- PR #4 merged on `main` as `3ab4e56b`: reusable C11 header nonce helper and native CI success on Ubuntu 22.04, Ubuntu 24.04, Windows Server 2022 (https://github.com/edisontw/pepepow-miner/actions/runs/42219413174). PR #3 closed as redundant Python-only nonce-layout test. Draft PR #5–#7 remain open: Stratum prevhash, uint256 target comparison, compact nBits decoder; check dependency and native CI before merging.
- `main` now contains only the C11 header nonce helper/test/CI beyond planning documents. It is NOT a usable miner, and PR #2's KAT is still unmerged.

## Critical assessment
- Repeated tests of the same Core output, or file alias protections, do not establish independent consensus parity. Avoid further expansion of self-consistency tests unless a real regression requires it.
- The pool's `verify_known_vector.c` compares only a digest prefix and does not establish a trustworthy full 32-byte golden vector. Preserve provisional provenance.
- Previous iterations reported a green earlier CI then left a newer CI failure; always inspect latest head-specific CI, not an older success.
- Focus on product capability: independently derived V110 digest, real Core/pool protocol semantics, then minimal native CLI and Stratum state machine with offline mock tests. No live submit without authorization.

## Next actionable run
1. Re-fetch main and all open PR heads; inspect head-specific CI rather than previously fixed failures. Keep PR #2 unmerged until the independent golden-vector evidence is reviewed.
2. Audit PEPEPOW Core and HooHash upstream exact source SHA and header/nonce/endianness. Produce one truly independent 80-byte header -> full 32-byte digest KAT with provenance, then scale parity to >=10,000 cases.
3. Keep changes small, use focused tests, no real wallet keys, no live mining, no production pool submit, no release, no automatic merge.
4. Report start/end main SHA, changed paths, commit/PR, actual test outcomes, remaining evidence and next task. Maintain this single handoff/status source rather than spawning duplicate progress documents.

## 2026-11-06 21:37+08 run update
- Latest observed main: `3ab4e56b16140f61f30d1af7ddc324d84040067b` (PR #4 merged); PR #2 head `3c141c53302fe53166912d34050989da1996efc6`, three head-specific CI checks successful (runs 42219852682, 42219852661, 42219852622). These are NOT independent parity.
- New Draft PR #8: https://github.com/edisontw/pepepow-miner/pull/8 (head `475b794449b925e9de95f5d77c4f31f67cb04c70`): C++20 offline Stratum job lifecycle guard; local Linux focused test PASS. No network, mining, pool submit, Windows native or independent G0 parity test. Conservative latest-job-only policy: concurrent non-clean jobs are not supported yet.
- PR #5, #6, #7 remain open; PR #3 closed without merge, PR #4 merged. Do not duplicate their prevhash/target helpers. Next: independently establish full 32-byte V110 expected digest with provenance; in parallel improve PR #8 job semantics and native CI only if focused, validated work is available.
