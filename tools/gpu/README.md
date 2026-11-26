# Offline CUDA device probe

`pepepow-cuda-device-probe` is an opt-in, read-only CUDA inventory tool. It
does not connect to a pool, launch a kernel, search nonces, mine, change device
settings, or enable background work. The root CPU build does not include this
project and does not require a CUDA Toolkit.

## Output contract

The executable writes one JSON object to stdout. Keys are emitted in a stable
order. `schema_version` is `1`; `status` is `ok` or `error`; `devices` is an
array ordered by CUDA device index; and `error` is either `null` or a stable
error object. A device record includes its index and name, compute capability,
global memory, shared memory limits, register limits, multiprocessor count,
warp size, maximum threads per block, and the CUDA-reported FP64 performance
ratio when available.

`cuda_versions.driver_api_supported` is the latest CUDA API version supported
by the installed driver, as reported by `cudaDriverGetVersion`. It is not the
NVIDIA display-driver package version. `runtime` is reported by
`cudaRuntimeGetVersion`; `toolkit_build` comes from the Toolkit `version.json`
release field when available (for example, `13.3.1`). CMake may separately
report an NVCC component version such as `13.3.73`. The display-driver package
version can be recorded separately with
`nvidia-smi --query-gpu=driver_version --format=csv,noheader`. On Windows with
CUDA 13, the runtime reported by the driver may differ from the Toolkit used to
build the probe.

Error exit codes are `2` for no CUDA device, `3` for no compatible driver, `4`
for runtime initialization failure, `5` for another Runtime API query error,
and `64` for invalid command-line arguments. Errors still produce one JSON
object on stdout; diagnostic text is not mixed into the output.

The FP64 ratio is a device throughput attribute, not a correctness guarantee.
It says nothing about agreement with PEPEPOW Core, OpenLibm, a CPU implementation,
or another GPU architecture. This probe does not establish mining readiness.

## Build and run on Windows

Install a CUDA Toolkit version supported by the installed NVIDIA driver and a
host compiler listed in NVIDIA's Windows installation guide. CUDA 13.3.1's
Windows support table lists MSVC 193x for Visual Studio 2022. This RTX 3060
host has MSVC 19.44: the probe built and ran here, but 19.44 is outside that
published compiler range, so this is local build evidence rather than an
officially listed toolchain qualification. Keep the display driver separate.

From an x64 Visual Studio 2022 Developer PowerShell:

```powershell
$cudaRoot = $env:CUDA_PATH
cmake -S tools/gpu -B build/cuda-device-probe `
  -G "Visual Studio 17 2022" -A x64 `
  "-DCUDAToolkit_ROOT=$cudaRoot" `
  "-DCMAKE_CUDA_COMPILER=$cudaRoot\bin\nvcc.exe"
cmake --build build/cuda-device-probe --config Release
$env:PATH = "$env:CUDA_PATH\bin;$env:PATH"
ctest --test-dir build/cuda-device-probe -C Release --output-on-failure
& .\build\cuda-device-probe\Release\pepepow-cuda-device-probe.exe
```

To make the contract test require a real CUDA device, set
`PEPEPOW_CUDA_REQUIRE_DEVICE=1` before running CTest. The regular contract test
also runs on hosts without a CUDA device and checks both deterministic mock
errors and the actual Runtime API response.

## Build and run on Linux or WSL

Use a CUDA Toolkit supported by the host driver's CUDA compatibility range. In
WSL, use the Windows-provided GPU driver; do not install a Linux NVIDIA display
driver inside the distribution. Record WSL evidence separately from Windows
native and bare-metal Linux evidence.

```sh
cmake -S tools/gpu -B build/cuda-device-probe \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc \
  -DCUDAToolkit_ROOT=/usr/local/cuda
cmake --build build/cuda-device-probe --parallel
ctest --test-dir build/cuda-device-probe --output-on-failure
build/cuda-device-probe/pepepow-cuda-device-probe
```

For a real-device contract run, set `PEPEPOW_CUDA_REQUIRE_DEVICE=1` in the
environment before CTest. The deterministic error-fixture executable is
test-only and is never named or reported as a live GPU result.

## CPU-oracle correctness harness (contract stage)

The opt-in correctness harness is a framework stage, not a HooHash CUDA
implementation. Its small CUDA kernel writes one nonce per work item so the
test can verify batch coverage on a real device. The Python contract tests use
the pinned Core `pepepow-kat` executable as the CPU oracle, check a fixed
80-byte/full-32-byte vector and a `UINT32_MAX` vector, validate nonce byte
order, reject duplicate or missing coverage, check target equality boundaries,
and CPU-recheck every fixture candidate using all 32 digest bytes. The
last-byte mutation is required to fail.

The JSON response contract uses an 80-byte `header_hex`, little-endian nonce
bytes at offsets 76–79, a uint32 `nonce_start`, and a bounded `nonce_count`.
The coverage-only GPU executable reports each nonce it executed and an empty
candidate list; it does not calculate HooHash. Candidate fixtures are marked
`MOCK` in test output. Only the `LIVE CUDA DEVICE` records describe real GPU
coverage. CI runs without a GPU, still builds the CUDA code, executes the
pinned CPU oracle and mocked negative cases, then accepts a recognized
no-device Runtime API result. Set `PEPEPOW_CUDA_REQUIRE_DEVICE=1` on a GPU host
to require live batch coverage.

First build the same pinned Core/OpenLibm oracle used by this repository's
G0 validation instructions. Then enable this extra test target explicitly:

```sh
cmake -S tools/gpu -B build/cuda-correctness \
  -DPEPEPOW_CUDA_CORRECTNESS_TESTS=ON \
  -DPEPEPOW_CPU_ORACLE=/absolute/path/to/pepepow-kat
cmake --build build/cuda-correctness --parallel
ctest --test-dir build/cuda-correctness --output-on-failure
```

`PEPEPOW_CPU_ORACLE` must be built from the pinned PEPEPOW Core reference and
OpenLibm; the CI workflow checks both source SHAs and builds that executable
before configuring the CUDA harness. `PEPEPOW_CUDA_TEST_ARCHITECTURE` defaults
to 75 for the short coverage kernel and can be set to another architecture
supported by the installed Toolkit.

This stage does not compare a CUDA HooHash digest with Core and does not claim
GPU mining correctness. The candidate schema and CPU recheck are ready for a
later hash backend to supply full digests.

Native RTX 3060 results are recorded in [CUDA correctness harness evidence](../../docs/validation/cuda-correctness-harness-windows-rtx3060-2026-11-26.md)
and [device-probe evidence](../../docs/validation/cuda-device-probe-windows-rtx3060-2026-11-26.md).

## NVIDIA references

- [CUDA Runtime API: device properties](https://docs.nvidia.com/cuda/cuda-runtime-api/structcudaDeviceProp.html)
- [CUDA Runtime API: device management](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__DEVICE.html)
- [CUDA Runtime API: versions](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART____VERSION.html)
- [CUDA C++ Programming Guide: compute capabilities](https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/compute-capabilities.html)
- [CUDA 13.3.1 Windows Installation Guide](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-installation-guide-microsoft-windows/index.html)
- [CUDA 13.3.1 Linux Installation Guide](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-installation-guide-linux/index.html)
- [CUDA 13.3.1 Release Notes](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-toolkit-release-notes/)
