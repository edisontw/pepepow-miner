# PEPEPOW Miner — G0 handoff (2026-11-06)

## Authority and scope
- At handoff, main: `9e2c3c671f300ba8efaa72969b5172ad855328e8`. Re-fetch main SHA on every new run; do not assume it remains unchanged.
- README and AGENTS.md describe a planned product, not a functioning miner. Read docs/ROADMAP.md, docs/G0_TECHNICAL_VALIDATION.md, docs/RESEARCH_LEDGER.md and all three docs/research reports before consensus changes.
- G0 is **NOT PASSED**. Independent full 32-byte HooHash V110 expected digest, >=10,000 independent parity cases, native Windows/Linux evidence, and later GPU/pool testing remain outstanding.

## Branches and work
- PR #2: https://github.com/edisontw/pepepow-miner/pull/2 — Core reference/KAT, provisional pool candidate, digest export and comparator; candidate is NOT independently established consensus truth. The latest CI run at `f8e1614a` failed in one Python test (8 run, 7 passed, 1 error): `first, second = ...` unpacked three subprocess calls. Log: https://github.com/edisontw/pepepow-miner/actions/runs/42208988156 . Fixed in commit `d29d512ae95e4972178526b1ae8ff463807d253c`; CI for fix not yet verified at handoff.
- Draft PR #3–#7: nonce boundaries, portable 80-byte header nonce helpers, Stratum prevhash 4-byte word reversal, uint256 target comparison, compact nBits decoder. These are separate branches, not main. Check overlap and dependencies before merging.
- main remains documentation/planning oriented. Do not call PR functionality shipped.

## Critical assessment
- Repeated tests of the same Core output, or file alias protections, do not establish independent consensus parity. Avoid further expansion of self-consistency tests unless a real regression requires it.
- The pool's `verify_known_vector.c` compares only a digest prefix and does not establish a trustworthy full 32-byte golden vector. Preserve provisional provenance.
- Previous iterations reported a green earlier CI then left a newer CI failure; always inspect latest head-specific CI, not an older success.
- Focus on product capability: independently derived V110 digest, real Core/pool protocol semantics, then minimal native CLI and Stratum state machine with offline mock tests. No live submit without authorization.

## Next actionable run
1. Re-fetch main and all open PR heads, inspect latest CI for PR #2 commit `d29d512a`; fix failure if any.
2. Audit PEPEPOW Core and HooHash upstream exact source SHA and header/nonce/endianness. Produce one truly independent 80-byte header -> full 32-byte digest KAT with provenance, then scale parity to >=10,000 cases.
3. Keep changes small, use focused tests, no real wallet keys, no live mining, no production pool submit, no release, no automatic merge.
4. Report start/end main SHA, changed paths, commit/PR, actual test outcomes, remaining evidence and next task. Maintain this single handoff/status source rather than spawning duplicate progress documents.
