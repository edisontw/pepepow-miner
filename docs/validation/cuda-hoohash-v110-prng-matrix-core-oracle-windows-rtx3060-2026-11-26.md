# Pinned-Core HooHash PRNG and Matrix Oracle Evidence

Date: 2026-11-26 (Asia/Taipei)

## Scope

This P3-B stage builds a test-only wrapper around the unmodified pinned Core
`hoohash.c`. The wrapper records the Core's xoshiro state initialization,
output, post-step state, lower 32-bit value, and each row-major binary64 matrix
value. An independent Python implementation checks every recorded field.

The wrapper does not modify the production CPU hash path, launch a CUDA matrix
kernel, implement the nonlinear transforms, produce a HooHash digest, mine,
contact a pool, or benchmark hashrate. The GPU matrix comparison is the next
stage.

## Pinned source and observed behavior

- PEPEPOW Core commit: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- Core checkout was clean, including untracked-file status, when CMake
  configured the oracle.
- `src/crypto/hoohash/hoohash.c` SHA-256:
  `7a42a02116bb645768eae7dd93096c4790e8f06871bbb71fa82fb0a5a9108a4a`.
- The source comment calls the generator xoshiro256**, but its actual output
  is `rotl64(s0 + s3, 23) + s0`. The oracle includes and calls the real static
  Core functions; the production source is not patched.
- The matrix value is `(double)lower_4_bytes / (double)UINT32_MAX *
  1000000.0`; row-major element order is retained.
- The reference target requires IEEE 754 binary64 and uses `/fp:strict` on
  MSVC. Fast-math and FP contraction are disabled on non-MSVC compilers.

## Native Windows host

- Windows 11 Pro x64, build 26300.
- NVIDIA GeForce RTX 3060, compute capability 8.6, 12,288 MiB, driver
  610.88.
- Visual Studio 2022 Build Tools, MSVC 19.44.35230, Windows SDK 10.0.26100.
- CUDA Toolkit 13.3.1, NVCC 13.3.73.
- CMake 3.31.6, x64 CPython 3.13.17.
- The CUDA BLAKE3 baseline build also uses the existing isolated official
  Toolkit overlay at `D:\Dev\pepepow-cuda-toolkit-13.3.1-overlay`. MSVC 19.44
  is outside CUDA 13.3.1's published VS 2022 compiler range; the successful
  local build is device evidence, not an official support qualification.

## Corpus and binary transcript

The contract builds 32 distinct matrix seeds from pinned-Core BLAKE3 of the
nonce-6 reference header and 31 deterministic header templates. It separately
checks that nonce 6 and `UINT32_MAX`, encoded little-endian in the same header
template, produce the same matrix seed. The nonce-6 seed is
`28d0be17bea0de270743b7c09c785157504aecc287f62e2673051bbadf764d6d`.

For each seed the binary observation contains its 32 seed bytes, four initial
little-endian uint64 state words, and 4096 records. Each 52-byte record stores
the PRNG output (uint64), post-step state (four uint64 values), lower 32 bits,
and matrix element IEEE 754 binary64 bits. Both container and fields have a
fixed little-endian contract. The test checks 131,072 PRNG outputs, 131,072
post-step states, and 131,072 complete matrix bit patterns.

## Build and test

Configured the standalone CUDA project with
`PEPEPOW_CUDA_HOOHASH_STAGE_TESTS=ON`,
`PEPEPOW_CUDA_CORE_MATRIX_ORACLE_TESTS=ON`, CUDA architecture 86, and the exact
clean Core checkout. Visual Studio Debug and Release builds both succeeded.
With `PEPEPOW_CUDA_REQUIRE_DEVICE=1`, CTest passed 4/4 in Debug and 4/4 in
Release. The output from each configuration included:

```text
PINNED CORE: four complete 80-byte BLAKE3 stage vectors PASS
LIVE CUDA: nonce-6/header, complete 32-byte digest PASS
LIVE CUDA: nonce-6/matrix-seed, complete 32-byte digest PASS
LIVE CUDA: uint32-max/header, complete 32-byte digest PASS
LIVE CUDA: uint32-max/matrix-seed, complete 32-byte digest PASS
PINNED CORE matrix oracle: 32 seeds, 4096 elements per seed
PINNED CORE MATRIX ORACLE CONTRACT PASS: 32 distinct seeds; 131072 PRNG outputs/states and 131072 binary64 matrix bit patterns checked
MOCK: no_device, driver_unavailable, initialization_failed, and query_failed JSON fixtures PASS
LIVE: 1 CUDA device reported
100% tests passed, 0 tests failed
```

`PEPEPOW_CUDA_REQUIRE_DEVICE=1` prevents device-unavailable results from
skipping the live baseline tests. The new matrix oracle itself is CPU code;
its CUDA matrix transcript comparison is not yet implemented.

Full local logs are outside the repository at
`D:\Dev\pepepow-cuda-prng-matrix-evidence-20261126`:

- `configure-oracle-debug.log`
- `build-core-oracle-debug.log`, `build-core-oracle-release.log`
- `ctest-core-oracle-debug.log`, `ctest-core-oracle-release.log`

## Limits

This validates the pinned Core intermediate oracle and the prior BLAKE3 CUDA
stage only. It does not establish CUDA matrix parity, FP64 transform parity,
complete HooHash V110 digest parity, GPU mining readiness, or G4 acceptance.
