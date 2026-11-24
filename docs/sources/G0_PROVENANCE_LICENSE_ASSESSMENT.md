# G0 source provenance and notice assessment

Date: 2026-11-24. Status: technical inventory recorded; redistribution review pending.

This document separates reproducible build provenance from legal conclusions. A matching digest, a successful build, or a license keyword does not establish permission to redistribute a source file or binary.

## OpenLibm Windows portable-C build

The audited upstream is [JuliaMath/openlibm](https://github.com/JuliaMath/openlibm/tree/9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d), commit `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d` (v0.8.7). The Windows evidence is GitHub Actions run [45181410916](https://github.com/edisontw/pepepow-miner/actions/runs/45181410916), a Windows Server 2022 x64 UCRT64 run, not a Windows 11 host run.

Both artifact ZIPs were downloaded and matched their GitHub artifact checksums. Their compile databases have these SHA-256 values:

| Configuration | Artifact ID | ZIP SHA-256 | Compile database SHA-256 |
| --- | ---: | --- | --- |
| Debug | 14820563640 | `6796680bfcc0ea47408b9e2385b06a1c5a23d15a229b3db2f124838f4d80e249` | `f78ec6131bb4d3204c71f2e63f2641f3066a096e9402a1dd8056ee82da8eb375` |
| Release | 14821916780 | `b48e8d643a592733854aaa0d8abefb24e01c07230827bf23e968b29b031238fd` | `cf9e66249af5707d462b6a1b922011591674e4ff539e26c11a92222b5af84e70` |

The two databases each contain 254 unique translation units; their path sets are identical. Counts by source subtree are `src/` 229, `ld80/` 21, `bsdsrc/` 3, and `amd64/` 1. No `test/` path appears. The complete generated record, including pinned Git blob SHA-1, source-byte SHA-256, source URL, header signals, and conservative candidate status, is [openlibm-windows-compile-inventory.json](openlibm-windows-compile-inventory.json), SHA-256 `727fca0055538b5580b9f76b058828dab9ee6d52d668b796ea776a5945a6e201`.

To reproduce the checked-in inventory from the downloaded run artifacts, verify the two ZIP checksums above, extract each `openlibm-compile-commands.json`, and run from the miner repository root:

```bash
git clone https://github.com/JuliaMath/openlibm.git /tmp/openlibm-inventory-source
git -C /tmp/openlibm-inventory-source checkout --detach 9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d
python3 tools/test_g0_openlibm_inventory.py -v
python3 tools/g0_openlibm_inventory.py \
  /tmp/openlibm-inventory-source \
  /path/to/debug/openlibm-compile-commands.json \
  /path/to/release/openlibm-compile-commands.json \
  > docs/sources/openlibm-windows-compile-inventory.json
sha256sum docs/sources/openlibm-windows-compile-inventory.json
```

The generator rejects other database checksums, non-254 row inputs, duplicate or noncanonical paths, path-set changes, and a non-pinned Git HEAD. The two test-source entries and root `LICENSE.md` are separately retrieved from that same pinned Git object database.

The Windows patch is [openlibm-portable-c-windows.patch](../../.github/patches/openlibm-portable-c-windows.patch). Its repository LF bytes hash to `9f37e2bb0e5c8160353bf7a624829eb2ffd14175495ae2e1a13a0c77739e0bb1`; the CRLF checkout bytes observed by the Windows workflow hash to `f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2`. The patch changes only `CMakeLists.txt`, to disable ASM selection for the portable-C build. The upstream CMake blob is `81045e25978ebb96001fd4b82ec3c658e8402257` (raw SHA-256 `36f50975ccf10f20daace4e8798d21cc6967189c00ab418b75a8c67a69ed6c1c`); the patched file reconstructed by applying the pinned patch hashes to `861e1e4eb190cc5a3425dde3de534308a90e1c8490b504ff62c1aea5711c689c`. No C/C++ translation-unit bytes are changed by this patch. Per-file source hashes in the inventory are reconstructed from the pinned Git objects, not raw files preserved from the historical runner checkout.

The historical artifacts contain the compile databases, FP flag records, symbol summaries, and binary checksums. They do **not** contain compiler `.d` dependency files, a Ninja dependency dump, the archive member list, the archive itself, or a linker map for the KAT executable. Symbol evidence confirms OpenLibm math symbols in the archive and `exp`/`sin` in the KAT, but does not identify every archive object linked into that executable. Included-header hashes and final object membership therefore remain unverified. The 254-unit list describes this Windows portable-C target only; Linux source selection and other platform variants need separate inventories.

The pinned root [LICENSE.md](https://github.com/JuliaMath/openlibm/blob/9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d/LICENSE.md) describes Julia Project MIT, Stephen L. Moshier ISC, FreeBSD BSD, FDLIBM, and LGPL notices for the test sources. The generated inventory detects per-file notice signals rather than assigning legal status:

- 128 translation units show an FDLIBM/Sun notice signal.
- 42 show a Moshier notice mapped to the ISC section of the pinned root notice.
- 70 show a BSD-style redistribution notice.
- 3 show a public-domain signal.
- Counts overlap: two files show both FDLIBM and Moshier signals; `src/s_modfl.c` shows FDLIBM and BSD-style notices.
- 17 translation units are `REVIEW_REQUIRED`: those three overlapping files, 13 with no recognized header signal, and `src/s_sincosl.c` with a custom permission notice that does not identify a license category.
- Single-signal entries are marked `EVIDENCE_ONLY`; none is cleared for redistribution. The MIT notice in the root file is not assumed to cover every translation unit with no individual notice.

`test/test-double.c` and `test/test-float.c` carry LGPL-2.1-or-later notices in the pinned tree. Neither appears in either archived library compile database, and the Windows workflow built the OpenLibm library and KAT rather than an OpenLibm test target. The CI run produced evidence artifacts, not a release package. The historical artifacts lack a linker map, so they do not establish general package membership beyond this test-source/build-target observation. Their notices remain in scope for any future test distribution; their presence in the repository alone does not establish inclusion in a miner executable.

## HooHash lineage

The pinned reference is [PePe-core](https://github.com/MattF42/PePe-core/tree/bc08108ceccd957c79040dc7fedc1d56d8d47f97), commit `bc08108ceccd957c79040dc7fedc1d56d8d47f97`. Its `COPYING` is MIT. Core's `src/crypto/hoohash/hoohash.c` and `.h` have MIT headers crediting Hoosat Oy and PePe-core developers and cite Hoosat commit `9634f11410a2d71be21086e813263fa007fb6810`.

At that Hoosat commit, authored by Toni Lukkaroinen on 2025-10-14, the message is “Change GPLv3 to MIT for more permissive algorithm usage.” The complete file list for the commit is just `LICENSE`: a 674-line GPLv3 text was replaced by a 21-line MIT text. `hoohash.c` and `hoohash.h` were byte-identical to their parent at this commit (Git blobs `bddc52cb4cd92064398d5eb5e3f1d4a2798a2f4c` and `c276493cf512a6a9919aeecfcf73141328188014`), and both still contain GPL-3.0-or-later header wording. The public commit search on 2026-11-24 returned this as the repository's latest commit; no later public header correction was found. The source history includes multiple contributors; the public history by path lists Toni Lukkaroinen, TMKCodes, and Tonto, which is not itself evidence of the scope of any grant.

Core's HooHash implementation is an adaptation, not a byte-for-byte copy: a static diff against the pinned Hoosat `hoohash.c` reports 497 changed lines (154 insertions, 343 deletions). This does not resolve the upstream header/root-license mismatch or establish that the MIT change was authorized by every relevant rightsholder. Keep the Core and Hoosat attribution and permission notices intact. Before product redistribution, obtain answers from the relevant rightsholders to these questions:

1. Does the MIT `LICENSE` change at `9634f11410a2d71be21086e813263fa007fb6810` expressly apply to `hoohash.c` and `hoohash.h` despite their unchanged GPL-3.0-or-later headers?
2. Was that relicensing authorized for all copyrighted contributions in those files, including contributions by other listed authors?
3. Are the GPL headers stale, and will the upstream project update them or provide a written clarification identifying the intended license for each file?
4. Does PePe-core have permission to distribute its adapted HooHash files under the MIT headers present at the pinned Core revision?

No upstream contact was made and no legal conclusion is recorded.

## BLAKE3 and pool candidate

Core's BLAKE3 source identifies version 1.5.1. The static comparison is against [BLAKE3 C](https://github.com/BLAKE3-team/BLAKE3/tree/54930c95227daaac4dcf1eb3028e2f4e0768d139). `blake3.h` and `blake3_portable.c` have identical Core/upstream Git blobs; `blake3.c`, `blake3_impl.h`, and `blake3_dispatch.c` differ. The reviewed differences are summarized in [G0_IMPLEMENTATION_NOTES.md](../G0_IMPLEMENTATION_NOTES.md): the Core `chunk_state_update` simplification, ARM64EC platform classification, and portable-only dispatch. The upstream BLAKE3 repository offers CC0-1.0 or Apache-2.0; the Core root `COPYING` is MIT, which is not by itself a per-file BLAKE3 notice map. Preserve BLAKE3 attribution/notices and leave dependency distribution review pending.

The pool reference is [pool-pepepow](https://github.com/edisontw/pool-pepepow/tree/c1298d7b40e5e9fd39f76af179c6e10a1c313147), commit `c1298d7b40e5e9fd39f76af179c6e10a1c313147`. Its `apps/pool-core/tools/verify_known_vector.c` (blob `2627e90746b3f838eb90df49c8189a6673bef211`) and `tests/test_stratum_ingress.py` contain the same candidate. The C helper compares only the first eight output bytes; it is not a 32-byte oracle. The pool tree has no root `LICENSE`, `COPYING`, or `NOTICE` file at that commit, and these candidate/test files do not establish their own redistribution grant. Keep their source licensing as `REVIEW_REQUIRED`.

The candidate header and full-digest mapping were independently checked by the pinned Core in PR #21 run [45361879558](https://github.com/edisontw/pepepow-miner/actions/runs/45361879558):

```text
header_hex = 0040002038e31388c54124146478ff691985eecd02610db91efbc9cd7aabca490000000007647f0508057dbf8c99ddaa87543c04e31dfe3f383e7386903d50c91728fabe830be16971e3021da96d9d33
pool display digest = 00000001fb895a82973fca52938848908d6a6cb3c0dfb93995dc61020ced0a6b
Core raw digest = 6b0aed0c0261dc9539b9dfc0b36c6a8d9048889352ca3f97825a89fb01000000
```

The run verified the full 32-byte reversed display/raw mapping and rejected direct display-order bytes. This is a pinned-Core mapping check, not independent consensus parity or a claim about redistribution rights.

## Redistribution review status

The source manifest and file inventory are technical provenance records. Redistribution review remains pending for OpenLibm per-file notices, Hoohash lineage, BLAKE3 attribution and modifications, pool-derived test material, platform-specific linked members, and the final product notice set. Do not treat a green CI run as legal clearance. No third-party source was copied into this assessment, and no release binary was produced or published.
