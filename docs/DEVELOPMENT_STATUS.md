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

## 2026-11-06 22:36+08 run update
- Main still `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #2 head before this status edit `d9c5af1f2054dee33a22760fabfdc7ddfa41b43c`, with 3/3 head-specific workflows successful (runs 42227117124, 42227117242, 42227117143). No independent parity evidence; G0 remains pending.
- PR #8 updated: `b2f27622ee9f5609c99c3b405d6c50c75416e443`. Fixed stale snapshot reuse when a pool reissues the same job ID with `clean_jobs=false`; latest-job-only state now advances generation for every notification. Added regression coverage for repeated IDs and non-clean replacement. Focused Linux C++20 test PASS; native Windows and PR #8 CI pending. No networking or pool submit.
- Next: add focused Windows/Linux CI to PR #8 and validate the new head; independently sourced HooHash full-digest golden vector still blocks G0.

## 2026-11-06 23:38+08 run update
- Main checked at `3ab4e56b16140f61f30d1af7ddc324d84040067b` (unchanged); PR #2 head before status update `d4bda9e7c4405e207421fef8f0b1922047c172cd`.
- Found literal `\\n` text embedded in PR #8 `include/pepepow/job_state.hpp` after the latest-job comment; this broke actual C++ parsing and invalidated the previous reported test confidence. Fixed only those escaped newlines in PR #8 commit `8c3afa2ad13a88695a4a309856d20f5964d63dfa`. Focused local Linux C++20 test against the corrected header and existing test body PASS (`-Wall -Wextra -Werror -pedantic`). GitHub confirms file has no literal `\\n` and PR #8 head matches; no PR #8 workflow runs reported yet. Windows native still untested.
- Next: add a minimal Ubuntu/Windows native CI workflow to PR #8, and confirm head-specific results. G0 independent 32-byte HooHash parity remains unverified; no pool submit or GPU test.

## 2026-11-07 02:30+08 run update
- `main` observed at `3ab4e56b16140f61f30d1af7ddc324d84040067b` (unchanged). PR #8 previous head `f67086ca3beeb4c9785d1be5fa0594957609fb0c` had successful head-specific native CI run `42262879564`; PR #2 older verified head `7698f8a9` had three successful workflows. Re-check current heads/CI before relying on these results.
- PR #8 `feature/offline-stratum-job-lifecycle-20261106` received `invalidate_work()` to explicitly reject queued shares when a work template/extranonce changes, with focused stale-snapshot regression assertions. Commits `5f35d6a27b6a20109421787d1b8c17eb6109b372` and `709a2d1b245c1e5123745501902c9708a1b73127`. No local compiler/network access to GitHub; new head-specific CI not yet observed, so **unverified** until CI passes. No live pool submit.
- G0 remains NOT PASSED: no independently sourced full 32-byte V110 digest or >=10,000 independent parity checks. Next: verify new PR #8 native CI, then integrate an offline Stratum protocol message parser into the state machine; do not conflate an invalidation API with a wired network client.

## 2026-11-07 03:36+08 run update
- Main `3ab4e56b16140f61f30d1af7ddc324d84040067b` unchanged; PR #8 head `5cd58cfc3ea05925101045ebb466f675651fb0c5`. Previous PR #8 head `709a2d1b` native CI run 42271488079 succeeded; new head CI not yet available.
- PR #8 changed `include/pepepow/job_state.hpp` and `tests/test_job_state.cpp`: `can_submit()` now also requires the candidate snapshot difficulty to match the active job difficulty; added tampered-difficulty rejection and in-flight job target preservation checks. Commits `9afd8db` and `5cd58cf`. This is an offline guard, not difficulty-to-target consensus validation.
- GitHub updates succeeded; focused local compile/run was not executed this round (container has no GitHub network access). CI pending. G0 independent full digest parity remains NOT PASSED.
- Next: inspect head-specific native CI, then add a minimal offline Stratum event-to-JobState adapter with explicit dialect fixtures; avoid another standalone guard-only iteration.
