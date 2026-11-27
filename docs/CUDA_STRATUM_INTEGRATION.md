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
the run fails, and no candidate or share submission is produced. This test
uses only a local mock Stratum peer and does not initialize CUDA or contact a
pool.

The CUDA-capable loopback contract also replaces a job with `clean_jobs=true`
while the first mock engine is active. The mock waits for the adapter's
cancel-file signal, returns a cancelled result, and records each engine start.
The test verifies that the old job is classified as stale, the replacement
job starts, and no candidate or submit event is emitted. The mock pool closes
the connection after the replacement search starts to bound the test. This
covers the live session-to-child cancellation path without a GPU.
