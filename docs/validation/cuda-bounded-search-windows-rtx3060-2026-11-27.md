# CUDA Bounded Search Windows RTX 3060 Evidence — 2026-11-27

## Scope

This records local Windows-native evidence for the opt-in, offline bounded CUDA
nonce search experiment. It does not establish G4, full-corpus GPU parity,
pool integration, share submission, production mining, or release readiness.
No pool endpoint was contacted and no share was submitted.

## Source and toolchain

- Source base: PR #37 head `38873a5606f9cf722c28e5871ab1659521f48cba`.
- Pinned PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- Pinned OpenLibm: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- OS: Windows 11 Pro, build `26300`, x64.
- GPU: NVIDIA GeForce RTX 3060, compute capability `8.6`, 12,288 MiB VRAM.
- NVIDIA driver: `610.88`.
- CUDA Toolkit: `13.3.1`; NVCC reports `13.3.73`.
- Host compiler selected by NVCC: Visual Studio 2022 MSVC `19.44.35230`.
- CMake: `4.5.1`, Visual Studio 17 2022 generator.
- CUDA code was compiled for `sm_86`; no fast-math option was enabled, and
  `--fmad=false`, `--ftz=false`, `--prec-div=true`, and `--prec-sqrt=true` were
  used.
- Python: `3.13.17`; MSYS2 UCRT64 GCC available as `16.2.0` for the existing
  transform-oracle build path.

The NVCC/host-compiler build succeeded on this machine. MSVC `19.44` is outside
the CUDA 13.3 installation guide's published VS 2022 MSVC `193x` range, so this
local result is recorded as experimental compatibility evidence, not NVIDIA
vendor qualification. NVCC emitted CUDA-header codepage warnings and a
deprecated `--cudart shared` warning; neither prevented the builds.

## Build and test commands

The existing configured build directory was
`D:/Dev/pepepow-cuda-search-evidence-20261127/build-vs331`; configuration had
the bounded search and pinned-Core full-digest tests enabled, Core SHA checked,
and test architecture set to `86`.

```powershell
cmake --build D:/Dev/pepepow-cuda-search-evidence-20261127/build-vs331 `
  --config Release --target pepepow-cuda-hoohash-search `
  pepepow-cuda-hoohash-digest-stage
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = "1"
ctest --test-dir D:/Dev/pepepow-cuda-search-evidence-20261127/build-vs331 `
  -C Release -R "cuda_hoohash_bounded_search_core_recheck|hoohash_cuda_full_digest_pinned_core_parity" `
  --output-on-failure

cmake --build D:/Dev/pepepow-cuda-search-evidence-20261127/build-vs331 `
  --config Debug --target pepepow-cuda-hoohash-search `
  pepepow-cuda-hoohash-digest-stage
ctest --test-dir D:/Dev/pepepow-cuda-search-evidence-20261127/build-vs331 `
  -C Debug -R "cuda_hoohash_bounded_search_core_recheck|hoohash_cuda_full_digest_pinned_core_parity" `
  --output-on-failure
```

Both configurations built the bounded search executable and digest-stage
executable successfully. Release CTest passed `2/2` in `1.56 s`; Debug CTest
passed `2/2` in `1.75 s`. `PEPEPOW_CUDA_REQUIRE_DEVICE=1` made a missing or
unusable GPU fail the device test rather than silently skip it.

## Physical-device cases

The bounded-search test exercised the RTX 3060 and pinned Core oracle in both
Release and Debug:

- Nonce 6 exact-target equality returned the full digest
  `a0cba391e2eda64e067cf2c4f2209370bae8b31ac7ff4cc6cf141ebb25073a15`; the
  pinned Core CPU oracle rechecked the full digest and target.
- `target - 1`, equality, and `target + 1` followed the inclusive uint256
  comparison contract.
- A three-nonce all-hit batch returned each nonce once and CPU-rechecked every
  complete digest.
- A two-batch no-hit job processed both nonces while reporting one matrix build
  and one matrix reuse.
- A `UINT32_MAX` singleton exhausted at next nonce `2^32`, without wrapping.
- A full `2^32`-nonce request cancelled at the one-millisecond deadline and
  returned status `cancelled`, reason `timeout`, and exit code `130`.
- A full `2^32`-nonce no-hit request cancelled through the cancel file during
  execution. The Release evidence reports 114 completed one-nonce batches,
  then status `cancelled`, reason `cancel_file`, and exit code `130`.
- Pre-device argument validation rejected a nonce range that would wrap past
  `UINT32_MAX`.
- The pinned Core guard checked full digest bytes, a last-byte mutation, and
  target boundary arithmetic.

Machine-readable local JSON outputs are retained outside the checkout at
`D:/Dev/pepepow-cuda-search-evidence-20261127/build-vs331/cuda-bounded-search-evidence/`.
They include target equality/adjacent targets, two-batch matrix reuse,
`UINT32_MAX`, deadline cancellation, cancel-file cancellation, and nonce
overflow. The GitHub CI artifact path is configured by the workflow for the
same evidence directory name.

## Limits

This validates the bounded search plumbing against fixed vectors and the
existing pinned-Core digest path. It is not the 10,000-case GPU parity corpus
or an exhaustive all-input correctness proof. GPU candidates are always
rechecked by the pinned CPU Core executable before being reported. No
Stratum/session lifecycle, reconnect/failover, pool job replacement, accepted
share, CLI product flow, or GUI is implemented by this experiment. G0, G1, G4,
and distribution compliance remain separate gates.
