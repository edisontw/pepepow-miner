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

## 2026-11-07 04:35+08 run update
- `main` observed at `3ab4e56b16140f61f30d1af7ddc324d84040067b`. PR #8 previous head `5cd58cfc3ea05925101045ebb466f675651fb0c5` had completed successful native workflow run `42281246315`; this does **not** validate the new commits.
- PR #8 added typed `JobState::Event` dispatch for connection, authorization, difficulty, notify, extranonce reset, and disconnect; regression checks cover unauthorized messages, invalid difficulty, stale job invalidation and session reset. Commits `74797edd70f95cedaffa6a26c620ec72b3c8daa1` and `8d154abf02956f6059c0391d9d3454b86fdbb0df`. This is a decoded-event boundary, **not** a JSON parser or live Stratum client.
- New head native CI and local compilation pending at this handoff; no pool connection/submit. Next: check head-specific CI and build a bounded offline Stratum JSON line decoder with fixed pool dialect fixtures, wired to these typed events. G0 independent full 32-byte parity remains NOT PASSED.

## 2026-11-07 05:35+08 run update
- main remained `3ab4e56b16140f61f30d1af7ddc324d84040067b`. Previous PR #8 head `8d154abf02956f6059c0391d9d3454b86fdbb0df` failed native CI run 42289772833: Ubuntu 22.04 and 24.04 GCC `-Werror=missing-field-initializers` in Event aggregate tests; Windows 2022 succeeded. This failure was confirmed from actual Ubuntu job logs.
- Fixed PR #8 `include/pepepow/job_state.hpp` by adding a default-argument Event constructor that initializes every field; commit `3d5e9a1ac9f1b11a7c289f8be3ce7ab1dc7639e5`. New head-specific CI is pending, do not claim success. No consensus/hash change or pool submit.
- Next: verify this head's native CI; if green, implement a bounded offline Stratum line framing/JSON decoder with fixed dialect fixtures, and keep G0 independent 32-byte parity explicitly pending.

## 2026-11-07 06:37+08 run update
- Re-fetched main `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #2 pre-status head `1544dbf8352cb4d889a60898419dbea1a06efb24`; three head-specific CI workflows succeeded (42297886425, 42297886391, 42297886359). PR #8 prior head `3d5e9a1ac9f1b11a7c289f8be3ce7ab1dc7639e5` native workflow 42297867577 succeeded.
- PR #8 added bounded offline Stratum line framer (64 KiB payload, fragmented/multiple CRLF lines, NUL/oversize discard through newline, session reset) and focused C++20 test. Commits `899ec80`, `43e9225`, `13422dc` update header, test, and existing native CMake/CTest matrix. Local focused GCC compile/run PASS. New head native CI pending. This is framing only: no JSON decoder, typed-event mapping, network, mining, or share submit.
- Independent 80-byte-to-32-byte HooHash parity still absent; G0 NOT PASSED. Next: verify PR #8 native CI and implement strict JSON dialect decode into typed JobState events using independent pool fixtures; keep code changes small.

## 2026-11-07 08:35+08 run update
- Rechecked main `3ab4e56b16140f61f30d1af7ddc324d84040067b` (unchanged), PR #2 head `08b1ba30848f6ce88ffa087d2eff18a00194a736` (three head-specific workflows succeeded), PR #8 prior head `13422dce285d4ff00669ea9daaefd3af9c5f6206` (native workflow 42306177254 succeeded). PR #3 closed unmerged, #4 merged; #5–#7 still open.
- PR #8 fixed a lifecycle hole: late `authorized(true)` after disconnect previously enabled a job without a new `connected()` event. Added explicit connection gating and two focused regression assertions in `include/pepepow/job_state.hpp` and `tests/test_job_state.cpp`; commits `c811b6d7` and `cecbb3ed`. Latest head CI and local build are **pending**; do not equate previous green CI with this change. No network or pool submit.
- G0 independent full 32-byte parity still absent. Next: inspect new PR #8 native CI, fix if needed, then implement strict offline JSON-RPC decoder with pool-specific fixtures and connect it to the existing typed events.

## 2026-11-07 09:38+08 run update
- `main` confirmed `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #2 head before this handoff update `a82deed636540b112d5c5a820efdc66a05681798`; PR #8 new head `053e10589b498acd080557c4bb4bbb421db3084d`.
- PR #8 now accepts a `mining.set_difficulty` event after connection but before authorization reply; real Stratum messages may arrive in that order. Commits `7ae3ffb687f209d2e9fc9701717a77e16e9b2140`, `053e10589b498acd080557c4bb4bbb421db3084d` update job state and focused test. Prior head CI `42319182532` passed; latest head CI pending. Local clone/CTest blocked by DNS resolution failure, **not tested locally**. No mining/network/submit.
- G0 remains NOT PASSED; no independently sourced full 32-byte digest parity. Next: check head-specific PR #8 native CI and implement bounded strict offline Stratum JSON decoder, without expanding isolated mocks.

## 2026-11-07 10:37+08 run update
- Re-fetched main at `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 previous head `053e10589b498acd080557c4bb4bbb421db3084d` had successful head-specific native workflow run `42325134132`.
- PR #8 offline framer now rejects embedded or repeated raw CR rather than passing malformed JSON-line bytes through; preserves split CRLF. Changes: `include/pepepow/stratum_line_framer.hpp` (`e6486bd`) and `tests/test_stratum_line_framer.cpp` (`59707d3`). No local compile/run because container GitHub DNS failed; new-head CI pending. This is input hardening, NOT JSON parsing or real mining.
- G0 independent full 32-byte HooHash parity still NOT PASSED. Next: verify head-specific native CI; implement strict JSON decoder and typed event integration instead of more isolated framer tests.

## 2026-11-07 12:39+08 run update
- Rechecked main `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #2 pre-edit head `f8d9fa7e389dabf759e3753832ca27c3594650a9` with three successful head-specific CI runs (42330198834, 42330198829, 42330198844). PR #8 previous head `513e94300727f6f77fe4e9fc82e55b328f8778df` passed native CI run 42334405034.
- PR #8 now includes a narrowly scoped **offline fixed-order** `mining.set_difficulty` JSON notification decoder, plus negative-input tests and CMake/CI wiring. Commits `150b56c0`, `619a528c`, `6ebfa18f`, `f902ad6e`. No generalized JSON-RPC parser, authorize/notify decoder, networking or pool submit; other field orderings are intentionally rejected pending protocol fixture review.
- Local C++20 `-Wall -Wextra -Werror -pedantic` focused test passed **with a minimal JobState test stub**, not the actual repository header; do not count it as an integration pass. New head-specific native CI was not yet reported at this handoff, so real JobState integration and Windows/Linux checks remain pending.
- G0 independent full 32-byte digest parity remains NOT PASSED. Next: inspect PR #8 latest native CI; if green, confirm actual pool notification shape and implement one additional real JSON-RPC message type with strict parsing.

## 2026-11-07 13:36+08 run update
- Main confirmed `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #2 pre-edit head `3723932a81e7150e94fedaeab341f7a46db86840`; PR #8 prior head `f902ad6ef008c77c28a72d439b56aeba4aa0c83a` native workflow 42339449019 completed **success**. PR #2 three latest workflows 42339461055/60830/60752 succeeded; none proves independent HooHash parity.
- PR #8 now adds strict **offline authorization reply** decoding with expected numeric request ID, boolean result and null error; tests cover valid authorization, denial, mismatched ID and malformed shapes. Commits `40fbf3a745a91e4e955de9fa99ddd02bed5ab52f`, `4cee71f481d4b52e42d102d985494636a44d0fd3` in existing decoder/test paths. This is deliberately limited to fixed-order responses; non-null error objects and other JSON field orders are not yet supported. New head native CI pending; local build not run. No network or share submission.
- G0 remains NOT PASSED: no independent full 32-byte golden digest or 10,000 parity. Next: verify PR #8 new head CI, then implement dialect-validated `mining.notify` decoding or replace the narrow JSON parsers with a bounded general parser before network integration.

## 2026-11-07 14:37+08 run update
- Main verified unchanged at `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 pre-change head `4cee71f481d4b52e42d102d985494636a44d0fd3` native CI run `42343531205` completed successfully. PR #2 head before this edit `e88a9b069cdbc485dc8279f5fd67d5cda787f78c`.
- PR #8 authorization decoder accepted invalid JSON numeric ID `02` if caller's expected ID was also `02`. Reject noncanonical leading-zero IDs in `include/pepepow/stratum_difficulty_decoder.hpp`, with regression in `tests/test_stratum_difficulty_decoder.cpp`. Commits `50a1dc020cf024eb4f7dfb094a8ce0de5c645eec`, `35e35844ef9f66859ca94ff4e9647aecd02b1a93`. New head CI pending; no local compilation executed.
- Next: verify head-specific native CI and inspect actual PEPEPOW pool `mining.notify` dialect before implementing its offline decoder. G0 independent 32-byte parity remains NOT PASSED. No production pool traffic or merge.

## 2026-11-08 submit ID reuse fix
- Main `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 previous head `217cf5896af9466b0df5b4c067b6565512dde1c4` native CI run 42440445048 succeeded.
- PR #8 commit `d5fbb4c12ff80c60afa02983a93e93fd81c6b585` requires monotonically increasing numeric submit IDs per connection, preventing a delayed duplicate reply from resolving a reused ID; reconnect resets the sequence. Updated focused session test. Linux container g++ C++20 with warnings as errors passed; new head CI pending. No live transport or pool submit tested.
- G0 independent full-digest parity remains unverified. Next: verify new head native CI and integrate a bounded request-ID allocator shared across subscribe, authorize and submit; avoid reusing IDs across request types.

## 2026-11-08 structured submit error validation
- PR #8 previous head `d5fbb4c12ff80c60afa02983a93e93fd81c6b585` native CI 42450221905 passed. Commit `1f60c35119abebe246ea4213f8ac3d26a953feb4` validates array/object JSON error structure with depth limit 32 before settling pending share; nested data remains supported. The validator checks JSON grammar and escapes but does not validate UTF-8 encoding. Focused Linux container C++20 submit decoder and session tests passed; new head CI pending.
- G0 independent full 32-byte parity remains unverified; no live network or GPU. Next check native CI, then integrate request IDs across subscribe/authorize/submit.
