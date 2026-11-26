# CUDA Device Probe Windows GPU Evidence

Date: 2026-11-26 (Asia/Taipei)

## Host and toolchain

- Native Windows 11 Pro x64, build 26300.
- NVIDIA GeForce RTX 3060, compute capability 8.6, 12,288 MiB VRAM.
- NVIDIA display driver 610.88.
- Visual Studio 2022 Build Tools, MSVC v143 19.44.35230 (toolset 14.44.35207), Windows SDK 10.0.26100.0.
- CUDA Toolkit release 13.3.1; NVCC component build 13.3.73.
- CMake 4.5.1, Ninja 1.13.2, x64 CPython 3.13.17.

The NVIDIA Toolkit installer completed with its selected compiler, runtime,
and Visual Studio integration components. That selected installation did not
include all compiler support files required for native compilation. The probe
was therefore built against an isolated Toolkit root assembled from the
official NVIDIA 13.3.1 redistributable manifest; the Program Files installation
was not modified by this overlay. Component archive SHA-256 values:

- `cuda_crt`: `9227ec7c80db10b7cb0d4ee71ed62ec7ae36e67890216413ab6f9afa35d577f0`
- `libnvvm` (Windows x64): `ca8f11d5173ac16a166be8fafefbf9676542a097de1fce61b3f17696dffc1f27`
- `libnvptxcompiler` (Windows x64): `28123a511d9effd4e0b518baad740566c5bf495f4657efa5ec048ac79c2e5ccf`

NVIDIA's [CUDA 13.3.1 redistributable manifest](https://developer.download.nvidia.com/compute/cuda/redist/redistrib_13.3.1.json)
is the source of these Windows packages. The Toolkit `version.json` in the
build root reports release `13.3.1`; NVCC itself reports `13.3.73`.

## Build and test

The standalone Visual Studio 2022 x64 project was configured in separate
build directories for `Release` and `Debug`, then built with the installed MSVC
compiler and the Toolkit root above. The project propagates the
`CudaToolkitDir` global to CMake's CUDA compiler try-compile as well as the
generated probe targets.

```powershell
$cudaRoot = '<isolated CUDA 13.3.1 component root>'
$build = '<build directory>'
$env:CUDA_PATH = $cudaRoot
$env:Path = "$cudaRoot\bin;$env:Path"
cmake -S tools/gpu -B $build -G "Visual Studio 17 2022" -A x64 `
  "-DCUDAToolkit_ROOT=$cudaRoot" `
  "-DCMAKE_CUDA_COMPILER=$cudaRoot\bin\nvcc.exe"
cmake --build $build --config Release
ctest --test-dir $build -C Release --output-on-failure
cmake --build $build --config Debug
ctest --test-dir $build -C Debug --output-on-failure
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = '1'
& "$build\Release\pepepow-cuda-device-probe.exe"
& "$build\Debug\pepepow-cuda-device-probe.exe"
```

Results on this native Windows GPU host:

- Release build: passed.
- Release CTest: 1/1 passed; mock error cases and invalid-argument JSON passed,
  then the live Runtime API returned a device.
- Debug build: passed.
- Debug CTest: 1/1 passed with the same mock/live separation.
- Both direct probe invocations returned exit code 0 and schema version 1.
- `nvidia-smi` before and after the probes reported RTX 3060, driver 610.88,
  compute capability 8.6, 12,288 MiB. The probe launches no kernel and performs
  no mining or sustained GPU load.

The Debug and Release JSON outputs were byte-for-byte identical on this host:

```json
{
  "schema_version": 1,
  "status": "ok",
  "cuda_versions": {
    "driver_api_supported": {"encoded": 13030, "major": 13, "minor": 3},
    "runtime": {"encoded": 13030, "major": 13, "minor": 3},
    "toolkit_build": "13.3.1"
  },
  "devices": [{
    "index": 0,
    "name": "NVIDIA GeForce RTX 3060",
    "compute_capability": {"major": 8, "minor": 6, "architecture": "sm_86"},
    "global_memory_bytes": 12884377600,
    "shared_memory_per_block_bytes": 49152,
    "shared_memory_per_block_optin_bytes": 101376,
    "registers_per_block": 65536,
    "registers_per_multiprocessor": 65536,
    "multiprocessor_count": 28,
    "warp_size": 32,
    "max_threads_per_block": 1024,
    "fp64": {
      "attribute_available": true,
      "supported": true,
      "single_to_double_precision_perf_ratio": 64,
      "limitation": "This relative throughput attribute does not establish CPU, OpenLibm, or cross-architecture digest parity."
    }
  }],
  "error": null
}
```

This evidence is for native Windows only. No WSL or bare-metal Linux GPU run is
claimed. The probe establishes device visibility and reported properties; it
does not establish HooHash V110 CUDA correctness or mining readiness.
