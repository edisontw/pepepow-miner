# CUDA HooHash V110 Full-Digest Experiment — Windows RTX 3060 — 2026-11-27

## Result

The opt-in CUDA digest experiment matched the pinned Core HooHash V110 output
for every byte in 10,000 consecutive 32-byte digests (nonces 0–9,999). The
same physical run also checked nonce 0, the Explorer header at nonces 6–8,
and `UINT32_MAX`. The executable writes a bounded offline transcript; it does
not connect to a pool, search for shares, or benchmark mining throughput.

This is correctness evidence for this source revision and test corpus. It does
not qualify a production GPU miner, G4 acceptance, a supported Windows CUDA
compiler combination, or binary redistribution. G0 remains blocked by the
separate upstream source-rights review.

## Source and host

- Repository: `edisontw/pepepow-miner`, branch
  `codex/cuda-fp64-transform-2026-11-26`.
- Pinned Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- Pinned OpenLibm: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94`.
- Host: Windows 11 x64, build 26300.
- GPU: NVIDIA GeForce RTX 3060, compute capability 8.6, 12,288 MiB VRAM.
- Driver: 610.88; CUDA driver/runtime API version reported by the stage:
  13030 / 13030.
- Toolkit: CUDA 13.3.1, NVCC 13.3.73.
- CMake: 3.31.6; Windows SDK 10.0.26100.0.
- Host compiler: Visual Studio 2022 Build Tools, MSVC 19.44.35230.0.
- CPU oracle compiler: MSYS2 UCRT64 GCC 16.2.0.
- Python: native x64 CPython 3.13.17.

The physical CUDA compilation and run used MSVC 19.44. NVIDIA's CUDA 13.3.1
Windows guide lists the Visual Studio 2022 MSVC 193x range; this host compiler
is outside that published range. Record this as local experimental evidence,
not vendor-qualified toolchain support.

## Commands

The existing MSYS2 UCRT64 compiler, pinned Core checkout, pinned OpenLibm
checkout, and previously built OpenLibm archive were reused. CUDA headers and
compiler support files were selected through the local
`pepepow-cuda-toolkit-13.3.1-overlay` because the installed Toolkit directory
did not contain the required `crt/host_config.h` include path.

```powershell
cmake -S tools/gpu -B build/cuda-digest -G "Visual Studio 17 2022" -A x64 `
  -DCUDAToolkit_ROOT=D:/Dev/pepepow-cuda-toolkit-13.3.1-overlay `
  -DPEPEPOW_CUDA_HOOHASH_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_CORE_MATRIX_ORACLE_TESTS=ON `
  -DPEPEPOW_CUDA_MATRIX_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_TRANSFORM_DIAGNOSTICS=ON `
  -DPEPEPOW_CUDA_FULL_DIGEST_TESTS=ON `
  -DPEPEPOW_CUDA_FULL_DIGEST_10000_TESTS=ON `
  -DPEPEPOW_CORE_SOURCE_DIR=D:/Dev/pepepow-deps/PePe-core `
  -DPEPEPOW_CORE_EXPECTED_SHA=bc08108ceccd957c79040dc7fedc1d56d8d47f97 `
  -DPEPEPOW_OPENLIBM_SOURCE_DIR=D:/Dev/pepepow-deps/openlibm `
  -DPEPEPOW_OPENLIBM_LIBRARY=D:/path/to/libopenlibm.a `
  -DPEPEPOW_TRANSFORM_ORACLE_COMPILER=D:/Dev/msys64/ucrt64/bin/gcc.exe `
  -DPEPEPOW_CUDA_TEST_ARCHITECTURE=86
cmake --build build/cuda-digest --config Release --parallel 2
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = "1"
ctest --test-dir build/cuda-digest -C Release --output-on-failure
```

The physical tests ran with `PEPEPOW_CUDA_REQUIRE_DEVICE=1`, so no missing-
device skip was allowed. The 10,000-nonce CTest is opt-in and bounded; its
timeout is 1,800 seconds. No CUDA fast-math option was enabled. The digest
kernel compiled with `--fmad=false`, `--ftz=false`, `--prec-div=true`, and
`--prec-sqrt=true`.

## Results

- Release build of all opt-in CUDA stages and CPU oracles: passed.
- Release CTest suite: 8/8 passed, including the live 10,000-nonce parity
  test; total 4.78 seconds.
- Debug build of all opt-in CUDA stages and CPU oracles: passed.
- Debug CTest suite excluding the 10,000-nonce stress batch: 7/7 passed;
  total 4.16 seconds.
- 10,000-nonce physical CUDA test: nonces 0–9,999, all full 32-byte digests
  matched pinned Core; CTest time 0.96 seconds.
- Boundary/fixed vectors: nonce 0, nonces 6–8, and `UINT32_MAX` all matched.
- The five fixed digest values were also accepted by the existing native
  UCRT64 `pepepow-kat` executable built against the pinned CPU implementation.
- Nonce wraparound (`UINT32_MAX` with count 2) was rejected before CUDA
  initialization with exit code 64.
- Physical CUDA metadata: NVIDIA GeForce RTX 3060, capability 8.6, driver API
  13030, runtime API 13030, toolkit build 13.3.1.

The test computes the nonce-independent matrix seed once, generates the
4,096-element / 32 KiB matrix once on the GPU, and reuses it for each nonce in
the batch. It computes both BLAKE3 header passes, the Core-order FP64 matrix
reduction through the OpenLibm diagnostic translations, the final BLAKE3,
and Core's final digest-byte reversal on the GPU. The Python comparator
compares complete digest records; it does not compare prefixes or target
results.

## Static kernel resource snapshot

A separate NVCC/PTXAS resource-usage compile used the same `sm_86` target and
strict floating-point flags. It reported:

| Kernel | Registers | Stack frame | Register spill loads/stores |
| --- | ---: | ---: | ---: |
| `calculateDigests` | 80 | 688 bytes | 0 / 0 |
| `generateMatrix` | 38 | 0 bytes | 0 / 0 |
| `prepareHashInputs` | 72 | 64 bytes | 0 / 0 |

The 688-byte `calculateDigests` stack frame remains material even though
PTXAS reports zero register spill loads and stores. This is a follow-up
optimization target: inspect local stack traffic and measure active-block
occupancy on the physical GPU while preserving full-digest parity. No GPU
throughput result is inferred from the CTest wall time.

## Evidence hashes

Evidence is under the local directory
`D:\Dev\pepepow-cuda-digest-evidence-20261127\build-vs331`.

| Artifact | SHA-256 |
| --- | --- |
| 10,000-case CPU transcript | `dc12ac687609af3c33d5099b5d5b344bd8c6a2a69d8f13932484ae6966cd3c47` |
| 10,000-case CUDA transcript | `dc12ac687609af3c33d5099b5d5b344bd8c6a2a69d8f13932484ae6966cd3c47` |
| 10,000-case JSON report | `b27417178be292b83c0baf5ae8f3b95e8098d92680b70dc1aa5330792a98c27a` |
| Release CUDA digest executable | `479accf2038eea70fd6d83ef29d7ecba6785c15fc7e60f89fa3c64527762e429` |
| Pinned OpenLibm static archive | `9fb7b4caf1deb2136314508c3116adafd8a192a67d0ca4a95e22744fa0ca5f96` |
| NVCC/PTXAS resource-usage log | `e504cc264822fe06b8a7a1bfa3fca22d8f1413c2dc85f1976856eaa4777801ff` |

The matching transcript hashes include the same `PPWDIG1\0` header and 10,000
complete output digests. The report records the exact nonce range, Core and
OpenLibm SHAs, digest count, per-transcript hashes, device status, and strict
floating-point build flags. The CTest console logs and both Debug/Release
build logs, plus `resource-usage/digest-release-ptxas.log`, remain alongside
the artifacts in the local evidence directory.
