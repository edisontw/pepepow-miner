# CUDA Stratum Search Integration

## Current scope

The opt-in `pepepow-live-miner` executable can launch the standalone bounded
CUDA HooHash search engine for a live Stratum job. CPU remains the default
device. The existing `pepepow-live-cpu-miner` target is retained.

The adapter sends one bounded nonce range to a child process, captures its
JSON result, validates the full result contract, and rechecks every candidate
through the in-memory pinned-Core CPU worker pool. A candidate is returned to
the Stratum session only when nonce, header, complete 32-byte digest, and
target all agree. Job replacement and stop requests create the engine's
cancel-file signal. A five-second grace period is followed by cleanup limited
to the child process and descendants created for that invocation. Per-run
temporary files are removed by the parent.

The CUDA engine currently owns a short-lived device context for each search
request. Persistent device contexts and matrix caching across Stratum jobs are
not part of this adapter. No consensus hashing code or floating-point flags
were changed.

## Build and explicit invocation

Build the root project with its existing pinned-Core and OpenLibm inputs and
with the live miner explicitly enabled. Build the separate CUDA project with
`PEPEPOW_CUDA_BOUNDED_SEARCH_TESTS=ON` to produce
`pepepow-cuda-hoohash-search` and the pinned-Core digest oracle. The CUDA
toolchain remains outside the default CPU build.

Example command shape:

```text
pepepow-live-miner --mine \
  --endpoint stratum+tcp://HOST:PORT \
  --login ADDRESS.WORKER --extranonce2 00000000 --mode POOL \
  --threads 1 --responses 1 --max-runtime-seconds 30 \
  --device cuda --cuda-engine PATH_TO_CUDA_ENGINE \
  --cpu-oracle PATH_TO_PINNED_CORE_DIGEST_ORACLE \
  --cuda-batch-size 128
```

This command opens a network connection only when explicitly invoked with
`--mine`. Do not use it for a production pool or real-share acceptance until
the separate G0/G1 gates authorize that stage. The Windows/Linux mock tests do
not connect to a pool and do not submit shares.

## CPU-only contract evidence

`cuda_search_process_contract` launches a CPU mock executable and covers a
candidate at `UINT32_MAX`, complete digest CPU recheck, a mutation in the last
digest byte, cooperative cancellation, stop latency, and temporary-directory
cleanup. This validates the process and JSON contract; it is not physical CUDA
device evidence, GPU parity, a Stratum pool test, or G4 acceptance.

Live CLI contract tests require both CUDA paths when `--device cuda` is
selected and reject CUDA paths when the default CPU device is selected.

If the CUDA search child exits unsuccessfully or returns an invalid result,
the live miner emits a JSON `search_error` event with `device: "cuda"`,
`code: "engine_failure"`, and a bounded, JSON-escaped diagnostic message.
The same diagnostic is written to stderr for interactive use. The Windows and
Linux loopback contract test injects a child process that exits with an error;
it verifies the event parses, quotes and backslashes survive JSON decoding,
control bytes are sanitized, the run fails, and no candidate or share
submission is produced. This test uses only a local mock Stratum peer and does
not initialize CUDA or contact a pool.

The CUDA-capable loopback contract also replaces a job with `clean_jobs=true`
while the first mock engine is active. The mock waits for the adapter's
cancel-file signal, returns a cancelled result, and records each engine start.
The test verifies that the old job is classified as stale, the replacement
job starts, and no candidate or submit event is emitted. The mock pool closes
the connection after the replacement search starts to bound the test. This
covers the live session-to-child cancellation path without a GPU.

The same contract starts a fresh CUDA child after a local primary endpoint
drops the connection, then retries the configured fallback endpoint. It checks
that both children observe cancellation, the new Stratum handshake starts a
search for the fallback job, the mode remains `POOL`, and no candidate or
share is sent. The pinned-Core Linux workflow and Windows native workflow run
these CPU-only product loopback cases without a CUDA toolkit or device.
## CPU-only candidate submission loopback (2026-11-28)

The CUDA-capable product loopback now drives the successful candidate path as
well as failure, cancellation, and reconnect paths. Its child-process fixture
uses the pinned PEPEPOW Core CPU implementation to find a target-meeting nonce
and emit the bounded-search JSON contract. The live adapter verifies the
child's nonce range, recomputes the complete 32-byte digest and target with its
pinned-Core CPU worker pool, and only then emits and submits the candidate.

The local mock asserts the complete 80-byte header, full digest field, little-
endian nonce bytes in the header, the Stratum submit job/extranonce/time/nonce
fields, and exactly one mock accepted reply. The fixture deliberately returns
nonce 1 so the byte-order and submit-format checks are nonzero.

Windows 11 Release passed the expanded product loopback three consecutive
times; Debug passed once. This is a local TCP mock using CPU code only. It did
not initialize CUDA, allocate VRAM, contact a public pool, or submit a real
share. It is not physical GPU validation or G4 acceptance.

## Candidate returned after cancellation (2026-11-28)

A dedicated CPU-only child fixture waits for the live adapter's cancel-file
signal, then deliberately returns a syntactically valid, target-meeting
candidate instead of a cancelled result. The Stratum loopback replaces the
active job with clean_jobs=true while this child is running. The product must
classify the old work as stale and suppress both candidate and submit events;
the replacement job starts with a fresh child. The test uses a valid pinned-
Core digest and exercises the cancellation/result race without CUDA.

## Native Windows RTX 3060 loopback (2026-11-28)

The strict opt-in [`test_native_cuda_stratum_loopback.py`](../tests/test_native_cuda_stratum_loopback.py)
first invokes the real bounded CUDA engine directly and requires a successful
device result naming an NVIDIA GPU. It then runs `pepepow-live-miner` in
`POOL` mode against a loopback-only Stratum peer, using the same built CUDA
engine and pinned-Core oracle. The test fails on missing device initialization;
it does not replace the engine with a mock or accept a no-device skip.

On Windows 11 with an RTX 3060, compute capability 8.6, CUDA 13.3.1, the engine
found nonce `00000000`. The miner emitted the candidate only after its complete
32-byte CPU recheck. A separate pinned-Core oracle invocation independently
recomputed the candidate digest, then the loopback peer received the matching
`mining.submit` request and returned a mock accepted response. The run searched
one 128-nonce batch and stopped at the one-response cap. The nonce, full header,
digest, submit fields, device preflight, and event stream are recorded in the
[native loopback evidence report](VALIDATION_EVIDENCE/G4/cuda-stratum-loopback-windows11-2026-11-28.md).

This is local mock evidence only. It did not contact a public or production
pool, use a payout address, or submit a real share. It covers the successful
candidate path; separate Windows/Linux acceptance and authorized live-pool
testing remain pending.
