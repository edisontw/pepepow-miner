# G0-01 source differences and C harness frontend

Date: 2026-11-04. Follow-up to the [source inventory](G0_SOURCE_AUDIT.md). G0 had not passed at this snapshot.

## Upstream relicensing evidence

The upstream LICENSE history confirms that the Core-pinned revision [9634f11410a2d71be21086e813263fa007fb6810](https://github.com/HoosatNetwork/hoohash/commit/9634f11410a2d71be21086e813263fa007fb6810) is Toni Lukkaroinen's 2025-10-14 commit: “Change GPLv3 to MIT for more permissive algorithm usage.” The same name appears in the source authorship fields. The commit changed only LICENSE, from GPLv3 to MIT.

The commit establishes explicit upstream relicensing intent and explains why offline harness work could continue. The old headers were not updated. Retain the Core and Hoosat notices, the commit evidence, and the header discrepancy when introducing the Core reference. Product licensing and the dependency and binary distribution reviews remain separate decisions.

## Three BLAKE3 source differences

The comparison used `src/crypto/` from Core `38e801428688dfd0aee272b4a91819ac1a13ab4e` and `c/` from BLAKE3 `54930c95227daaac4dcf1eb3028e2f4e0768d139`. Pinned blob IDs are in the [manifest](sources/g0-source-manifest.json).

| File / symbol | Observed change | G0 implication |
| --- | --- | --- |
| blake3.c / chunk_state_update | Retains the final fill_buf call, drops its saved result and subsequent input/input_len updates, and fixes a duplicated word in a comment | The function exits immediately after the former updates. Static review found the updates unused. Vectors are still required. |
| blake3_impl.h | Excludes ARM64EC from IS_X86_64 and includes it in IS_AARCH64 | Platform classification only. ARM64EC support requires evidence. |
| blake3_dispatch.c | Three public dispatch functions call the portable implementation directly. simd_degree returns 1. Active CPU feature selection is removed and the MSVC include guard is adjusted. | Use Core's scalar portable baseline. Upstream SIMD throughput assumptions do not apply. |

This was a complete static file comparison. Preserve the pinned portable reference while producing independent expected values and measuring runtime parity.

Sources: [Core crypto](https://github.com/MattF42/PePe-core/tree/38e801428688dfd0aee272b4a91819ac1a13ab4e/src/crypto) and [BLAKE3 C](https://github.com/BLAKE3-team/BLAKE3/tree/54930c95227daaac4dcf1eb3028e2f4e0768d139/c).

## Frontend deliverables

- [tools/kat.c](../tools/kat.c) strictly parses 160/64 hex characters, compares all 32 digest bytes, exits 1 on mismatch, and exits 2 on invalid input.
- [CMakeLists.txt](../CMakeLists.txt) requires an explicit real reference. The synthetic target is a separate opt-in.
- [Frontend tests](../tests/harness/test_contract.py) cover 17 parser, comparator, and exit-code cases using a clearly labeled synthetic implementation.
- [Build and run instructions](../tools/README.md).

This delivery covered the frontend. Core linking, independent expected digests, and formal acceptance of the candidate KAT were pending. The synthetic failure tests cover frontend behavior only.

## Next steps recorded at this snapshot

1. Build the independent reference from pinned Core, portable BLAKE3, and OpenLibm 0.8.7. Retain source and notice evidence and reject system-libm fallback.
2. Check the candidate against the reference, record raw function bytes, RPC/canonical mapping and chain provenance, then pin the expected digest and use the harness.
3. Rerun the final-byte negative case with the real reference, then expand the corpus and native Windows and Ubuntu tests.

## Measured results

Native Windows x64 builds with Zig 0.15.2/clang 20.1.2 used -O0 and -O2. Each build passed all 17 frontend tests. Both builds and test processes exited 0. A final-byte mismatch exited 1. Invalid input exited 2 without calling the synthetic implementation.

See the [environment, commands, and complete log](VALIDATION_EVIDENCE/G0/frontend-contract-2026-11-04.txt). CMake configuration, Core hashing, and Linux execution were outside this run. G0 acceptance remained pending.
