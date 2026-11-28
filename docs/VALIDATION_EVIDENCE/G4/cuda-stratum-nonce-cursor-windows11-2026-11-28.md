# Live Stratum nonce cursor validation — Windows 11 x64 — 2026-11-28

## Change under test

The live Stratum session now delegates nonce-range accounting and extranonce2
rollover to `include/pepepow/stratum_nonce_cursor.hpp`. The cursor uses a
64-bit next-nonce sentinel so the end of the uint32 nonce space is represented
without wrapping to zero. Batch reservations are bounded by the remaining
nonce space. Extranonce2 increment validates the complete hex input before
mutating it and fails without mutation when the value is malformed or all `f`.

## Native environment

- OS: Windows 11 Pro x64.
- Build: MSYS2 UCRT64 GCC 16.2.0, CMake 4.5.1, Ninja 1.13.2.
- Build directories: `D:/Dev/pepepow-cuda-stratum-bridge-cmake-20261128` (Release) and `D:/Dev/pepepow-cuda-stratum-bridge-debug-20261128` (Debug).
- The product loopback uses CPU-only child-process fixtures. No CUDA device was initialized, no public pool was contacted, and no share was submitted.

## Commands and results

With `D:/Dev/msys64/ucrt64/bin` and `D:/Dev/msys64/usr/bin` prepended to
`PATH`, the following commands passed:

```powershell
cmake --build D:/Dev/pepepow-cuda-stratum-bridge-cmake-20261128 --target test_stratum_nonce_cursor pepepow-live-miner --parallel 4
ctest --test-dir D:/Dev/pepepow-cuda-stratum-bridge-cmake-20261128 --output-on-failure -R 'stratum_nonce_cursor_contract|live_pool_miner_cuda_product_loopback'
cmake --build D:/Dev/pepepow-cuda-stratum-bridge-debug-20261128 --target test_stratum_nonce_cursor pepepow-live-miner --parallel 4
ctest --test-dir D:/Dev/pepepow-cuda-stratum-bridge-debug-20261128 --output-on-failure -R 'stratum_nonce_cursor_contract|live_pool_miner_cuda_product_loopback'
```

Both CTest invocations passed 2/2 tests. The unit contract covers bounded
reservations, final-nonce exhaustion, no wraparound, candidate advancement at
`UINT32_MAX`, cursor reset, lowercase and mixed-case hex increment, malformed
input, and all-`f` overflow. The product loopback verifies the refactored
session path remains compatible with the existing offline Stratum fixtures.

This is CPU/mock contract evidence only. Full GPU hash parity, physical CUDA
search, formal G4, live pool interoperability, and G1 acceptance remain
pending.
