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

Set `PEPEPOW_CUDA_REQUIRE_DEVICE=1` before CTest on physical-GPU acceptance
hosts. A no-GPU CI job must explicitly set
`PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP=1`; only `no_device`, `driver_unavailable`,
and `initialization_failed` may then skip live enumeration. Generic CUDA/runtime
errors always fail the contract. The independent skip-policy test covers both
the accepted and rejected cases.

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
environment before CTest. Set `PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP=1` only when
the test job is intentionally running without a GPU. The deterministic
error-fixture executable is test-only and is never named or reported as a live
GPU result.

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

A no-GPU CI job must explicitly set `PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP=1`; only
recognized device-unavailable errors may then skip live coverage. Generic
CUDA/runtime errors fail the test. Set `PEPEPOW_CUDA_REQUIRE_DEVICE=1` on
physical-device hosts to reject every skip and require live batch coverage.

Native RTX 3060 results are recorded in [CUDA correctness harness evidence](../../docs/validation/cuda-correctness-harness-windows-rtx3060-2026-11-26.md)
and [device-probe evidence](../../docs/validation/cuda-device-probe-windows-rtx3060-2026-11-26.md).

## First HooHash V110 CUDA stage

The first hash-stage executable computes only BLAKE3 over an 80-byte header.
`--mode header` returns the nonce-dependent first pass. `--mode matrix-seed`
zeros bytes 76–79 before hashing, matching the nonce-independent seed input in
the pinned Core implementation. Both modes return the full 32-byte BLAKE3
digest. The CUDA result is compared byte-for-byte with BLAKE3 compiled from the
pinned PEPEPOW Core source; nonce 6 and `UINT32_MAX` vectors cover the boundary.

This stage does not generate the HooHash matrix, perform its FP64 transforms,
or produce a HooHash V110 digest. It has no mining, pool, or benchmarking path.

## Pinned-Core PRNG and matrix oracle

The matrix-oracle option includes the unmodified pinned Core
`src/crypto/hoohash/hoohash.c` in a test-only wrapper. It exposes the file-local
PRNG and matrix generator without patching the Core checkout or changing the
production CPU hash path. The wrapper records the 32-byte seed, four initial
little-endian `uint64` state words, each of the 4096 PRNG outputs and post-step
states, each lower 32-bit value, and every row-major binary64 matrix bit
pattern. The binary transcript uses explicit little-endian fields and is
compared against an independent Python transcription of the pinned operations.

The pinned Core source labels its generator `xoshiro256**`, while the actual
output expression is `rotl64(s0 + s3, 23) + s0` followed by the xoshiro state
transition. The wrapper preserves the implementation as written. Matrix
values use the Core expression `(double)lower_4_bytes / (double)UINT32_MAX *
1000000.0` in row-major order; the oracle does not substitute a reciprocal or
change the operation order.

The CPU contract checks 32 distinct seeds, including the matrix seed for the
nonce-6 Explorer header and 31 deterministic header templates. It confirms
that nonce 6 and `UINT32_MAX` for the same template produce the same Core seed,
and checks all 131,072 PRNG values, post-step states, and matrix binary64 bit
patterns. This stage is a CPU oracle only; the following CUDA matrix stage
must compare the full transcript on a physical GPU before any FP64 transform
work begins.

Enable the standalone pinned-Core oracle together with the existing Core
BLAKE3 test stage:

```powershell
cmake -S tools/gpu -B build/cuda-hoohash-matrix `
  -DPEPEPOW_CUDA_HOOHASH_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_CORE_MATRIX_ORACLE_TESTS=ON `
  -DPEPEPOW_CORE_SOURCE_DIR=D:/Dev/pepepow-deps/PePe-core `
  -DCUDAToolkit_ROOT="$env:CUDA_PATH"
cmake --build build/cuda-hoohash-matrix --config Release
ctest --test-dir build/cuda-hoohash-matrix -C Release --output-on-failure
```

## CUDA matrix-generation stage

The first CUDA matrix stage uses one bounded CUDA block per seed and advances
the pinned Core PRNG sequentially for each row-major matrix. It emits the same
little-endian transcript as the pinned-Core wrapper: seed, initial state,
every PRNG output and next state, lower 32-bit value, and all 4096 binary64
matrix bit patterns. The Python test compares the complete files byte-for-byte
and reports the first differing seed, element, and field.

The stage uses the Core operation order for
`(double)lower / (double)UINT32_MAX * 1000000.0`, forces the division through
CUDA's round-to-nearest-even `__ddiv_rn`, and compiles with `--fmad=false`. It
does not enable `--use_fast_math`. The flags are present in the generated
Visual Studio CUDA project, not inferred from a documentation comment. The
[CUDA Math API](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-math-api/index.html)
documents `__ddiv_rn`; the [CUDA 13.3.1 NVCC guide](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-compiler-driver-nvcc/index.html)
documents `--fmad` and its interaction with fast math.

The physical-device corpus includes the nonce-6 header matrix seed, 31 other
deterministic header templates, and a synthetic all-`0xff` seed that exercises
uint64 wraparound. It compares 33 complete transcripts (135,168 PRNG values,
state transitions, and matrix elements). A negative control flips the last
matrix element's final bit and confirms that the comparator reports element
4095. Device memory is bounded by the 64-seed input cap (under 15 MiB for the
observation buffer); this is a short correctness run, not a load test.

Enable the stage with the pinned-Core and CPU matrix oracles:

```powershell
cmake -S tools/gpu -B build/cuda-hoohash-matrix `
  -DPEPEPOW_CUDA_HOOHASH_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_CORE_MATRIX_ORACLE_TESTS=ON `
  -DPEPEPOW_CUDA_MATRIX_STAGE_TESTS=ON `
  -DPEPEPOW_CORE_SOURCE_DIR=D:/Dev/pepepow-deps/PePe-core `
  -DCUDAToolkit_ROOT="$env:CUDA_PATH"
cmake --build build/cuda-hoohash-matrix --config Release
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = "1"
ctest --test-dir build/cuda-hoohash-matrix -C Release --output-on-failure
```

The stage has stable error JSON and nonzero exit codes. A no-GPU CI job must
explicitly set `PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP=1`; only the recognized
`no_device`, `driver_unavailable`, and `initialization_failed` errors can
skip. Generic CUDA execution failures always fail the test. Physical-device
acceptance sets `PEPEPOW_CUDA_REQUIRE_DEVICE=1` and does not allow a skip.

Passing this stage establishes exact matrix-generation parity for the tested
seeds only. FP64 nonlinear transforms, full HooHash V110 digest parity, GPU
candidate CPU recheck, and G4 acceptance remain incomplete.

## FP64 transform diagnostic stage

The opt-in transform diagnostic invokes the unmodified pinned Core functions
through a test-only C wrapper and links that CPU oracle to the pinned OpenLibm
static library. A bounded CUDA kernel evaluates the same inputs with CUDA
device math. It records binary64 inputs and outputs for
MediumComplexNonLinear, IntermediateComplexNonLinear,
HighComplexNonLinear, ComplexNonLinear, SafeComplexTransform, and
TransformFactor, along with both transform factors, the selected branch,
the selected branch argument, and the underlying sine, cosine, sum, and square-root intermediates. It also compares CUDA's paired `sincos` operation with separate CUDA calls and repeated pinned-Core/OpenLibm controls.

The deterministic input corpus includes Core-generated matrix values used
with seven nonce/header cases, the singularity and branch boundaries,
finite extremes, signed zero, subnormals, and NaN/Infinity cases. Non-finite
values are not passed to SafeComplexTransform; the pinned Core loop has no
NaN/Infinity escape. Inputs are capped at 32,768 records, the kernel uses one
bounded launch, and CTest has a 180-second timeout.

The report preserves the Core and CUDA binary64 bit patterns for every
mismatch and records absolute error, relative error, ULP difference, branch
selection, NaN/Infinity classification, and paired-versus-separate `sincos`
path comparisons. For the `exp` check it passes the exact OpenLibm sum bits as
a separate CUDA input, isolating device `exp` from trig differences. The CUDA
transcript also evaluates a diagnostic-only device translation of pinned
OpenLibm `__ieee754_exp` from `src/e_exp.c` (Git blob
`639b9fe18dbae273d3182146bb9aebbd59c9e362`) on that same operand. Its source
copyright and permission notice are retained. This lets the test distinguish
the CUDA device-math result from a direct translation of the CPU oracle's
algorithm; it is not a production hash implementation. Full binary transcripts
and the input catalog are saved beside the report. A live-device run requires
this diagnostic port to match every CPU/OpenLibm `exp` bit pattern; native CUDA
math mismatches remain recorded without being treated as transform parity. The
test does not accept numerical
mismatches using an epsilon: it reports them as diagnostics, and bitwise
transform parity remains false until every compared output matches. A
no-GPU CI job may skip only recognized no-device, driver, or initialization
errors after setting PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP=1; runtime and kernel
failures remain failures. Physical acceptance sets
PEPEPOW_CUDA_REQUIRE_DEVICE=1.

The CUDA diagnostic also translates pinned OpenLibm `sincos`,
`__ieee754_rem_pio2`, and `__kernel_rem_pio2` from `src/s_sincos.c`,
`src/e_rem_pio2.c`, and `src/k_rem_pio2.c` (source blobs
`8628235644aa8e996cbecd37f1b220a74d269a08`,
`c09e38a3640f8b8b91faf69444db529542727671`, and
`9555d3ab5257717a20b33151db47d7d495582f00`). The original permission notices
are retained. It compares both remainder components and the quadrant as well
as sine, cosine, their sum, and `exp(sin + cos)`. All these port fields matched
the pinned Core/OpenLibm outputs bit-for-bit across the 29,224-input physical
corpus. This includes `MediumComplexNonLinear`,
`IntermediateComplexNonLinear`, and every finite-input `ComplexNonLinear`
result when computed through the OpenLibm ports; the intermediate port
preserves Core's singularity guard. The tested `SafeComplexTransform` output
also matches for all records (nonfinite records are safe-disabled). Three
nonfinite `ComplexNonLinear` NaN bit patterns differ, but their classifications
match. An initial mismatch
exposed a missing `z = q[jz]` at entry to the reducer's recompute loop; that
source operation is now present and both a strict UCRT64 host translation and
the RTX 3060 device run match. The port remains diagnostic-only. Native CUDA
device-math fields still differ, and this does not establish full HooHash
digest parity.

The Windows RTX 3060 experiment found bitwise-identical outputs between paired
`sincos` and separate `sin`/`cos` calls for both OpenLibm and CUDA across the
full 29,224-input corpus. The diagnostic OpenLibm `exp` translation matched
all 29,224 CPU outputs, while native CUDA `exp` differed in 2,433 cases. The
new OpenLibm `sincos` translation and its composed `MediumComplexNonLinear`
result also match all 29,224 CPU outputs. The port-composed complex path matches
all finite inputs; native CUDA `sin` and `cos` still differ from Core/OpenLibm,
so the complete GPU hash path remains unvalidated. See the dated
[physical-device evidence](../../docs/validation/cuda-hoohash-v110-fp64-transform-windows-rtx3060-2026-11-27.md)
for per-function counts, the corrected intermediate result, reproducer,
compiler caveat, and artifact hashes.

The CUDA target compiles with --fmad=false, --ftz=false, --prec-div=true, and
--prec-sqrt=true; fast math is not enabled. The CI workflow inspects the
generated build commands to verify these flags. This is a diagnostic stage,
not a production hash path. It does not claim full digest parity, GPU mining
readiness, G4 acceptance, or permission to publish a GPU miner.

Configure the pinned sources and OpenLibm build explicitly:

```powershell
cmake -S tools/gpu -B build/cuda-hoohash-transform `
  -DPEPEPOW_CUDA_HOOHASH_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_CORE_MATRIX_ORACLE_TESTS=ON `
  -DPEPEPOW_CUDA_MATRIX_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_TRANSFORM_DIAGNOSTICS=ON `
  -DPEPEPOW_CORE_SOURCE_DIR=D:/Dev/pepepow-deps/PePe-core `
  -DPEPEPOW_OPENLIBM_SOURCE_DIR=D:/Dev/pepepow-deps/openlibm `
  -DPEPEPOW_OPENLIBM_LIBRARY=D:/path/to/libopenlibm.a `
  -DPEPEPOW_TRANSFORM_ORACLE_COMPILER=D:/Dev/msys64/ucrt64/bin/gcc.exe `
  -DPEPEPOW_CUDA_TEST_ARCHITECTURE=86
cmake --build build/cuda-hoohash-transform --config Release
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = "1"
ctest --test-dir build/cuda-hoohash-transform -C Release --output-on-failure
```

See the [Windows RTX 3060 transform diagnostic evidence](../../docs/validation/cuda-hoohash-v110-fp64-transform-windows-rtx3060-2026-11-27.md)
for the measured OpenLibm/libdevice differences. The transform diagnostic by
itself does not establish full-digest GPU correctness.

## Experimental full-digest stage

The separate `PEPEPOW_CUDA_FULL_DIGEST_TESTS` option connects the GPU BLAKE3
header passes, the already compared 4,096-element matrix generator, the
OpenLibm FP64 diagnostic translations, the pinned Core row/nonce reduction,
and the final BLAKE3 digest. A batch accepts one 80-byte header template and a
bounded inclusive uint32 nonce range; nonce bytes are written little-endian
at offsets 76–79. The matrix seed is computed once with those bytes zeroed and
the resulting 32 KiB matrix is reused across the batch. Each output record is
the complete 32-byte digest in the byte order returned by pinned Core.

The CPU oracle includes the unmodified pinned Core HooHash source and links
that Core's BLAKE3 implementation with the pinned OpenLibm static library. The
Python contract checks fixed Core digest fixtures, nonce zero, adjacent
nonces, `UINT32_MAX`, overflow rejection, and every digest byte. Generic CUDA
runtime and kernel failures fail the test; only recognized no-device/driver/
initialization errors may skip in an explicitly configured no-GPU CI job. A
physical-device run sets `PEPEPOW_CUDA_REQUIRE_DEVICE=1`. Passing a bounded
batch is evidence for those tested inputs only: this remains an experimental
correctness test. The Windows RTX 3060 record below includes one 10,000-nonce
full-digest parity run. This does not claim a production GPU miner, hashrate,
G4 acceptance, or redistribution clearance.

Enable the full-digest experiment along with all prerequisite diagnostics:

```powershell
cmake -S tools/gpu -B build/cuda-hoohash-transform `
  -DPEPEPOW_CUDA_HOOHASH_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_CORE_MATRIX_ORACLE_TESTS=ON `
  -DPEPEPOW_CUDA_MATRIX_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_TRANSFORM_DIAGNOSTICS=ON `
  -DPEPEPOW_CUDA_FULL_DIGEST_TESTS=ON `
  -DPEPEPOW_CUDA_FULL_DIGEST_10000_TESTS=ON `
  -DPEPEPOW_CORE_SOURCE_DIR=D:/Dev/pepepow-deps/PePe-core `
  -DPEPEPOW_OPENLIBM_SOURCE_DIR=D:/Dev/pepepow-deps/openlibm `
  -DPEPEPOW_OPENLIBM_LIBRARY=D:/path/to/libopenlibm.a `
  -DPEPEPOW_TRANSFORM_ORACLE_COMPILER=D:/Dev/msys64/ucrt64/bin/gcc.exe `
  -DPEPEPOW_CUDA_TEST_ARCHITECTURE=86
cmake --build build/cuda-hoohash-transform --config Release
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = "1"
ctest --test-dir build/cuda-hoohash-transform -C Release --output-on-failure
```

The 10,000-nonce case is bounded to nonce 0 through 9,999 and has a 30-minute
CTest timeout. On systems without an NVIDIA device, set
`PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP=1` for the run; recognized unavailable-
device errors may skip the GPU execution after all pinned CPU fixtures pass.
The exact Windows physical-device result is recorded in the dated
[full-digest validation report](../../docs/validation/cuda-hoohash-v110-full-digest-windows-rtx3060-2026-11-27.md).

The digest transcript starts with `PPWDIG1\0`, a little-endian count, and
exactly 32 bytes per nonce. The stage makes no network connection and does
not submit shares or benchmark mining throughput.

Build and run the stage contract with CUDA Toolkit 13.3.x and a pinned Core
checkout:

```powershell
cmake -S tools/gpu -B build/cuda-blake3-stage `
  -DPEPEPOW_CUDA_HOOHASH_STAGE_TESTS=ON `
  -DPEPEPOW_CORE_SOURCE_DIR=D:/Dev/pepepow-deps/PePe-core `
  -DPEPEPOW_CORE_EXPECTED_SHA=bc08108ceccd957c79040dc7fedc1d56d8d47f97 `
  -DCUDAToolkit_ROOT="$env:CUDA_PATH"
cmake --build build/cuda-blake3-stage --config Release
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = "1"
ctest --test-dir build/cuda-blake3-stage -C Release --output-on-failure
```

The standalone stage rejects any other Core commit and any tracked or
untracked Core checkout changes. A no-GPU CI job explicitly sets
`PEPEPOW_CUDA_ALLOW_NO_DEVICE_SKIP=1`; only recognized unavailable-device
responses may skip live execution. Generic CUDA/runtime failures always fail,
and physical-device acceptance sets `PEPEPOW_CUDA_REQUIRE_DEVICE=1`.

The selected CUDA Toolkit must provide compiler support files. NVIDIA's CUDA
13.3.1 guide lists MSVC 193x for Visual Studio 2022; local MSVC 19.44 results
remain local build evidence and are outside that published range. The
standalone GPU project does not change the default CPU build.

## NVIDIA references

- [CUDA Runtime API: device properties](https://docs.nvidia.com/cuda/cuda-runtime-api/structcudaDeviceProp.html)
- [CUDA Runtime API: device management](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__DEVICE.html)
- [CUDA Runtime API: versions](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART____VERSION.html)
- [CUDA C++ Programming Guide: compute capabilities](https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/compute-capabilities.html)
- [CUDA 13.3.1 Windows Installation Guide](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-installation-guide-microsoft-windows/index.html)
- [CUDA 13.3.1 Linux Installation Guide](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-installation-guide-linux/index.html)
- [CUDA 13.3.1 Release Notes](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-toolkit-release-notes/)
