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

## Full-digest regression on the bounded-search head

The bounded-search change also modifies the CUDA digest-stage translation
unit. Re-run the four-template 10,000-nonce full-digest corpus on the exact
bounded-search head to check that path after the search additions:

- Repository head: `7064a0a053c570e46fb5e4ff08268b0562754c81` (draft PR #38),
  based on PR #37 head `38873a5606f9cf722c28e5871ab1659521f48cba`.
- Core oracle source: PEPEPOW Core
  `bc08108ceccd957c79040dc7fedc1d56d8d47f97`; OpenLibm
  `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- Windows 11 Pro build 26300 x64; RTX 3060, sm_86, 12 GiB, driver 610.88;
  CUDA Toolkit 13.3.1 / NVCC 13.3.73; CMake 4.5.1; MSVC 19.44.35230; native
  CPython 3.13.17 and MSYS2 UCRT64 GCC 16.2.0.
- `PEPEPOW_CUDA_REQUIRE_DEVICE=1` was set for both CTest runs. Missing GPU,
  driver, initialization, and generic Runtime/kernel errors therefore could
  not be accepted as no-device skips.
- Debug CTest passed `1/1` in `1.59 s`; Release CTest passed `1/1` in
  `1.66 s`. Both ran
  `hoohash_cuda_full_digest_10000_nonce_parity` and reported a live RTX 3060.
- Each configuration compared 10,005 full 32-byte digests: 10,000 nonces
  across four header templates, plus nonce 0, Explorer nonces 6–8, and
  `UINT32_MAX`. There were zero mismatches in either configuration.
- The four templates each covered all 256 low nonce-byte values. Per-template
  transcript SHA-256 matched between CPU/Core and CUDA; the matching hashes
  were identical between Debug and Release as well.
- Generated Visual Studio projects were inspected: both configurations target
  `compute_86`/`sm_86` with `--fmad=false`, `--ftz=false`, `--prec-div=true`,
  and `--prec-sqrt=true`; `--use_fast_math` was absent.

The separate build directory was configured with the following command; the
Core and OpenLibm checkouts were clean at their pinned SHAs:

```powershell
& 'D:\Dev\msys64\ucrt64\bin\cmake.exe' `
  -S 'D:\Dev\pepepow-cuda-bounded-search-20261127\tools\gpu' `
  -B 'D:\Dev\pepepow-cuda-full-digest-10k-20261127' `
  -G 'Visual Studio 17 2022' -A x64 `
  -DCMAKE_GENERATOR_INSTANCE='D:/Dev/vs2022-build-tools' `
  -DCMAKE_CUDA_COMPILER='C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3/bin/nvcc.exe' `
  -DCUDAToolkit_ROOT='D:/Dev/pepepow-cuda-toolkit-13.3.1-overlay' `
  -DPEPEPOW_CORE_SOURCE_DIR='D:/Dev/pepepow-deps/PePe-core' `
  -DPEPEPOW_OPENLIBM_SOURCE_DIR='D:/Dev/pepepow-deps/openlibm' `
  -DPEPEPOW_OPENLIBM_LIBRARY='D:/Dev/pepepow-miner-validation/20261124-windows11/build/openlibm-release/libopenlibm.a' `
  -DPEPEPOW_TRANSFORM_ORACLE_COMPILER='D:/Dev/msys64/ucrt64/bin/gcc.exe' `
  -DPEPEPOW_CUDA_TEST_ARCHITECTURE=86 `
  -DPEPEPOW_CUDA_HOOHASH_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_CORE_MATRIX_ORACLE_TESTS=ON `
  -DPEPEPOW_CUDA_MATRIX_STAGE_TESTS=ON `
  -DPEPEPOW_CUDA_TRANSFORM_DIAGNOSTICS=ON `
  -DPEPEPOW_CUDA_FULL_DIGEST_TESTS=ON `
  -DPEPEPOW_CUDA_FULL_DIGEST_10000_TESTS=ON `
  -DPEPEPOW_CUDA_BOUNDED_SEARCH_TESTS=ON `
  -DBUILD_TESTING=ON

& 'D:\Dev\msys64\ucrt64\bin\cmake.exe' --build `
  'D:\Dev\pepepow-cuda-full-digest-10k-20261127' --config Release --parallel 8
$env:PEPEPOW_CUDA_REQUIRE_DEVICE = '1'
& 'D:\Dev\msys64\ucrt64\bin\ctest.exe' `
  --test-dir 'D:\Dev\pepepow-cuda-full-digest-10k-20261127' `
  -C Release -R '^hoohash_cuda_full_digest_10000_nonce_parity$' `
  --output-on-failure --verbose

& 'D:\Dev\msys64\ucrt64\bin\cmake.exe' --build `
  'D:\Dev\pepepow-cuda-full-digest-10k-20261127' --config Debug --parallel 8
& 'D:\Dev\msys64\ucrt64\bin\ctest.exe' `
  --test-dir 'D:\Dev\pepepow-cuda-full-digest-10k-20261127' `
  -C Debug -R '^hoohash_cuda_full_digest_10000_nonce_parity$' `
  --output-on-failure --verbose
```

| Transcript | Cases | Matching CPU/GPU SHA-256 |
| --- | ---: | --- |
| Base header, nonces 0–2,499 | 2,500 | `ac329e151d1d6d480a66ff5aad07c6b3f5569956400b527cf7ef8575b901ce1e` |
| Version byte offset 0 XOR 1 | 2,500 | `1f14ec32a5c907d4517c11092a0aaf547613474e7d42f236c3f5c57a576eeb90` |
| Previous-hash byte offset 4 XOR 1 | 2,500 | `b63cf04d5b7be3d7e1d2f78b32e0ae0d9f3effa366f54e86a5ccfb3897d6bc80` |
| Merkle-root byte offset 36 XOR 1 | 2,500 | `a9c0b3b71a69dd86537f3d5580bbcd8a068c340ce90afda653e43ee2a5e2a6f6` |
| Fixed nonce 0 | 1 | `4bdb5faad086781ff7f92308f381e2cb060f48acd91572eb45d8d13af62fdff4` |
| Fixed Explorer nonces 6–8 | 3 | `a75609d933bfc5cdae4fe05fb612f88db482b54f72431a21cc9ca381a6062622` |
| Fixed `UINT32_MAX` | 1 | `dedcebdf8fa47e4ab8ed7cb517c27e839e5bf82c19dc776cef38251a0315f337` |

The JSON run reports are checked in as
[`Debug`](cuda-full-digest-10000-debug-windows-rtx3060-2026-11-27.json) and
[`Release`](cuda-full-digest-10000-release-windows-rtx3060-2026-11-27.json).
Their SHA-256 values are respectively
`519ec30f36c165e03e57b9b075d20cd5a5590ab44c96e07fa2f3dd5d7e91d878` and
`5737cff4a8892040dedf654fd541b36b17d002996de3a06de108f01003dec299`.
The complete CPU and GPU binary transcripts and JSON report are retained in
both local configuration directories:
`D:\Dev\pepepow-cuda-full-digest-10k-20261127\cuda-full-digest-10000-debug\`
and `D:\Dev\pepepow-cuda-full-digest-10k-20261127\cuda-full-digest-10000-release\`.
The direct Release run log is in the Release directory; both verbose CTest
results are captured above. Debug and Release digest-stage executable SHA-256 values are
`409c26404a28f64554bac0a8ff91d31159f8f65feca369a6fa0c656f2a2d6c0a` and
`a2d1ea7c35512ac3f466276e65d4797cdcd7696db293ac6c0f7fc34dc8b07493`.

This is a bounded regression corpus for the offline diagnostic digest stage.
It is not an exhaustive all-input proof, a Stratum-integrated GPU miner, G4
acceptance, or a mining hashrate measurement. The CUDA 13.3.1 installation
guide lists VS 2022 MSVC 193x; MSVC 19.44 is outside that published range, so
these remain local experimental compatibility results.

## Review follow-up — 2026-11-28

Two review findings on PR #38 were reproduced and fixed: a deadline/cancellation
that arrives during a successful pinned-Core recheck now suppresses candidate
publication, and the one-millisecond deadline contract no longer assumes a
hardware-dependent batch count.

The physical-device regression test used a delayed oracle proxy (250 ms delay)
with a 100 ms deadline. On Windows 11 x64 with an NVIDIA GeForce RTX 3060
(compute capability 8.6, CUDA driver/runtime API 13030), CUDA Toolkit 13.3.1 /
NVCC 13.3.73, MSVC 19.44.35230.0, and Visual Studio 17 2022 x64, the test
reached a successful CPU recheck then returned cancelled / timeout, with one
nonce processed and no published candidates. The JSON result is retained
as [deadline-during-successful-recheck](cuda-bounded-search-deadline-during-recheck-windows-rtx3060-2026-11-28.json).

The Windows build used the existing CUDA 13.3.1 compatibility overlay, pinned
Core bc08108ceccd957c79040dc7fedc1d56d8d47f97, OpenLibm source
9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d, and the previously built OpenLibm
static archive. The bounded-search test target built successfully in Debug
and Release. With PEPEPOW_CUDA_REQUIRE_DEVICE=1, CTest
cuda_hoohash_bounded_search_core_recheck passed 1/1 in each configuration.
The fixture also verified its marker was created, and no engine, proxy, or
oracle process remained after testing.

This follow-up covers cancellation/deadline publication safety only. It does
not change the consensus hash path or establish G4 readiness.
