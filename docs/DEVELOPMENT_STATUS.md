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

## 2026-11-08 submit error UTF-8 validation
- PR #8 previous head `1f60c35119abebe246ea4213f8ac3d26a953feb4` passed native CI 42457645657. Commit `954a4f6a4e510e68e299b715b523965d520dc95d` validates raw UTF-8 in structured error JSON strings, rejecting invalid sequences, overlong encodings, encoded surrogates and codepoints above U+10FFFF. ASCII JSON escapes remain supported; this does not validate Unicode surrogate pairing in escaped sequences or every Stratum message.
- Focused Linux container g++ C++20 decoder and session tests passed. New head CI pending. G0 independent full-digest parity not established. Next check CI then integrate shared request-ID management for subscribe/authorize/submit.

## 2026-11-08 offline request controller
- PR #8 head `954a4f6a4e510e68e299b715b523965d520dc95d` native CI 42461969505 passed. Commit `3c19769c56ecfc725daad86844973f712268cb2c` adds an offline controller that allocates unique increasing uint64 IDs across subscribe, authorize and submit, correlates pending replies and checks connection generation. The controller sends no network traffic; callers must use this controller consistently. There is no timeout/cancellation path yet.
- Focused Linux container C++20 controller test passed; registered test in native CMake matrix, new CI pending. G0 independent 32-byte parity remains unverified. Next verify CI and add timeout/cancellation behavior before network transport.

## 2026-11-08 offline request cancellation
- PR #8 previous head `3c19769c56ecfc725daad86844973f712268cb2c` native CI 42465940267 passed. Commit `27a52ea82d340effc2df8be7963e5ece0a411b0f` supports generation-checked cancellation for pending subscribe, authorize and submit. Canceled submit outcome is unknown, counters unchanged; late replies are ignored and IDs are not reused in-session.
- Focused Linux container C++20 controller and session tests passed; new head CI pending. Timeout detection/scheduling remains caller responsibility, and no transport or live pool was tested. G0 independent full-digest parity not established. Next verify CI then add transport with deterministic loopback fixtures.

## 2026-11-08 offline Stratum outbound request encoding
- Verified main at `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #2 pre-handoff head `e1d7c7ee5800f3b936ce61242399b53638e11345`. Head-specific PR #2 G0 workflows **success**: pinned Core 42470378355, frontend 42470378133, corpus 42470378121. These tests are **not independent full-digest parity**.
- PR #8 (`feature/offline-stratum-job-lifecycle-20261106`) advanced from `27a52ea82d340effc2df8be7963e5ece0a411b0f` to `18ba43a148534dd9f9881a685fc15fbdf36ab5cf` (https://github.com/edisontw/pepepow-miner/commit/18ba43a148534dd9f9881a685fc15fbdf36ab5cf). New `include/pepepow/stratum_request_encoder.hpp` implements bounded offline JSON-RPC subscribe/authorize/submit request serialization with printable ASCII JSON escaping, nonzero numeric request IDs and strict extranonce2/nTime/nonce hex lengths. `tests/test_stratum_request_encoder.cpp` covers valid wire lines and malformed inputs. Registered in `tests/job_state/CMakeLists.txt`; the existing native workflow paths now include the controller and encoder headers/tests to prevent skipped CI.
- Local focused GCC 14.2 C++20 (`-Wall -Wextra -Werror -pedantic`) compile/run passed; standalone focused CMake/CTest `stratum_request_encoder` passed 1/1. PR #8 head-specific workflow 42476074786 **success on ubuntu-22.04, ubuntu-24.04, windows-2022**: https://github.com/edisontw/pepepow-miner/actions/runs/42476074786 . These are CI hosts, not a Windows 10/11 field-machine test.
- This is serialization only: no network transport, hash engine, valid share generation, production pool submit, or Windows 11/native hardware validation. The request encoder is not yet coupled atomically to the controller's request-ID lifecycle.
- G0 remains **NOT PASSED**. Core/pool candidates and corpus do not establish a source-independent 80-byte header -> full 32-byte V110 digest, nor 10,000 independent parity cases. No PR merged, no release.
- Next single action: connect offline request encoding to the controller's allocated IDs with deterministic loopback request/reply fixtures; ensure failed validation cannot create outstanding pending requests or leak IDs.

## 2026-11-08 19:45 offline request/controller atomic integration
- Rechecked main HEAD `3ab4e56b16140f61f30d1af7ddc324d84040067b`, PR #2 head `7b63d4ee5ffd949a3b4cb56259b5165369ba8561`, and PR #8 starting head `18ba43a148534dd9f9881a685fc15fbdf36ab5cf`. PR #5–#7 remain draft and separate. Prior PR #8 head workflow 42476074786 was successful; this does not validate the new head.
- PR #8 now at `3166bb190e4db821e8d7157770c2985b8bf0c7cc`. Changed `include/pepepow/offline_stratum_controller.hpp` to add `make_subscribe_request`, `make_authorize_request`, and `make_submit_request`: the controller validates outbound JSON-RPC before allocating IDs/tracking a share, and produces the encoded wire line only for registered requests. Changed `tests/test_offline_stratum_controller.cpp` to exercise invalid authorization/submit input without consuming IDs, valid ID sequence, canceled share replies, reconnect generation and stale job rejection.
- Commits: `495fbfd7d5eec3be67bf82d22576ad6f1f205250`, `82999b88e53b7af47d74c6c54b38e3aa0db95049`, `3166bb190e4db821e8d7157770c2985b8bf0c7cc` (corrected C++ literal escaping). GitHub connector writes succeeded. **New head-specific CI not yet available** when checked; focused native compile/test was **not run locally** in this turn. Do not claim pass until latest head CI completes. Review failure logs and make the smallest fix if needed.
- Offline serialization only; no socket transport, real pool, hashing, accepted share, Windows 10/11 field test or GPU. G0 remains **NOT PASSED**; independent full 32-byte parity/10,000 independent comparisons remain absent.
- Next single action: inspect PR #8 exact-head native matrix CI, fix any failures, then add a minimal deterministic loopback transport fixture without production network activity.

## 2026-11-08 20:47+08 run update
- Re-fetched main commit list: `3ab4e56b16140f61f30d1af7ddc324d84040067b` remains latest main; PR #8 branch `feature/offline-stratum-job-lifecycle-20261106` was inspected directly. Prior PR #8 head `3166bb190e4db821e8d7157770c2985b8bf0c7cc` native CI run [42477189099](https://github.com/edisontw/pepepow-miner/actions/runs/42477189099) is **success**, but that is not evidence for the new head.
- PR #8: `include/pepepow/offline_stratum_controller.hpp` now rejects stale/unauthorized `begin_submit` before allocating a request ID (`baeb3063e97571ad1daf22619cbe9121872e4ab2`); `tests/test_offline_stratum_controller.cpp` asserts stale submission rejection across reconnect (`f277352b0b564008f25abdb6405db2209f4a88df`). Both GitHub writes succeeded. Latest-head native workflow not yet reported; no local compile/run was performed. The pending-submit-capacity failure path can still consume an ID and warrants later preflight work if relevant.
- No network transport, live submit, hashing or independent G0 parity. **G0 NOT PASSED**. Next: inspect CI for `f277352b`, fix failures; if green, add a bounded deterministic offline loopback transport fixture, not another isolated state guard.

## 2026-11-08 21:44+08 run update
- Rechecked main at `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 started at `f277352b0b564008f25abdb6405db2209f4a88df` and ended at `b3e131fb8cabc880021accc28e58376835a8c710`. PR #2 was `48989e963daa557de5b484090c4f6fe3206a3ed2` before this handoff edit. PR #5–#7 remain open draft; PR #3 closed and #4 merged.
- Verified previous PR #8 head-specific native run [42481965710](https://github.com/edisontw/pepepow-miner/actions/runs/42481965710) successful on Ubuntu 22.04, Ubuntu 24.04, and Windows Server 2022. This does NOT validate the new head.
- PR #8 commits [09a72ded](https://github.com/edisontw/pepepow-miner/commit/09a72ded4320cdba66290cd016be9607b282e693) and [b3e131fb](https://github.com/edisontw/pepepow-miner/commit/b3e131fb8cabc880021accc28e58376835a8c710) integrate the existing bounded StratumLineFramer into OfflineStratumController.receive_bytes(bytes, generation). Framing is reset on connected/disconnected and stale-generation bytes are ignored; focused controller test adds fragmented-line and reconnect isolation cases. No socket, JSON dispatch, mining, or pool submit was added.
- New head-specific workflow runs were not yet returned at this handoff; local native compiler test not executed. Mark new code **CI pending/unverified** until its own run passes. G0 remains **NOT PASSED**, without independently sourced complete 32-byte parity or 10,000 independent vectors; Windows 11/GPU/production pool remain untested.
- Next: inspect PR #8 head-specific native CI and fix any real failure, then implement a bounded offline line-to-message dispatcher for current generation using existing decoders rather than another isolated mock.

## 2026-11-08 22:40+08 run update — repair native framing regression
- Rechecked main HEAD `3ab4e56b16140f61f30d1af7ddc324d84040067b` (unchanged); PR #8 start head `b3e131fb8cabc880021accc28e58376835a8c710`, PR #2 start head `5dd0450fcaaf7e26ede93819f7cae428e61ae941`. PR #3 closed unmerged, #4 merged, #5–#7 open, no PR #9 found.
- PR #8 exact-head native workflow [42486387959](https://github.com/edisontw/pepepow-miner/actions/runs/42486387959) **FAILED**: build succeeded but Run step failed on Ubuntu 22.04/24.04 and Windows Server 2022. Inspecting newly added controller regression found five `\\\\n` C++ string sequences (literal backslash+n), while `StratumLineFramer` only emits complete lines on an actual LF byte. This explains the deterministic assertion failure; no full raw log was retrieved.
- Fixed only `tests/test_offline_stratum_controller.cpp` in PR #8 commit [954c383](https://github.com/edisontw/pepepow-miner/commit/954c3836607ac2879a9e19265f918009117c9b61), replacing those five test literals with real escaped LF `\\n`. Controller/production code unchanged. New exact-head CI is **pending**, and no local compiler was run; do not claim this fix passed until confirmed. No network, hash, pool submit, GPU, or Windows 11 field test.
- G0 **NOT PASSED**: no independently sourced complete 32-byte HooHash V110 parity, 10,000 independent comparisons, or required native field evidence.
- Next: inspect PR #8 commit `954c383` head-specific CI and correct any real failures; then wire a bounded offline line-to-message dispatch using existing decoders rather than adding more isolated mock tests.

## 2026-11-08 23:45+08 autonomous run
- Main checked: `3ab4e56b16140f61f30d1af7ddc324d84040067b` (unchanged). PR #8 began at `954c3836607ac2879a9e19265f918009117c9b61`; head-specific native run [42491009477](https://github.com/edisontw/pepepow-miner/actions/runs/42491009477) succeeded across Ubuntu 22.04, Ubuntu 24.04, Windows Server 2022 (configure/build/run steps successful). This is not native Windows 11 evidence.
- PR #8 now adds bounded framed-byte-to-work-event dispatch in `OfflineStratumController::receive_work_events`, applying the existing fixed-dialect difficulty/notify decoders, reporting ignored lines and framing rejects. Reply correlation remains explicit, not auto-dispatched. Added focused fragmented input, invalid line, stale generation tests. Commits `d9a72d5`, `96ddc2e`, `e683e46` (fixture delimiter correction). Latest head-specific CI still must be checked; no local compilation was performed in this run.
- G0 **NOT PASSED**: no independent 80-byte-to-full-32-byte V110 parity oracle, no 10,000 independent vectors, no Windows 11 native evidence; no live pool submission or GPU testing.
- Next: verify PR #8 head-specific CI and fix any concrete failure; then add offline reply routing without losing request ID/generation correlation, or a minimal loopback CLI vertical slice.

## 2026-11-09 00:49 Asia/Taipei — offline Stratum framed handshake
- Verified main base via PR #8: `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 previous head `e683e4674e4eed30ab863d87aec6406d2f3b8765`, native Actions 42496366314 completed successfully (previous head only).
- PR #8 now `1914ba55b25fbd24fa3df2efc4947e9ecc350d78`: `receive_work_events()` dispatches framed subscribe/authorize replies using strict pending-ID and generation checks before work difficulty/notify, with separate handshake counters; added fragmented subscribe/authorize, combined work events, duplicate reply and stale generation fixtures. Commits `22ca6c3`, `ee3e9f0`, `1914ba5` (last fixes fixture newline bytes).
- Current head-specific CI: pending/not yet reported at handoff; no local C++ compile (network clone unavailable in container). No socket, live pool, GPU or Windows 11 device test. G0 independent 32-byte digest parity and >=10,000 independent cases remain NOT PASSED.
- Next: inspect current head CI; fix actual errors if any, then add strict framed submit-reply correlation to the same offline dispatcher (avoid parsing untrusted IDs loosely).

## 2026-11-09 01:46 Asia/Taipei — framed submit-reply routing
- Checked PR #8 start head `1914ba55b25fbd24fa3df2efc4947e9ecc350d78`, base main `3ab4e56b16140f61f30d1af7ddc324d84040067b`; prior head native workflow [42501937426](https://github.com/edisontw/pepepow-miner/actions/runs/42501937426) reported success (job steps still require checking). PR #2 start `e72973d70063a8e7da8e23a7205b8376e607372a`.
- PR #8 commits [b90c3da](https://github.com/edisontw/pepepow-miner/commit/b90c3da0fa07f66994f66e30ba958397511fb85c) and [b7bd1c1](https://github.com/edisontw/pepepow-miner/commit/b7bd1c18594f95e563621a0404ce41210b3469df): `receive_work_events` now routes framed submit replies by canonical decimal request ID into existing strict full-JSON decoder and pending set; separate accepted/rejected/pool-error counters. Tests cover accepted, duplicate, wrong ID, malformed payload, rejection, pool error, uint64 overflow, stale generation. No socket, pool submit, hashing or GPU.
- Current head `b7bd1c18594f95e563621a0404ce41210b3469df`: no CI reported immediately after commit; local C++ compilation not performed. Must verify exact-head native CI before acceptance. G0 independent full 32-byte parity, 10,000 independent cases, Windows 11 device tests: **NOT PASSED**.
- Next: verify exact-head CI, fix any concrete failure; then implement a bounded offline loopback CLI using the existing controller, without live pool transport.

## 2026-11-09 02:47 Asia/Taipei — pending submit queue preflight
- Rechecked main HEAD `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 started at `b7bd1c18594f95e563621a0404ce41210b3469df`; its exact-head native CI [42506731637](https://github.com/edisontw/pepepow-miner/actions/runs/42506731637) completed **success**. PR #2 began at `b1c71c8ce6d822fe7287265e1556b7ff9dc9968d`.
- PR #8 changed `include/pepepow/offline_stratum_session.hpp` and `include/pepepow/offline_stratum_controller.hpp` to preflight the 4096 outstanding submit limit before allocating a request ID. This addresses the known failure path where `track_submit` rejects a full queue after `allocate` consumed an ID. Added focused `tests/test_offline_stratum_controller.cpp` regression: fill 4096 pending requests, reject one without consuming ID, cancel a request and verify next ID is 4099. Commits `4279ac93`, `d9533bce`, `b5257547`; PR #8 ending head `b52575470fbcd47757db80a74ff205a5497621fb`.
- New head-specific CI **not yet confirmed** at this handoff; no local native C++ compile/test performed. The preflight is a synchronous single-thread offline controller guard, not a concurrency reservation or socket transport. Do not claim passed until CI confirms. No production pool, GPU, hashing or Windows 11 hardware tests.
- G0 **NOT PASSED**: independent complete 32-byte HooHash parity, 10,000 independent cases, and Windows 11 native evidence still absent.
- Next: verify PR #8 exact-head native matrix; if green, build minimal bounded offline loopback CLI vertical slice that exercises request, response and work events with no live pool connection.

## 2026-11-09 03:47 Asia/Taipei — offline transcript replay executable
- Re-fetched main `3ab4e56b16140f61f30d1af7ddc324d84040067b`, PR #8 began at `b52575470fbcd47757db80a74ff205a5497621fb`, PR #2 began at `ad09ff9895d284ddd50db1b76606b9f446d1fa61`. PR #8 previous exact-head native CI [42511786586](https://github.com/edisontw/pepepow-miner/actions/runs/42511786586) passed all three Ubuntu 22.04/24.04 and Windows Server 2022 jobs.
- PR #8 now has `tools/offline_stratum_replay.cpp`: bounded 4096-byte stdin reads; sequential JSONL dispatch into existing offline controller; emits subscribe and authorize request text (never sends), prints event/share diagnostics; exits nonzero if handshake incomplete. Built with C++20 via `tests/job_state/CMakeLists.txt`; native workflow paths include this executable. `--self-test` exercises offline subscribe, authorize, difficulty dispatch; CTest registers it. New PR #8 head `376004955776339a6afa21dae1fa33d912fd365c` (commits `5efe3a4`, `10c4401`, `f9bf11c`, `f1b5bdf`, `2e2e87c`, `03a9078`, `3760049`).
- Exact-head [CI 42516962418](https://github.com/edisontw/pepepow-miner/actions/runs/42516962418) queued at check time; no local compiler/test execution, so the new executable is **NOT YET VERIFIED**. This is stdin transcript replay, not a socket transport, miner, GPU backend, accepted-share evidence, or actual pool connection.
- G0 **NOT PASSED**: independent full 32-byte HooHash V110 digest provenance, 10,000 independent parity cases and Windows 11 native acceptance still missing.
- Next: inspect latest-head native CI; fix concrete build/test failures if any. If green, add bounded offline loopback/socket harness only after focused protocol verification, without live pool submit.

## 2026-11-09 04:46 Asia/Taipei — autonomous run
- Rechecked main `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 started `376004955776339a6afa21dae1fa33d912fd365c`, ended `a7088632b8b3142af1955d83e7f1f8c5ffeab52e`. PR #2 started `758417987b369bd988853f1f1f0d50390d60b2d8`.
- Prior PR #8 native workflow https://github.com/edisontw/pepepow-miner/actions/runs/42516962418 confirmed successful on Ubuntu 22.04, Ubuntu 24.04 and Windows Server 2022 (all three Configure/Build/Run succeeded). New head CI remains pending confirmation.
- PR #8 changed `tools/offline_stratum_replay.cpp` and `tests/job_state/CMakeLists.txt`: replay now rejects an unterminated trailing JSONL frame with nonzero exit, even after a successful handshake; added `--self-test-truncated` negative CTest. Commits `0cfb6582550eacaeb054a556f7884308fd60082a`, `a7088632b8b3142af1955d83e7f1f8c5ffeab52e`. No local compiler run; head-specific CI is pending.
- G0 **NOT PASSED**: no independently established 80-byte header to full 32-byte V110 digest parity or >=10,000 independent cases; Windows 11 and live pool/GPU untested. No real pool submission or release.
- Next: inspect new head-specific CI; fix any failure, then build a bounded offline loopback transport for the CLI, preserving generation and request-ID checks.

## 2026-11-09 05:45+08 run update
- Main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 head `a7088632b8b3142af1955d83e7f1f8c5ffeab52e` -> `5029a1fbc40135fd50de500fe02b72b9b9443def`. Previous head's native workflow run 42521876959 succeeded; new head-specific workflow not yet reported at check time.
- PR #8 commits `e719a97f0b86398b6afdacf6b7a2f3c9b1c8880a` and `5029a1fbc40135fd50de500fe02b72b9b9443def`: offline replay CLI now returns nonzero for oversized/rejected Stratum frames even if subscribe/authorize succeeded; adds `--self-test-malformed` and negative CTest. Changed `tools/offline_stratum_replay.cpp`, `tests/job_state/CMakeLists.txt`. This prevents malformed transcripts from being falsely accepted. No network or mining.
- Local focused native build was attempted but blocked by container DNS (`Could not resolve host: github.com`); do not claim local test passed. Await head-specific CI and fix based on actual logs if necessary. G0 **NOT PASSED**: independent full 32-byte digest and 10,000 independent parity cases remain missing; Windows 11/GPU/pool untested.
- Next single task: inspect PR #8 head-specific CI; if green, add a bounded offline loopback transport fixture for request/reply delivery and reconnect generation isolation.

## 2026-11-09 06:45+08 run update
- Rechecked main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 started `5029a1fbc40135fd50de500fe02b72b9b9443def`, ended `037336035a64493ff351808fffc5c4326e49938f`; PR #2 started `3fe936246a3efbfcf830ecd7efd078e152b529d2`.
- Prior exact-head CI [42526812834](https://github.com/edisontw/pepepow-miner/actions/runs/42526812834) passed Ubuntu 22.04, Ubuntu 24.04 and Windows Server 2022. New head-specific CI not yet returned at check time.
- PR #8 commits `ed16d4bf5c3392d13921937882160fcc2a800f68`, `037336035a64493ff351808fffc5c4326e49938f`: added executable `--self-test-reconnect` to offline replay CLI, using the real controller to assert partial-frame clearing across disconnect/reconnect, stale generation isolation and fresh subscribe/authorize handshake. Registered focused CTest `offline_replay_reconnect`. Changed `tools/offline_stratum_replay.cpp` and `tests/job_state/CMakeLists.txt`.
- Local native compile unavailable because container cannot resolve github.com; do not claim new fixture passed until exact-head CI is green. No live socket, pool submission, GPU, mining or Windows 11 hardware tests.
- G0 **NOT PASSED**: independent full 32-byte HooHash V110 digest provenance and 10,000 independent parity cases still missing.
- Next: verify exact-head CI; if green, add a bounded offline loopback transport exercising outbound request delivery and inbound response on a controlled local socket.

## 2026-11-09 07:45+08 run update
- Rechecked main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 began `037336035a64493ff351808fffc5c4326e49938f`, ended `3526f56cc9e104cae50f8def691f0bd8d697c272`; PR #2 began `a32b4e8d665172eae9638f729ddcc6c76be5b6ad`.
- Previous PR #8 exact-head workflow [42531789613](https://github.com/edisontw/pepepow-miner/actions/runs/42531789613) returned success. New head-specific CI is not yet verified.
- PR #8 updated `tools/offline_stratum_replay.cpp` and `tests/job_state/CMakeLists.txt`: added `--self-test-cancel` and focused CTest to check cancelled subscribe ID cannot be reused, stale reply cannot subscribe, subsequent ID succeeds, and old generation cancellation fails after reconnect. Commits `7072a707`, `29ddf8cb`, `3526f56c`. No local native compilation; new test pending CI verification. No socket or live pool traffic.
- G0 **NOT PASSED**: independent full 32-byte V110 digest parity and 10,000 independent comparisons absent; Windows 11, GPU, live pool not tested.
- Next: verify exact-head CI, fix failures from logs, then implement one bounded local loopback socket receive/send vertical slice with generation isolation.

## 2026-11-09 08:47+08 run update
- Rechecked main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 began `3526f56cc9e104cae50f8def691f0bd8d697c272` and ends `4396328feb5f644f5e1237c04d6e4d731abbe90d`; PR #2 before handoff update `12f53914d343cec273e6599fd2f01dfeac2e2aca`.
- Previous PR #8 exact-head native workflow [42536939345](https://github.com/edisontw/pepepow-miner/actions/runs/42536939345) succeeded. New head-specific workflow not yet reported at time of check; do not infer pass. No local compilation performed.
- PR #8 code change: `tools/offline_stratum_replay.cpp` now prevalidates login with existing strict encoder before emitting any subscribe request, preventing partial output on invalid credentials; `tests/job_state/CMakeLists.txt` adds focused empty-login rejection CTest. Commits `861f9310bafcf029bdcc7d1cbfb8e9e820a10597`, `4396328feb5f644f5e1237c04d6e4d731abbe90d`. Test awaits exact-head CI.
- G0 **NOT PASSED**: independent full 32-byte HooHash V110 digest parity, 10,000 independent comparisons, Windows 11 native, GPU and pool evidence absent. No live submit.
- Next: inspect exact-head CI and fix any failure; otherwise implement one bounded localhost-only socket loopback send/receive fixture with generation isolation.

## 2026-11-09 09:45+08 run update
- Main start/end checked at `3ab4e56b16140f61f30d1af7ddc324d84040067b`. PR #8 start `4396328feb5f644f5e1237c04d6e4d731abbe90d`; previous head-specific native CI run `42542099221` completed success. PR #2 pre-status head `4962441261a9205bce4a4b198019de8994980fd0`.
- PR #8 `feature/offline-stratum-job-lifecycle-20261106` added explicit `--health-json` offline readiness output (`mode=offline-replay`, `g0=NOT PASSED`, `transport=unavailable`, `mining_ready=false`) and focused CTest asserting it does not claim mining readiness. Changed `tools/offline_stratum_replay.cpp` and `tests/job_state/CMakeLists.txt`; commits `559e4c6eba91d91facdf1f03fafab4e47213058c` and `1427751c2ed3d067034ab711b5769a7b69b953af`.
- New PR #8 head `1427751c2ed3d067034ab711b5769a7b69b953af` has no head-specific CI run yet at handoff. Local native build not run: container DNS cannot resolve github.com. Treat focused test as PENDING, not passed. No pool socket, live mining, share submit or Windows 11/GPU test.
- G0 **NOT PASSED**: no independently sourced full 32-byte HooHash V110 parity, no 10,000 independent cases, and no required Windows 11 native evidence. Next single task: confirm new PR #8 CI and implement a bounded localhost-only socket loopback request/reply transport test, without production pool connection.

## 2026-11-09 10:45+08 run update
- Main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b` (PR base); PR #8 started `1427751c2ed3d067034ab711b5769a7b69b953af`, ended `59144c7dc7989ef615a47f3b483220bfc9f44cad`. PR #2 pre-update `beec6dde01a4d5959ec95abaee6640c43d010ae5`.
- Previous PR #8 exact-head native CI run [42547105068](https://github.com/edisontw/pepepow-miner/actions/runs/42547105068) succeeded. New head-specific CI not yet reported; focused test PENDING. No local compile performed.
- PR #8 commits `59067db955aad9b9e00b0f8f697719c3a391584c`, `59144c7dc7989ef615a47f3b483220bfc9f44cad`: offline replay CLI now fails nonzero on unrecognized reply frames after an otherwise valid handshake, preventing misleading transcript PASS; added `--self-test-unexpected` and negative CTest. Files `tools/offline_stratum_replay.cpp`, `tests/job_state/CMakeLists.txt`. No socket transport added.
- G0 **NOT PASSED**: independent 32-byte HooHash parity and 10,000 independently sourced cases absent; Windows 11/GPU/pool untested. Next single task: check new head CI, then implement bounded localhost-only socket transport test with reconnect generation isolation.

## 2026-11-09 13:48+08 run update
- Main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 started `59144c7dc7989ef615a47f3b483220bfc9f44cad`, now `b52f5e7f1f36b0c6dad2d92e032aeb04bca4c686` (commits `dd8e587`, `4b0b29f`, `b52f5e7`). PR #2 prior head `610eead69e03676987e10a811f42669080d41948`.
- PR #8 keeps JSON-RPC request IDs monotonic across reconnects instead of resetting to 1, preventing a late reply with a reused ID from correlating to a new request. Updated controller regression and offline replay reconnect fixture for IDs 7/8 and 2/3 respectively. Paths: `include/pepepow/offline_stratum_controller.hpp`, `tests/test_offline_stratum_controller.cpp`, `tools/offline_stratum_replay.cpp`.
- Previous head `59144c7` native workflow run 42552410333 succeeded; **new head `b52f5e7` has no reported workflow run yet**, so focused cross-platform tests remain pending. No local compile performed. No live pool connection or share submit.
- G0 **NOT PASSED**: no independent full 32-byte V110 digest or 10,000 independent parity evidence; Windows 11 hardware and GPU untested. Next: check new-head native CI, then implement bounded localhost-only socket loopback transport with split replies and reconnect isolation. Do not repeat offline-only guard tests.

## 2026-11-09 14:46+08 run update
- Re-fetched main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 started `b52f5e7f1f36b0c6dad2d92e032aeb04bca4c686`, now `9c5cd56ec875f10ba6b2fdbf733d3ba48b1cf7aa`; PR #2 pre-status `49b4a6e588b4b18728df30a11b6e7b2ad5d94752`.
- Previous PR #8 exact-head native workflow `42569465520` completed success. New head-specific CI for `9c5cd56` not yet reported; focused native test **PENDING**. Local git clone still blocked by GitHub DNS resolution.
- PR #8 commits [babffb8](https://github.com/edisontw/pepepow-miner/commit/babffb8a5f454d50a26e9b9ccfc518c95a26ced3) and [9c5cd56](https://github.com/edisontw/pepepow-miner/commit/9c5cd56ec875f10ba6b2fdbf733d3ba48b1cf7aa): add `tests/test_stratum_socket_loopback.cpp` and register `stratum_socket_loopback` in `tests/job_state/CMakeLists.txt`. Native socket test binds OS-assigned 127.0.0.1 port only, exchanges subscribe/authorize over TCP, fragments subscribe reply, and rejects stale generation after reconnect. No external pool, hashing, share submit or mining.
- G0 **NOT PASSED**: independent full 32-byte HooHash parity, >=10,000 independent comparisons, Windows 11 native evidence absent. Next single task: inspect new-head CI and fix any compiler or socket framing failure before extending transport.

## 2026-11-09 15:52 Asia/Taipei — PR #8 TCP framing fix
- Main at start: `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 start `9c5cd56ec875f10ba6b2fdbf733d3ba48b1cf7aa`; PR #2 start `39fdfa6e35a8f1314a36625c81b06e55b9965538`.
- Head-specific PR #8 native run https://github.com/edisontw/pepepow-miner/actions/runs/42575161475 succeeded on Ubuntu 22.04/24.04 and Windows Server 2022 for the prior head.
- PR #8 commit `3c6b06736f452be54e2311d2f2d641cdb43e6e61` changes only `tests/test_stratum_socket_loopback.cpp`: request receive now accumulates exact bytes, rather than incorrectly assuming a single TCP recv returns the entire subscribe/authorize request. Existing two-part reply and reconnect checks remain. This is test reliability, not a production transport.
- New head-specific CI and local native compilation not yet verified at handoff. G0 **NOT PASSED**; independent 32-byte V110 parity and 10,000 independent cases absent; no Windows 11/GPU/real pool evidence.
- Next: inspect CI for `3c6b067`; if green, advance from socket smoke fixture to a small bounded localhost-only transport abstraction, with timeout and reconnect isolation, not more duplicate transcript tests.

## 2026-11-09 17:26+08 run update
- Verified main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b`. PR #8 started `3c6b06736f452be54e2311d2f2d641cdb43e6e61`; corresponding Actions run 42582122785 completed successfully (this is the PRE-change head). PR #2 started `abb06cb4bbe6bbf5f43b349b0e419bc353781e61`.
- PR #8 new commit `2d2c9b95ae34e75b5fd12ed27eb1dcb3a0e12c41` updates only `tests/test_stratum_socket_loopback.cpp`: a bounded TCP `send_all` loop now handles partial socket writes for subscribe, authorize and mock replies, rather than assuming one send writes the whole frame. Native CI for this new head is PENDING; local compilation was unavailable because github.com DNS lookup failed. Do not claim this change is tested yet.
- G0 **NOT PASSED**: no independently sourced 80-byte -> complete 32-byte V110 digest parity, no 10,000 independent comparisons, no Windows 11 native/GPU/real-pool evidence. No mining or external submit performed.
- Next one task: verify head-specific PR #8 native CI and fix failures; then move the tested bounded socket I/O into a reusable localhost-only transport primitive rather than adding more transcript fixtures.

## 2026-11-09 17:48+08 run update
- Main start/end `3ab4e56b16140f61f30d1af7ddc324d84040067b`; PR #8 start `2d2c9b95ae34e75b5fd12ed27eb1dcb3a0e12c41`, latest branch head `950315c6d27c6bb0e4644531ddb30626b7f2e3d4`; PR #2 pre-status head `781352b6e36ca6819d810c6175989a026c4b087f`.
- Previous PR #8 head `2d2c9b9` exact-head native Actions [42593530727](https://github.com/edisontw/pepepow-miner/actions/runs/42593530727) succeeded. New head-specific CI pending; local clone/compile blocked by github.com DNS resolution. **New change not yet validated.**
- PR #8 commits [46467e4](https://github.com/edisontw/pepepow-miner/commit/46467e4c3c1c4f9089102b61f63aef06d80cdd20), [c85c821](https://github.com/edisontw/pepepow-miner/commit/c85c821723bfe55705d599c51249825c77b823ce), [950315c](https://github.com/edisontw/pepepow-miner/commit/950315c6d27c6bb0e4644531ddb30626b7f2e3d4): add reusable `include/pepepow/stratum_socket_io.hpp` with bounded 64-KiB cross-platform socket send/receive and configurable send/receive timeouts; use it in `tests/test_stratum_socket_loopback.cpp`. No external network or mining.
- G0 **NOT PASSED**: independent full 32-byte V110 digest, >=10,000 independent parity cases and Windows 11 native evidence missing. No GPU or production pool test.
- Next: inspect new PR #8 exact-head CI and fix native build/socket failures; then connect bounded I/O to minimal localhost-only transport.

## 2026-11-09 18:45+08 run update
- Main observed `3ab4e56b16140f61f30d1af7ddc324d84040067b` (unchanged); PR #8 started at `950315c6d27c6bb0e4644531ddb30626b7f2e3d4`; its head-specific native workflow 42596297347 completed successfully.
- PR #8 now `5ebcaf3328f004f5cc9ff9768b1f14ca3ae738cb` (commits `d560294`, `5ebcaf3`): POSIX `send_all()` uses `MSG_NOSIGNAL` where available to prevent process termination on a broken socket; loopback test asserts a locally shutdown write fails without terminating. Windows keeps zero send flags. No live connection.
- New head-specific CI and local focused compile/run are pending; do not claim the new change passed. G0 independent 32-byte V110 digest and >=10,000 independent parity remain NOT PASSED; Windows 11/GPU/pool untested.
- Next: inspect CI for PR #8 head `5ebcaf3`, fix failures if any; then add bounded localhost-only transport read framing instead of further smoke-test duplication.
