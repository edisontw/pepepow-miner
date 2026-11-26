# G0 Evidence Reconciliation — 2026-11-26

## Decision

**G0 remains NOT PASSED.** The current main head is
`a505730696936117b2fa67c7a02b7ae4f1443856`. The CPU implementation and G0
inputs from the #30 integration head `78664ab5c7a5d560cfc9b0f833189379e6fdcf32`
are unchanged through #31–#33: the intervening diff adds only CUDA tools,
CUDA tests, CUDA workflows, and their evidence. Existing CPU evidence therefore
still applies without repeating the physical Windows acceptance or CPU hash
matrix.

The Owner authorized completing G0 sign-off in [Issue #1 comment
7192595038](https://github.com/edisontw/pepepow-miner/issues/1#issuecomment-7192595038).
That authorization does not grant third-party rights. The repository's
conservative provenance decision still blocks a G0 pass while material
rightsholder/notice questions remain unresolved.

## Acceptance evidence

| G0 condition | Evidence | Result |
| --- | --- | --- |
| Pinned Core/OpenLibm and native toolchain evidence | Core `bc08108ceccd957c79040dc7fedc1d56d8d47f97`; OpenLibm `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`; Linux dependency inventories and Windows Server artifacts | Recorded |
| Independent full 32-byte parity, including at least 10,000 cases | Main integration Linux workflow [45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621), Ubuntu 22.04/24.04 Debug and Release; prior exact-head 10,000-case and Explorer-vector evidence retained | Pass |
| Last-byte mutation and invalid-input failure behavior | Main integration Linux and Windows Server workflows [45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621) and [45824850719](https://github.com/edisontw/pepepow-miner/actions/runs/45824850719) | Pass |
| Ubuntu 22.04 x64 native Debug/Release | [Run 45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621), exact #30 merge commit | Pass |
| Ubuntu 24.04 x64 native Debug/Release | [Run 45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621), exact #30 merge commit | Pass |
| Windows 11 physical technical acceptance | Existing Owner record: 84/84 on `7284aa22cafd79e3521291307eca8f5b9a575744`; Windows 10 remains Owner equivalence, without a separate physical run | Preserved; not repeated |
| Windows Server Debug/Release regression | [Run 45824850719](https://github.com/edisontw/pepepow-miner/actions/runs/45824850719) | Pass |
| Source/notice review and distribution status | [Provenance assessment](../../sources/G0_PROVENANCE_LICENSE_ASSESSMENT.md), [source manifest](../../sources/g0-source-manifest.json), [Third-party notices](../../../THIRD_PARTY_NOTICES.md) | **Open; blocks G0** |
| Formal G0 sign-off | Owner has authorized sign-off once conditions are met; unresolved rights remain | **Not issued** |

## Blocking source-rights questions

- Pinned PEPEPOW Core's MIT-root/HooHash lineage includes upstream `hoohash.c`
  and `hoohash.h` with GPL-3.0-or-later headers. No written clarification from
  all relevant rightsholders is recorded.
- Core's BLAKE3 files contain changes relative to upstream. Preserve the
  upstream license-choice text, notices, modifications, and provenance pending
  review.
- OpenLibm's per-file notice inventory has 17 translation units marked
  `REVIEW_REQUIRED`; source-level signal detection is not a license grant.
- The pool-derived candidate has no identified redistribution grant. Keep it
  out of release bundles unless permission is documented or the test material
  is replaced with independently generated data.

Owner approval is not third-party authorization. The project remains local-test
only for affected components; no source bundle or binary containing them may be
published until the documented rights gate is cleared. Issue #1 remains open.

## Next gate

Do not start G1 live-pool acceptance until G0 passes. After G0, use only the
verified PEPEPOW LAB Pool endpoint, an explicitly configured Owner test payout
address, bounded CPU threads/time, the minimum answered-share counts, and
complete JSONL evidence. No live endpoint was contacted in this audit.
