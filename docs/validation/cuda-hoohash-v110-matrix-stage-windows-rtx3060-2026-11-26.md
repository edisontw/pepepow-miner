# HooHash V110 CUDA Matrix Stage Evidence

Date: 2026-11-26 (Asia/Taipei)

## Scope

This stage implements only the HooHash PRNG matrix-generation step on CUDA.
Each GPU transcript is compared byte-for-byte with an observation emitted by
the test-only wrapper around the unmodified pinned Core `hoohash.c` and checked
against the independent Python oracle. The kernel advances each seed's PRNG
sequentially and emits all 4096 row-major binary64 values.

It does not implement the nonlinear transforms, final digest, full GPU
HooHash, mining, pool connectivity, candidate submission, or hashrate
measurement. No sustained GPU load or hardware setting change was performed.

## Pinned source and behavior

- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`, clean checkout.
- Core `src/crypto/hoohash/hoohash.c` SHA-256:
  `7a42a02116bb645768eae7dd93096c4790e8f06871bbb71fa82fb0a5a9108a4a`.
- Actual Core PRNG output is `rotl64(s0 + s3, 23) + s0` followed by its
  xoshiro transition, despite the source comment calling it xoshiro256**.
- Matrix expression and operation order:
  `(double)lower_4_bytes / (double)UINT32_MAX * 1000000.0`.
- CUDA uses `__ddiv_rn` for round-to-nearest-even division and `--fmad=false`
  to prevent floating-point contraction. No `--use_fast_math` flag is present.
  The generated Visual Studio CUDA project was inspected and contains
  `--generate-code=arch=compute_86,code=[compute_86,sm_86] --fmad=false` in
  both Debug and Release configurations.
- NVIDIA's [CUDA Math API](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-math-api/index.html)
  documents `__ddiv_rn`; the [CUDA 13.3.1 NVCC guide](https://docs.nvidia.com/cuda/archive/13.3.1/cuda-compiler-driver-nvcc/index.html)
  documents `--fmad` and fast-math behavior.

## Native Windows host and toolchain

- Windows 11 Pro x64, build 26300.
- NVIDIA GeForce RTX 3060, compute capability 8.6, 12,288 MiB VRAM, driver
  610.88.
- Visual Studio 2022 Build Tools, MSVC 19.44.35230, Windows SDK 10.0.26100.
- CUDA Toolkit 13.3.1, NVCC 13.3.73, CMake 3.31.6, x64 CPython 3.13.17.
- CUDA files used the existing isolated NVIDIA Toolkit component overlay at
  `D:\Dev\pepepow-cuda-toolkit-13.3.1-overlay`. NVIDIA's CUDA 13.3.1 Windows
  guide lists MSVC 193x for Visual Studio 2022; installed MSVC 19.44 is outside
  that range. Successful local execution is device evidence, not an official
  toolchain support qualification.

## Corpus and resource bounds

The test verifies the nonce-6 Explorer header seed, 31 additional deterministic
header templates, and a synthetic 32-byte all-`0xff` seed. The same header
template with nonce 6 and `UINT32_MAX` produces one identical Core matrix seed.
The all-`0xff` state exercises 8,160 modulo-2^64 wrap events in the 4096-step
sequence.

Each seed has 4096 transcript observations. For 33 seeds the GPU produced
135,168 complete PRNG output/state/lower-value/matrix-bit records. The
observation buffer occupied 7,569,408 bytes; the output transcript was
7,030,860 bytes. The executable rejects more than 64 seeds, bounding the
observation allocation to 14,680,064 bytes.

The negative control flipped the last bit of the last matrix element. The same
complete-transcript comparator rejected it at row-major element 4095 in the
`matrix binary64 bits` field.

## Build and physical-device test results

The project was configured with the standalone Visual Studio 2022 x64
generator, Toolkit overlay above, Core commit above, and CUDA architecture
86. Debug and Release builds succeeded. With
`PEPEPOW_CUDA_REQUIRE_DEVICE=1` and no no-device skip override, CTest passed
5/5 in both configurations. Both runs reported:

```text
PINNED CORE matrix oracle: 33 seeds, 4096 elements per seed
UINT64 OVERFLOW CASE: all-FF initial state exercised 8160 modulo-2^64 wrap events
LIVE CUDA MATRIX: 33 seeds x 4096 elements; 135168 PRNG values, states, and binary64 patterns match pinned Core byte-for-byte
NEGATIVE CONTROL: final matrix element mutation rejected
ERROR CONTRACT: malformed corpus and host output failures return stable nonzero JSON
LIVE: 1 CUDA device(s) reported
100% tests passed, 0 tests failed
```

Both configurations also passed the four prior pinned-Core/CUDA BLAKE3
full-digest stage vectors. The device-required environment prevents a no-GPU
skip from satisfying local acceptance. The matrix stage test itself reports a
recognized unavailable-device result as a skip only when the CI opt-in is
explicit and device-required mode is off; generic runtime failures always
fail.

Full logs are outside the repository at
`D:\Dev\pepepow-cuda-prng-matrix-evidence-20261126`:

- `configure-matrix-debug.log`
- `build-matrix-debug.log`, `build-matrix-release.log`
- `ctest-matrix-debug.log`, `ctest-matrix-release.log`

## Limits

This establishes bitwise parity for the pinned-Core PRNG and matrix generation
over the listed 33 seeds on one RTX 3060. It does not establish CUDA nonlinear
transform parity, complete 32-byte HooHash V110 digest parity, 10,000-case GPU
parity, candidate CPU recheck, performance, cross-device reproducibility, or
G4 acceptance. No GPU miner is claimed usable.
