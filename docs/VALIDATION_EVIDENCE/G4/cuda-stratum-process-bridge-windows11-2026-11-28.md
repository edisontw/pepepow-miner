# CUDA Stratum Process Bridge — Windows 11 CPU/Mock Evidence

## Scope

This record covers the opt-in process adapter between the native Stratum
session and the standalone bounded CUDA search executable. It does not claim
physical GPU execution, full CUDA HooHash digest parity, G4 acceptance, a pool
connection, or share submission. All tests in this record use a CPU mock
child process for the search process contract.

## Source and toolchain

- Miner base: PR #41 integration branch, starting commit
  `c8eb30219c1f03bae6cac8c5938e537e04b7f9af`.
- Pinned PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OS: Windows 11 Pro x64, build `26300`.
- MSYS2 UCRT64: GCC `16.2.0`, CMake `4.5.1`, Ninja `1.13.2`.
- CMake-selected Python: native CPython `3.13.17`.
- OpenSSL found by CMake: MSYS2 UCRT64 `libcrypto.dll.a`, version `4.1.0`.
- MSVC compile-only check: Visual Studio 2022 Build Tools, MSVC `19.44.35230`,
  `/std:c++20 /W4 /WX`; the process-contract test source compiled successfully.
- CUDA Toolkit and RTX 3060 remain recorded in prior native evidence. No CUDA
  command or GPU test ran in this round, so this change introduced no GPU load
  or VRAM allocation.

## Build and test commands

Builds were kept outside the checkout in separate Release and Debug directories.
Both configurations pinned the Core source SHA above, used the installed
OpenLibm headers/library, enabled the native Stratum search and opt-in live
miner targets, and kept the CUDA Toolkit out of the root build.

```powershell
$env:PATH = 'D:\Dev\msys64\ucrt64\bin;' + $env:PATH
cmake --build D:\Dev\pepepow-cuda-stratum-bridge-cmake-20261128 `
  --target test_cuda_search_process test_cuda_search_result_decoder `
  test_native_stratum_search offline_stratum_miner `
  pepepow-live-miner pepepow-live-cpu-miner --parallel 4
ctest --test-dir D:\Dev\pepepow-cuda-stratum-bridge-cmake-20261128 `
  -R '^(cuda_search_process_contract|cuda_search_result_decoder|native_stratum_search|offline_stratum_miner_loopback|live_pool_miner_loopback|live_pool_miner_cuda_product_loopback)$' `
  --output-on-failure

cmake --build D:\Dev\pepepow-cuda-stratum-bridge-debug-20261128 `
  --target test_cuda_search_process test_cuda_search_result_decoder `
  test_native_stratum_search pepepow-live-miner pepepow-live-cpu-miner --parallel 4
ctest --test-dir D:\Dev\pepepow-cuda-stratum-bridge-debug-20261128 `
  -R '^(cuda_search_process_contract|cuda_search_result_decoder|native_stratum_search|live_pool_miner_loopback|live_pool_miner_cuda_product_loopback)$' `
  --output-on-failure

cmd.exe /c '"D:\Dev\vs2022-build-tools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++20 /EHsc /W4 /WX /ID:\Dev\pepepow-cuda-stratum-bridge-20261128\include /c D:\Dev\pepepow-cuda-stratum-bridge-20261128\tests\test_cuda_search_process.cpp /Fo:D:\Dev\pepepow-cuda-stratum-bridge-msvc-20261128\test_cuda_search_process.obj'
```

## Results

- The broader Release set passed `6/6` in `10.99 s`, and the broader Debug
  set passed `5/5` in `1.13 s`.
- After adding an explicit Windows inherited-handle allowlist for the child,
  and bounded descendant cleanup through its Windows Job Object, the final
  process and CUDA-product loopback tests passed again: Release `2/2`, Debug
  `2/2`.
- Cancellation grace now starts when the parent signals cancellation, rather
  than at the engine invocation deadline. A new uncooperative-child mock was
  terminated within the configured 150 ms grace plus test scheduling overhead;
  the process contract test passed in both Release and Debug.
- MSVC `/W4 /WX` compile-only check of `test_cuda_search_process.cpp` passed.
- The mock child returned a candidate at `UINT32_MAX`; the parent recomputed
  the complete 32-byte digest with pinned Core before returning it.
- A last-digest-byte mutation and a below-target mock candidate were rejected.
- An atomic cancel request produced a cancel file; the child exited
  cooperatively, with measured test stop time below two seconds.
- A mock child that deliberately ignored its cancel file was terminated within
  600 ms, proving a user stop does not wait for the longer engine timeout.
- An expiring parent deadline also caused cooperative child cancellation and
  returned timeout without a candidate.
- Per-invocation temporary result, stderr, and cancel files were removed after
  both normal and cancelled executions.
- The live CLI contract retained CPU as its default and rejected incomplete
  CUDA settings, CUDA paths selected with CPU mode, and missing engine/oracle
  files before attempting a connection. Loopback tests used only a local mock.
- An exploratory compile of the entire miner using MSVC while adding MSYS2
  UCRT64 OpenSSL headers failed due to incompatible MSVC and MinGW CRT headers.
  It is not a supported mixed-toolchain configuration; this record does not
  claim an MSVC linked build. The supported Windows-native UCRT64 GCC build
  succeeded in Debug and Release.

The tests did not initialize CUDA, allocate VRAM, connect to a pool, or submit
shares. They validate process control and result integrity only. Physical GPU
integration and full digest parity remain outstanding.
