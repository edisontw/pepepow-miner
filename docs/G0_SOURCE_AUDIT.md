# G0-01 source and candidate vector audit

Date: 2026-11-04. Baseline main: `9e2c3c671f300ba8efaa72969b5172ad855328e8`.
This records the initial source inventory. See the [implementation notes](G0_IMPLEMENTATION_NOTES.md) for the upstream relicensing commit, classification of the three BLAKE3 differences, and C harness frontend results. At this snapshot, the independent Core KAT and native hash parity were pending. Issue #1 remained open and G0 had not passed.

Follow-up, 2026-11-24: the current pinned-source reconciliation, full Windows OpenLibm translation-unit inventory, conservative license-signal triage, and redistribution boundaries are recorded in the [G0 provenance and license assessment](sources/G0_PROVENANCE_LICENSE_ASSESSMENT.md) and [current source manifest](sources/g0-source-manifest.json). G0 remains NOT PASSED.

## Deliverables

- [Source manifest](sources/g0-source-manifest.json): 20 pinned commit, path, and Git blob IDs. No third-party source was copied.
- [Candidate vector](../tests/vectors/candidates/pool-known-chain-001.json): 80-byte header, 32-byte pool canonical digest, and source symbol. The candidate is independent of the future miner. Core output and block height were deliberately left unset pending independent verification.
- The observations below are reproducible static findings. No hash execution was part of this snapshot.

## Findings

### HooHash license signals in the historical source

Core's hoohash.c points to Hoosat commit `9634f11410a2d71be21086e813263fa007fb6810`.
That revision's [LICENSE](https://github.com/HoosatNetwork/hoohash/blob/9634f11410a2d71be21086e813263fa007fb6810/LICENSE) is MIT, while [hoohash.c](https://github.com/HoosatNetwork/hoohash/blob/9634f11410a2d71be21086e813263fa007fb6810/hoohash.c) and [hoohash.h](https://github.com/HoosatNetwork/hoohash/blob/9634f11410a2d71be21086e813263fa007fb6810/hoohash.h) state GPL-3.0-or-later. Core's own COPYING and hoohash.c state MIT. This records the source observations without deciding legal precedence or compatibility.

[AGENTS.md](../AGENTS.md) requires source license verification before copying or distributing third-party code. This work did not vendor code, distribute binaries, or select a project license. The follow-up found the author's explicit GPLv3-to-MIT change. Retain that commit and the old headers together, and assess dependency and distribution notices individually. See the implementation notes.

### BLAKE3 1.5.1 source differences

Core blake3.h reports version 1.5.1. Git blob comparisons against upstream [54930c95227daaac4dcf1eb3028e2f4e0768d139](https://github.com/BLAKE3-team/BLAKE3/tree/54930c95227daaac4dcf1eb3028e2f4e0768d139) found:

| File | Byte-identical |
| --- | --- |
| blake3.h | Yes |
| blake3_portable.c | Yes |
| blake3.c | No |
| blake3_impl.h | No |
| blake3_dispatch.c | No |

The upstream license offers CC0-1.0 or Apache-2.0. See the implementation notes for the static classification. Runtime parity and complete notices remained pending at this snapshot.

### OpenLibm pin and fallback behavior

The Core [depends recipe](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/depends/packages/openlibm.mk) pins 0.8.7. The manifest records the archive SHA-256 from the recipe and the tag commit `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`. The archive was not downloaded or checksum-tested during this audit.

[OpenLibm LICENSE.md](https://github.com/JuliaMath/openlibm/blob/9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d/LICENSE.md) lists MIT, BSD, ISC, and FDLIBM notices, with LGPL terms for some tests. Classify notices according to the files actually built or distributed.

Core configure.ac warns and falls back to system libm when OpenLibm is absent. The G0 build must confirm HAVE_OPENLIBM, the linked library, and the actual compiler flags. Missing dependencies must fail explicitly. The src/Makefile.am settings include no-fast-math, fp-contract=off, and excess-precision=standard. Record the configure-detected x86 flags from the actual compile commands.

### Pool candidate comparison and encoding

- verify_known_vector.c compares the first eight bytes with memcmp and returns 0 even on FAIL.
- test_pepepow_header_hash_matches_known_chain_vector in tests/test_stratum_ingress.py compares all 64 hex characters.
- _calculate_pepepow_share_hash reverses native output once before returning canonical bytes. The C tool calls matrix multiplication directly. These API layers use different digest encodings.
- The test name refers to a chain vector. This audit had no height, raw RPC header, or independent Core execution output, so the candidate remained provisional. See the manifest for source permalinks and blob IDs.

## Results at this snapshot

The local PowerShell JSON and length checks exited 0:

`PASS: JSON parses; header=80 bytes; candidate digest=32 bytes; Core digest remains pending; 20 pinned source entries; no vendored code.`

Reproduction from the repository root in PowerShell:

```powershell
$ErrorActionPreference = 'Stop'
$v = Get-Content -Raw 'tests/vectors/candidates/pool-known-chain-001.json' | ConvertFrom-Json
$m = Get-Content -Raw 'docs/sources/g0-source-manifest.json' | ConvertFrom-Json
if ($v.header_hex -cnotmatch '^[0-9a-f]{160}$') { throw 'Invalid header' }
if ($v.expected_pool_canonical_hex -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid digest' }
if ($null -ne $v.expected_core_function_output_hex) { throw 'Unverified Core digest' }
foreach ($entry in $m.entries) {
    if ($entry.commit -notmatch '^[0-9a-f]{40}$' -or
        $entry.git_blob_sha1 -notmatch '^[0-9a-f]{40}$' -or $entry.vendored) {
        throw 'Invalid source entry'
    }
}
```

| Check | Result |
| --- | --- |
| JSON, lengths, and source ID format | Passed, format only |
| Core and pool branch recheck | SHAs matched the manifest |
| HooHash, BLAKE3, and OpenLibm license integration | Pending |
| C/CMake harness build, 32-byte KAT, and final-byte negative test | Pending, unrun |
| Windows 11 and Ubuntu 22.04/24.04 native parity | Pending |
| Pool and GPU testing | Unrun, outside this work package |

PATH did not contain cmake, clang, gcc, or cl. Common Visual Studio, LLVM, MSYS2, and CMake locations contained no installation. This was a limited environment survey. No system tools were installed and no other platform was substituted for native evidence.

## Next steps recorded in the audit

1. Review the three BLAKE3 differences and HooHash lineage, gather file-level notices and license decisions, and seek rights-holder clarification if needed. This audit did not contact external parties.
2. Establish a reproducible native C/CMake toolchain and independent Core reference. Record OpenLibm, compiler, and flags.
3. Obtain independent Core output for the candidate header, confirm raw-to-canonical mapping and chain provenance, and then promote the candidate to a formal KAT.
4. Compare all 32 bytes, require nonzero exit for a final-byte mutation, and reject invalid hex, wrong lengths, and missing input before expanding to 10,000 cases.

See the implementation notes for later frontend and toolchain evidence. Frontend results alone do not complete G0-01.
