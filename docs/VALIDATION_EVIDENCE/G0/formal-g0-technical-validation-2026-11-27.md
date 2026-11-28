# G0 Technical Validation Evidence — 2026-11-27

## Decision

**G0 Technical Validation: PASS.** This sign-off covers the pinned CPU HooHash
V110 implementation and the technical acceptance evidence below. It does not
state that every third-party source or future package is cleared for
distribution.

The current main snapshot is
`dfccd6fd2aeb3525cc41e27428537571642da6b7`. The CPU implementation and G0
inputs from the #30 integration head
`78664ab5c7a5d560cfc9b0f833189379e6fdcf32` remain unchanged through the
subsequent CUDA-only PRs #31–#33. The current main delta after the #33 merge is
documentation-only. Existing CPU evidence therefore applies without repeating
the Windows physical acceptance or the CPU hash matrix.

The Owner authorized formal G0 sign-off in [Issue #1 comment
7192595038](https://github.com/edisontw/pepepow-miner/issues/1#issuecomment-7192595038).
This records that sign-off for the technical gate only. The separate
Distribution License Compliance gate is tracked in
[`THIRD_PARTY_NOTICES.md`](../../../THIRD_PARTY_NOTICES.md).

## Technical acceptance evidence

| G0 technical condition | Evidence | Result |
| --- | --- | --- |
| Pinned Core/OpenLibm and reproducible build provenance | Core `bc08108ceccd957c79040dc7fedc1d56d8d47f97`; OpenLibm `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`; checked-in Linux Debug/Release dependency inventories and Windows Server artifacts | Pass; recorded |
| Independent full 32-byte parity, including at least 10,000 cases and chain vectors | Main integration Linux workflow [45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621), Ubuntu 22.04/24.04 Debug/Release; retained exact-feature-head 10,000-case and Explorer-vector evidence | Pass |
| Last-byte mutation and invalid-input failure behavior | Main integration Linux and Windows Server workflows [45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621) and [45824850719](https://github.com/edisontw/pepepow-miner/actions/runs/45824850719) | Pass |
| Ubuntu 22.04 x64 native Debug/Release | [Run 45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621), exact #30 integration head | Pass |
| Ubuntu 24.04 x64 native Debug/Release | [Run 45824850621](https://github.com/edisontw/pepepow-miner/actions/runs/45824850621), exact #30 integration head | Pass |
| Windows 11 physical technical acceptance | Owner record: 84/84 on `7284aa22cafd79e3521291307eca8f5b9a575744` | Pass; preserved, not repeated |
| Windows 10 technical acceptance | Owner equivalence to the Windows 11 result; no separate physical Windows 10 run | Accepted by Owner equivalence; no physical-run claim |
| Windows Server Debug/Release regression | [Run 45824850719](https://github.com/edisontw/pepepow-miner/actions/runs/45824850719) | Pass |
| Formal G0 Technical Validation sign-off | Owner authorization in Issue #1 comment 7192595038 and this evidence reconciliation | Recorded |

## Separate distribution gate

Distribution License Compliance remains pending for the exact future release
configuration, applicable notices, SBOM, runtime dependencies, and source
provenance. It is not an open condition for G0 Technical Validation and does
not block continued local builds or offline tests.

For HooHash development, the Owner adopts the public MIT `LICENSE` change at
Hoosat commit
[`9634f11410a2d71be21086e813263fa007fb6810`](https://github.com/HoosatNetwork/hoohash/commit/9634f11410a2d71be21086e813263fa007fb6810)
and the MIT HooHash headers in the pinned Core as the current evidence basis.
Preserve the older GPL-3.0-or-later upstream file headers, the MIT transition,
copyright notices, source SHA, and provenance. This technical sign-off makes
no blanket third-party distribution claim. The notice and release gate remains
[`THIRD_PARTY_NOTICES.md`](../../../THIRD_PARTY_NOTICES.md).

## Next gate

G1 LAB Pool acceptance may proceed under the Owner's existing authorization.
After this G0 sign-off, the Owner confirmed the Pool Mining endpoint and
supplied a test payout address. A subsequent bounded Windows 11 CPU/POOL run
completed 1,000 answered replies; 992 were accepted and 8 returned generic
rejections without a classifiable Pool error. The sanitized report and JSONL
are recorded on the open draft [PR #40](https://github.com/edisontw/pepepow-miner/blob/929d8b9f40cccfb8be6bde45c3213c4ac31bac55/docs/VALIDATION_EVIDENCE/G1/windows11-cpu-pool-acceptance-2026-11-27.md).
This post-sign-off run is one configuration only and does not pass G1. Linux,
SOLO, live reconnect/failover, other required configurations, and remaining G1
criteria are still pending. The payout address is intentionally omitted from
repository evidence.

Issue #1 remains open for the remaining product gates and distribution work.
