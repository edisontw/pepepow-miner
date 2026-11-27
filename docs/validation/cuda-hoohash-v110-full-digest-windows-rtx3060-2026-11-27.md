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
  test; total 4.84 seconds.
- Debug build of all opt-in CUDA stages and CPU oracles: passed.
- Debug CTest suite: 8/8 passed, including the live 10,000-nonce parity
  test; physical-device total 4.98 seconds.
- No-GPU Debug CTest suite: 7/7 passed in 2.00 seconds with
  `CUDA_VISIBLE_DEVICES=-1` and the explicit no-device skip enabled.
- 10,000-nonce physical CUDA test: nonces 0–9,999, all full 32-byte digests
  matched pinned Core; CTest time 0.96 seconds in Release and 0.99 seconds in
  Debug.
- Five additional Release 10,000-nonce runs compared each complete digest to
  pinned Core, with matching CPU/GPU transcript hashes in every run.
- The success JSON schema was tested with a labeled mock fixture; zero/NaN,
  inconsistent-rate, and changed-scope fixtures were rejected. With
  `CUDA_VISIBLE_DEVICES=-1`, the no-GPU path passed the CPU fixtures, explicitly
  skipped only `no_device`, and emitted the expected skip report.
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

## CUDA event pipeline timing

The experimental stage emits `gpu_pipeline_ms` and
`gpu_pipeline_digests_per_second` using CUDA events around
`prepareHashInputs`, `generateMatrix`, and `calculateDigests`. The timing
excludes host setup, allocations, host/device transfers, CPU oracle work, and
output writing. It measures this diagnostic pipeline with one matrix reused
across the batch; it is not miner hashrate and does not imply pool performance.
The Python contract also runs a clearly labeled mock success JSON fixture
before device access, so no-GPU CI validates the success schema and rejects
invalid timing values without presenting mock data as physical-device output.

Five Release runs each covered nonces 0–9,999 and passed full 32-byte CPU/GPU
digest comparison. Median event time was 77.307907 ms (range 76.950531–
83.954689 ms), equivalent to a median 129,352.87 pipeline digests/s. The
10,000-record CPU and GPU transcript SHA-256 matched in every run:
`dc12ac687609af3c33d5099b5d5b344bd8c6a2a69d8f13932484ae6966cd3c47`.
Per-run timing and transcript details are preserved locally at
`D:\Dev\pepepow-cuda-digest-evidence-20261127\build-vs331\event-timing-10k-resource-repeats.json`
(SHA-256 `6756ff548331ac28605346f5a038c0e731568b53997da72ed1460ab478bd8cf6`).

## Runtime kernel resources and occupancy

The success JSON reports `cudaFuncGetAttributes` register, local-memory, and
static shared-memory values plus `cudaOccupancyMaxActiveBlocksPerMultiprocessor`
for the configured block size. CUDA defines `localSizeBytes` as local memory
per thread and the occupancy API as a per-SM active-block capacity
calculation. These values are theoretical launch-resource estimates, not
measured achieved occupancy. The report also includes grid size so the one-
block matrix generator is not confused with a fully occupied GPU.

| Kernel | Grid | Registers/thread | Local bytes/thread | Max active blocks/SM | Theoretical active warps/SM | Per-SM theoretical occupancy |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `prepareHashInputs` | 79 × 128 | 72 | 64 | 7 | 28 / 48 | 58.33% |
| `generateMatrix` | 1 × 1 | 38 | 0 | 16 | 16 / 48 | 33.33%* |
| `calculateDigests` | 79 × 128 | 80 | 688 | 6 | 24 / 48 | 50.00% |

* `generateMatrix` launches one block total, so only one SM receives work for
  that kernel. The 33.33% value is the occupancy calculator's theoretical
  per-SM capacity for a one-thread block if enough blocks existed; it is not
  the achieved device-wide occupancy of this one-block launch.

The runtime `calculateDigests` local-memory attribute (688 bytes per thread)
matches PTXAS's 688-byte stack frame. A separately generated `sm_86` PTX file
contains a 688-byte local-frame declaration and 416 `ld.local` plus 146
`st.local` PTX instructions in `calculateDigests`. These are static PTX
instructions, not dynamic memory-transaction counts. Nsight Compute,
`cuobjdump`, and `nvdisasm` were not installed, so achieved occupancy and SASS
local-memory traffic remain unmeasured. PTX SHA-256:
`b03bb96a4ee8391c86b90b3ec1595837a676ffd1fdfcc0f76b162452405370b0`.

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
mining hashrate is inferred from the CTest wall time; the separately reported
CUDA-event measurement is limited to this experimental kernel pipeline.

## Evidence hashes

Evidence is under the local directory
`D:\Dev\pepepow-cuda-digest-evidence-20261127\build-vs331`.

| Artifact | SHA-256 |
| --- | --- |
| 10,000-case CPU transcript | `dc12ac687609af3c33d5099b5d5b344bd8c6a2a69d8f13932484ae6966cd3c47` |
| 10,000-case CUDA transcript | `dc12ac687609af3c33d5099b5d5b344bd8c6a2a69d8f13932484ae6966cd3c47` |
| 10,000-case JSON report | `1b1cc39ca02b93cb578401f3d81ca41a619d492f3440e290aed4268532cf29e1` |
| Five-run timing/resource JSON | `6756ff548331ac28605346f5a038c0e731568b53997da72ed1460ab478bd8cf6` |
| Release CUDA digest executable | `372bd2c490f33189d1b4eda69d331306f009b233fcfbfbf746cfd66af6cfce32` |
| `sm_86` CUDA PTX | `b03bb96a4ee8391c86b90b3ec1595837a676ffd1fdfcc0f76b162452405370b0` |
| Pinned OpenLibm static archive | `9fb7b4caf1deb2136314508c3116adafd8a192a67d0ca4a95e22744fa0ca5f96` |
| NVCC/PTXAS resource-usage log | `e504cc264822fe06b8a7a1bfa3fca22d8f1413c2dc85f1976856eaa4777801ff` |

The matching transcript hashes include the same `PPWDIG1\0` header and 10,000
complete output digests. The report records the exact nonce range, Core and
OpenLibm SHAs, digest count, per-transcript hashes, device status, and strict
floating-point build flags. The CTest console logs and both Debug/Release
build logs, plus `resource-usage/digest-release-ptxas.log`, remain alongside
the artifacts in the local evidence directory.
