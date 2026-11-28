# Native CUDA Stratum Loopback — 2026-11-28

## Scope and boundary

This run connected the native Windows CUDA search engine to the experimental
Stratum CLI through a local mock peer. It did not contact a public or production
pool, use a payout address, or submit a real share. The mock's `accepted`
reply is not pool acceptance evidence and does not pass G1 or G4.

## Source and environment

- PEPEPOW Miner source: `ed0566ff6d28c15bae19c3861cfacf57b38a2155` on the
  stacked PR #41 branch; pinned Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- Windows 11 x64; NVIDIA GeForce RTX 3060, compute capability 8.6, 12 GiB;
  driver 610.88.
- CUDA Toolkit overlay 13.3.1 / NVCC 13.3.73; MSVC 19.44.35230.0; CMake
  3.31.6; native Python 3.13.17. The system CUDA Toolkit directory lacks the
  `include/crt` headers referenced by its own runtime headers, so this build
  used the existing local `pepepow-cuda-toolkit-13.3.1-overlay` tree.
- The core CPU loopback executable was built from this source at `ed0566f`
  with the pinned Core/OpenLibm inputs. The CUDA oracle and engine were built
  from the same branch source and pinned Core revision.

## Build and tests

The complete root CPU project build completed (the existing native build
directory had no explicit CMake build type), then the root CTest suite passed
**11/11** on Windows. The standalone GPU project built its CUDA stages,
oracle, and bounded-search engine in Release. Its live-device bounded-search
contract passed **1/1** with `PEPEPOW_CUDA_REQUIRE_DEVICE=1`; the recorded JSON
identified the RTX 3060, toolkit 13.3.1, and compute capability 8.6. That
contract exercised target boundaries, pinned-Core candidate recheck, batch
reuse, cancellation/deadline handling, and the `UINT32_MAX` boundary.

The strict integration command was:

```powershell
python tests/test_native_cuda_stratum_loopback.py `
  --miner build/heartbeat-round11/pepepow-live-miner.exe `
  --engine build/round13-cuda-vscmake/Release/pepepow-cuda-hoohash-search.exe `
  --cpu-oracle build/round13-cuda-vscmake/pepepow-hoohash-digest-core-oracle.exe `
  --evidence-dir build/round13-cuda-vscmake/cuda-stratum-loopback-evidence
```

Result: **PASS**. The test's engine preflight processed one nonce and returned
the physical NVIDIA device identity. In the local Stratum session, the miner
processed one 128-nonce batch, submitted one candidate, received one mock
acceptance, and exited at the one-response goal. The submission fields were:

```text
login: mock-wallet.worker
job: gpu-loopback
extranonce2: 00000000
ntime: 65a0bc00
nonce: 00000000
```

The submitted 80-byte header and full digest were:

```text
header: 00000020000000000000000000000000000000000000000000000000000000000000000052c3bf0eb15561e4344cf77b09471031b59bd708ea18e8c9f65a79f316b1c8fe00bca065ffff001d00000000
digest: b40d446bfa316d0dfe903e3e793c203c8a5459db275ef07ff671499fd8941a9c
```

The miner emits this candidate only after its CUDA process result passes the
in-process pinned-Core full-digest and target recheck. A separate pinned-Core
oracle invocation then matched all 32 digest bytes before this report was
written. The local build's `cuda-stratum-loopback-evidence/summary.json` and
`events.jsonl` preserve the device preflight, candidate, submit, mock response,
and resource samples; those generated files remain in the ignored build tree.

## Limits and next acceptance

This run validates only the native Windows successful CUDA-to-Stratum path.
It does not establish broader GPU parity, a real-pool share, G1, or G4. Existing
job cancellation and reconnect product loopbacks use CPU child fixtures; the
physical-device bounded-search test covers GPU cancellation separately. Next,
retain separate WSL/native Linux/CI evidence, then defer GPU live-pool testing
until the G0 gate and current Owner authorization allow it.
