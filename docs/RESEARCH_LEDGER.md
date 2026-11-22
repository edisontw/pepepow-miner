# Research verification ledger

Reviewed on 2026-11-04 (Taiwan time). This static research review predates real HooHash execution, pool connections, and performance measurements. The [G0 implementation notes](G0_IMPLEMENTATION_NOTES.md) document later source corrections and native synthetic C frontend testing. VERIFIED-SOURCE means static source confirmation. PROVISIONAL requires tests. BLOCKED precludes production use based on the available evidence. No item in this snapshot had passed runtime validation.

## Pinned snapshots

- Miner main: [780d49568989415818f984e1141c06d465dc0dcb](https://github.com/edisontw/pepepow-miner/tree/780d49568989415818f984e1141c06d465dc0dcb), eight Markdown files and no product code, CI, or LICENSE.
- Core master: [38e801428688dfd0aee272b4a91819ac1a13ab4e](https://github.com/MattF42/PePe-core/tree/38e801428688dfd0aee272b4a91819ac1a13ab4e).
- Pool main: [c1298d7b40e5e9fd39f76af179c6e10a1c313147](https://github.com/edisontw/pool-pepepow/tree/c1298d7b40e5e9fd39f76af179c6e10a1c313147).

The branch heads matched the historical planning SHAs. These audit snapshots did not introduce the dependencies or approve their distribution.

## Research decisions

| Source | Proposal or conflict | Decision |
| --- | --- | --- |
| [01: technical risks and recommendation](research/01_CORE_TECHNICAL_RESEARCH.md) | CPU/Stratum before GPU. Forking ccminer could accelerate NVIDIA work. | Build our own host with a license-reviewed reference adapter. Keep GPU forks outside the MVP baseline. |
| [02: conclusions, risks, and priorities](research/02_ARCHITECTURE_PERFORMANCE.md) | C++/Qt, CUDA first, HIP primary | Adopt the C++20 host. Qt is conditional first choice. CPU CLI first. Defer the AMD ADR until hardware is available. |
| [03: B3, D1, D2](research/03_PRODUCT_DEVELOPMENT_PLAN.md) | Rust/Tauri option and G0–G5/M0–M9 | Consolidate architecture decisions and track status using G0–G5 only. |
| 01 GPU sampling/full recheck versus 02 per-candidate verification | Share submission safeguards differ | Verify the full digest of every GPU candidate on CPU. |
| 01 4-byte nonce versus 02 64-bit allocator | Wire format and allocator width differ | Use uint32 on wire, allow uint64 internally, and prevent nonce wrap. |
| 02 new generation versus 01 previous-job grace | Work may be discarded early or submitted after invalidation | Track scheduler generation separately from job submission eligibility and verify using pool fixtures. |
| 02 24h/72h versus 03 24h | Release gates differ | Use a 24h functional gate and 72h stable-release gate. Disclose remaining Beta gaps. |
| 03 P0/P1 versus original README operating modes | Automatic load tuning, APIs, and services expand the MVP | Keep G2 to manual profiles and event diagnostics. Defer automatic activity detection and remote administration. |

## Source evidence and engineering actions

Links pin SHAs and identify symbols. Rerun affected acceptance tests when sources or toolchains change.

| ID / status | Source and observation | Action or gate |
| --- | --- | --- |
| R01 VERIFIED-SOURCE | [Core spec](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/doc/hoohash_spec.md): 80-byte header, LE nonce at offsets 76–79, seed masking | Build byte-level G0 fixtures using the Core preimage. |
| R02 VERIFIED-SOURCE | [block.cpp GetPOWHash](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/src/primitives/block.cpp): 0x4000 HooHash has priority over 0x8000; digest is copied with memcpy after serialization | Pin version and header independently of reported fork heights. Verify chainparams activation separately. |
| R03 VERIFIED-SOURCE | [hoohash.c](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/src/crypto/hoohash/hoohash.c): hoohashv110 reverses at the end, selects HAVE_OPENLIBM conditionally, and fills invalid-length output with 0xff | Avoid a second reversal in the adapter. Record actual OpenLibm compiler configuration. The block.cpp invalid-length path fills zeros. Reject invalid input in the harness before interpreting any digest. |
| R04 VERIFIED-SOURCE / runtime pending | [dev_vectors.cpp](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/src/crypto/hoohash/dev_vectors.cpp): fixed seed generates 100 headers and calls the same hash function | Suitable as a corpus generator starting point. Independent expected values and chain provenance require separate evidence. Verify byte-swap comments against block.cpp. |
| R05 VERIFIED-SOURCE | [verify_known_vector.c](https://github.com/edisontw/pool-pepepow/blob/c1298d7b40e5e9fd39f76af179c6e10a1c313147/apps/pool-core/tools/verify_known_vector.c): full header and digest literals, eight-byte memcmp, return 0 on FAIL | Issue #1 tracks the candidate's Core origin and encoding. Require full KAT evidence and nonzero exit when the final byte is mutated. |
| R06 VERIFIED-SOURCE | [stratum_protocol.py](https://github.com/edisontw/pool-pepepow/blob/c1298d7b40e5e9fd39f76af179c6e10a1c313147/apps/pool-core/stratum_protocol.py): format_prevhash_for_stratum reverses each 4-byte word. Inspect notify/subscribe structure and both object and submit-array errors. | Pin complete notify-to-header80 fixtures and test both error shapes. |
| R07 VERIFIED-SOURCE / boundary pending | [stratum_ingress.py](https://github.com/edisontw/pool-pepepow/blob/c1298d7b40e5e9fd39f76af179c6e10a1c313147/apps/pool-core/stratum_ingress.py): STRATUM_DIFF1_TARGET_PEPEPOW_POOL = 0000ffff…; _share_target_from_difficulty computes int(D1 / difficulty) with a float input and clamps to [1, 2^256−1] | The floor formula in 01 summarizes the math. Compare exact integer/rational targets with pool rounding, test target−1/target/target+1 and tiny or nonfinite difficulty, and hold interoperability acceptance for unexplained differences. |
| R08 VERIFIED-SOURCE | In the same file, _manual_share_difficulty_from_password divides the sd= path by 65536 at a different layer from the miner-facing target baseline | Verify configuration-to-wire-to-target behavior with fixtures before applying scale to set_difficulty messages. |
| R09 Upstream relicensing verified / distribution review pending | The Core hoohash.c MIT header names Hoosat `9634f11410a2d71be21086e813263fa007fb6810`. Reports 01 and 02 differed in their description of upstream repository and file licensing. | Follow-up confirmed 9634f114 was the original author's GPLv3-to-MIT commit and classified the BLAKE3 differences. See the G0 implementation notes. Retain old header discrepancies and notices. OpenLibm file-by-file and final distribution review remain pending. |
| R10 PROVISIONAL | Report 02 claims on FP64, throughput, AMD Windows capability, and competitor performance. Report 03 claims on GUI/telemetry feasibility. | No repository hardware/build evidence at this snapshot. Verify each OS, device, and driver. Benchmark claims require local evidence. |
| R11 PROVISIONAL | Reports 01 and 02 on SafeComplexTransform, constants, NaN/Inf, and floating-point order | Compare each relevant symbol and intermediate value against pinned Core. The whole algorithm audit was pending. Consensus changes require independent evidence. |

## Evidence requirements

Record results under `docs/VALIDATION_EVIDENCE/<gate>/<run-id>/`: source manifest, fixtures, actual commands, stdout and stderr, exit codes, OS, CPU, compiler and flags, passed/failed/pending status, and limitations. The path did not yet exist at this snapshot.

Promote external claims only with a permalink and test evidence. The implementation PR for each gate owns that evidence.
