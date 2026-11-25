# G1 loopback transport, reconnect, and CPU worker evidence

## Scope

This report records offline implementation validation after PRs #22, #24, and #25. All socket tests use numeric IPv4 loopback addresses and local mock peers. No production pool or real share was used. These results do not constitute formal G0 or G1 acceptance.

Pinned inputs remained PEPEPOW Core `bc08108ceccd957c79040dc7fedc1d56d8d47f97` and OpenLibm v0.8.7 `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.

## Transport deadlines

PR #22 head `37d263554233559635ae8f44ae776434bfcc0233` integrates PR #23's frame-deadline patch. The original implementation reproduced both review findings:

- Slow-drip partial JSONL bytes prevented the miner from observing a completed or expired search.
- Continuous valid difficulty notifications prevented the pending submit from expiring.

The regression tests pass with absolute monotonic frame and request deadlines. A frame deadline is limited by a shorter applicable request or mining deadline. Subscribe, authorize, and submit responses each have bounded waiting. Partial frames and pending request IDs are discarded when a transport generation is retired. A rejected authorization does not emit an authorized event.

Additional loopback cases cover partial frame followed by disconnect, delayed or cross-generation responses, stale candidates, successful framing, and shutdown during pending transport activity. Linux root CTest passed 2/2 in Debug and Release. Linux job-state tests passed 22/22 and preflight tests passed 16/16 in both configurations. Windows Server 2022 native UCRT64 Debug and Release G1 steps passed.

## Reconnect and failover

PR #24 head `c06c292bd6ea13db85eaf7b860fddbacc0e7456b` passed its exact-head Linux and Windows Server checks.

Local mock tests cover disconnect and recovery, retry exhaustion, bounded exponential backoff, cancellation during backoff, failover to a second loopback port, fresh subscribe and authorize, stale replies, repeated work and duplicate-submission suppression. The configured login and POOL/SOLO mode are carried across the same-mode loopback endpoints.

## CPU workers and concurrency inspection

PR #25 head `a077384f429bdf55efde5669c73dd51d484050b4` passed exact-head Linux pinned-Core, independent 10,000-case parity, and Windows Server Debug/Release workflows.

The native test compares full 32-byte digests at 1, 2, 4, and 8 workers. It checks non-overlapping nonce coverage, inclusive target acceptance, target rejection, overflow rejection after `UINT32_MAX`, rapid job replacement, stale-candidate rejection, and shutdown while workers are active. A focused GCC ThreadSanitizer run passed 1/1. Source inspection of pinned HooHash, portable BLAKE3, OpenLibm, linked mathematical symbols, and floating-point behavior is documented in [the HooHash worker audit](hoohash-v110-worker-audit-2026-11-25.md).

A synchronous Core hash call cannot be interrupted mid-call. The worker pool observes cancellation between calls. The ThreadSanitizer result supplements source inspection and targeted parity testing.

## Local commands

The native miner build used the pinned Core and OpenLibm source directories. For each Debug and Release configuration:

```sh
cmake -S . -B build/reference -DCMAKE_BUILD_TYPE=Debug \
  -DPEPEPOW_NATIVE_STRATUM_SEARCH=ON \
  -DPEPEPOW_CORE_SOURCE_DIR=/path/to/PePe-core \
  -DPEPEPOW_CORE_EXPECTED_SHA=bc08108ceccd957c79040dc7fedc1d56d8d47f97 \
  -DOPENLIBM_INCLUDE_DIR=/path/to/openlibm/include \
  -DOPENLIBM_LIBRARY=/path/to/openlibm-build/libopenlibm.a
cmake --build build/reference --parallel 2
ctest --test-dir build/reference --output-on-failure
```

Focused ThreadSanitizer commands and benchmark details are in the [worker audit](hoohash-v110-worker-audit-2026-11-25.md). The benchmark values describe local synthetic-header hashing under a cloud cgroup CPU quota and are not production-pool hashrate.

## Exact-head GitHub Actions

- PR #22 head `37d263554233559635ae8f44ae776434bfcc0233`: [Linux pinned-Core tests](https://github.com/edisontw/pepepow-miner/actions/runs/45563709908), [Windows Server Debug/Release tests](https://github.com/edisontw/pepepow-miner/actions/runs/45563710032), [job-state matrix](https://github.com/edisontw/pepepow-miner/actions/runs/45563709920).
- PR #24 head `c06c292bd6ea13db85eaf7b860fddbacc0e7456b`: [Linux pinned-Core tests](https://github.com/edisontw/pepepow-miner/actions/runs/45588376421), [Windows Server Debug/Release tests](https://github.com/edisontw/pepepow-miner/actions/runs/45588376319).
- PR #25 head `a077384f429bdf55efde5669c73dd51d484050b4`: [Linux pinned-Core tests](https://github.com/edisontw/pepepow-miner/actions/runs/45589391208), [independent 10,000-case parity](https://github.com/edisontw/pepepow-miner/actions/runs/45589391352), [Windows Server Debug/Release tests](https://github.com/edisontw/pepepow-miner/actions/runs/45589391104), [frontend contract](https://github.com/edisontw/pepepow-miner/actions/runs/45589391051).

The listed GitHub Actions runs completed successfully, and their `head_sha` values matched the corresponding feature heads above. These GitHub runs used Windows Server 2022 hosted runners. They do not establish physical Windows 10 or Windows 11 acceptance. G0 source-license and NOTICE review and formal G0/G1 Owner sign-off remain pending. Issue #1 remains open.
