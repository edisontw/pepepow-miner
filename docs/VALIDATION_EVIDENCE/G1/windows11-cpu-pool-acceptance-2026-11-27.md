# Windows 11 CPU Pool acceptance evidence

**Result: one configuration completed; G1 remains NOT PASSED.** This record
covers Windows 11 x64 CPU mining in POOL mode only. It does not qualify Linux,
SOLO, CUDA, reconnect/failover against the live Pool, or the full G1 gate.

## Run

- Date: 2026-11-27, Asia/Taipei.
- Pool: `stratum+tcp://pool.pepepow.net:39333`; Pool mode; cleartext TCP.
- Payout address: Owner-supplied address; intentionally omitted from this
  report, the JSONL, and the committed changes.
- Implementation commit: `505673726049d5e0f02e654f5f56683e4f683c42`.
- Pinned Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- Windows 11 Pro, build 26300; AMD Ryzen 5 7600X, 12 logical processors.
- NVIDIA RTX 3060 (12 GiB, compute capability 8.6, driver 610.88) was present
  but not used by this CPU-only run.
- MSYS2 UCRT64 x64: GCC 16.2.0, CMake 4.5.1, Ninja 1.13.2, OpenSSL 4.1.0;
  native CPython 3.13.17.
- Release executable; one CPU worker; 1,000 answered-response goal; 1,800
  second hard runtime limit; 1,000 ms minimum submit interval; at most three
  same-endpoint retries. The run completed on one connection, so no retry was
  used.
- Start: `2026-11-27T21:34:42.9293569+08:00`.
- End: `2026-11-27T21:51:30.9995359+08:00` (1,008.070179 seconds).
- Pool preflight returned `status=ok`, `poolStatus=online`,
  `templateModeEffective=daemon-template`,
  `shareHashValidationMode=hoohashv110-pepew-header80`, `degraded=false`,
  `stale=false`, daemon/template RPC reachable, and TLS disabled.

## Outcome

- Subscribe/authorize succeeded; one connection and one authorization.
- Answered submit replies: **1,000** — 992 accepted, 8 rejected, 0 Pool errors.
- The 8 rejected replies were generic `result=false` responses with no Pool
  error object. Their cause cannot be classified as invalid, low difficulty,
  duplicate, or stale from the returned protocol data. Explicit Pool error
  message classification counters were all zero.
- 50 stale candidates were discarded after job replacement before submission;
  these are not stale share replies.
- 107,958 hashes; miner-reported local search rate 8,682.250222 H/s. The
  measured intervals include search orchestration and cancellation time; this
  is not process CPU-time throughput or a profiling result.
- Accepted difficulty-weighted rate: 96.736884 H/s, calculated from accepted
  wire difficulty divided by elapsed wall time. This reflects the configured
  submit throttle and is distinct from local CPU search rate.
- Exit code 0, `stop_reason=response_goal`; stderr was empty. All 5,340 JSONL
  lines parsed as JSON. The payout address was absent from the captured output.

The complete JSONL transcript is
[`windows11-cpu-pool-acceptance-2026-11-27.jsonl`](windows11-cpu-pool-acceptance-2026-11-27.jsonl).
SHA-256: `E156FA208D6B210A83A70236BA1DB52FB8CE627B64DD098197FBAC9FF465A2EE`.
The sanitized Pool preflight record is retained locally outside the repository;
SHA-256: `AB333BCE7E4E51224A63CDE87C468A42A56FD0DA8DC798BF807D3482FF49094C`.

This is real Pool interoperability evidence for one Windows CPU/POOL
configuration. Offline tests separately cover reconnect/failover and explicit
error classifications. Additional required OS/mode configurations and the
remaining G1 acceptance criteria are still pending.
