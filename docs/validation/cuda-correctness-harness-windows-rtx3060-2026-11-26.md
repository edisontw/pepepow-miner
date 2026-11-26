# CUDA Correctness Harness Windows GPU Evidence

Date: 2026-11-26 (Asia/Taipei)

## Host and pinned CPU oracle

- Native Windows 11 Pro x64, build 26300.
- NVIDIA GeForce RTX 3060, compute capability 8.6, 12,288 MiB VRAM; driver 610.88.
- Visual Studio 2022 Build Tools / MSVC 19.44.35230, Windows SDK 10.0.26100.
- CUDA Toolkit 13.3.1, NVCC component 13.3.73.
- CMake 4.5.1, Ninja 1.13.2, x64 CPython 3.13.17.
- CPU oracle: `pepepow-kat` linked against PEPEPOW Core `bc08108ceccd957c79040dc7fedc1d56d8d47f97` and OpenLibm v0.8.7 `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`; native UCRT64 GCC 16.2.0 build.

The Core executable is invoked as a separate process, avoiding cross-linking a
MinGW CPU library into the MSVC/NVCC test executable. Core source and OpenLibm
source remain external pinned dependencies; this change does not vendor them
or make a license/redistribution decision.

## Fixed full-digest vectors

Both expected digests are 32-byte `hoohashv110` function-byte outputs from the
pinned Core reference. Nonce bytes occupy offsets 76–79 in little-endian wire
order.

| Case | Header | Full digest | Expected source |
| --- | --- | --- | --- |
| Existing independent nonce-6 vector | `0040002003020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c5b5d507845ead90ea4c4caeef4baef7817c0fb2169d20739883e22bb6d8cf8ef0be1697171e3021d06000000` | `a0cba391e2eda64e067cf2c4f2209370bae8b31ac7ff4cc6cf141ebb25073a15` | Existing independently checked test vector |
| Same 76-byte prefix, nonce `UINT32_MAX` | `0040002003020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c5b5d507845ead90ea4c4caeef4baef7817c0fb2169d20739883e22bb6d8cf8ef0be1697171e3021dffffffff` | `4195b28c3892661c3bd563d368b4cd141d880921fb7652ae3bcd34f64ea39b61` | Generated once by the pinned Core binary as a nonce-boundary fixture; not independent evidence |

The test calls the pinned Core executable for each vector and compares every
digest byte. A fixture candidate is also passed through the same CPU oracle;
mutating its final digest byte is rejected. Candidate recheck is currently a
`MOCK GPU CANDIDATE` fixture: no HooHash CUDA kernel exists yet.

Target boundaries use the repository helper's 32-byte little-endian integer
contract and inclusive `hash <= target` rule: equality and `target - 1` pass;
`target + 1` fails.

## Build and run

The existing pinned-Core UCRT64 `pepepow-kat.exe` was used as the oracle. The
standalone CUDA project was configured in a separate Visual Studio x64 build
directory with `PEPEPOW_CUDA_CORRECTNESS_TESTS=ON` and that oracle path, then
built in Debug and Release. The test-only CUDA kernel writes one nonce value
per GPU work item and performs no hashing.

```powershell
$cudaRoot = '<isolated CUDA 13.3.1 component root>'
$oracle = '<pinned-Core pepepow-kat.exe>'
$build = '<build directory>'
$env:CUDA_PATH = $cudaRoot
$env:Path = "$cudaRoot\bin;$env:Path"
cmake -S tools/gpu -B $build -G "Visual Studio 17 2022" -A x64 `
  "-DCUDAToolkit_ROOT=$cudaRoot" `
  "-DCMAKE_CUDA_COMPILER=$cudaRoot\bin\nvcc.exe" `
  -DPEPEPOW_CUDA_CORRECTNESS_TESTS=ON `
  "-DPEPEPOW_CPU_ORACLE=$oracle"
cmake --build $build --config Release
cmake --build $build --config Debug
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = '1'
ctest --test-dir $build -C Release --output-on-failure
ctest --test-dir $build -C Debug --output-on-failure
```

Results on the native Windows GPU host:

- Release build passed; Release CTest passed 3/3.
- Debug build passed; Debug CTest passed 3/3.
- Both direct harness runs passed the pinned Core nonce-6 vector and
  `UINT32_MAX` vector, full-digest candidate recheck, last-byte negative case,
  duplicate/missing nonce rejection, little-endian header validation, and
  target boundary test.
- The live CUDA coverage kernel covered 32 consecutive nonces ending at
  `UINT32_MAX`, then covered `UINT32_MAX` as a one-item batch.
- A range starting at `UINT32_MAX` with count 2 was rejected with exit 64
  before any device work.
- `nvidia-smi` after the run reported the same RTX 3060, driver 610.88, CC 8.6,
  and 12,288 MiB.

In test output, `MOCK` identifies the candidate/negative fixtures and
`LIVE CUDA DEVICE` identifies actual device coverage. The GitHub Actions
workflow builds the same pinned Core CPU oracle and CUDA harness on a runner
without requiring a GPU; it tests contract fixtures and accepts a recognized
no-device Runtime API result. The workflow run for the posted feature branch
is recorded separately after CI completes.

This harness does not implement HooHash V110 on CUDA. It establishes fixed
I/O, CPU-oracle, batch-coverage, candidate-recheck, and target-boundary tests;
GPU full-digest parity and hashrate remain untested.
