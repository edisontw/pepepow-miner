# CUDA HooHash V110 FP64 Transform Diagnostic Evidence

Date: 2026-11-27 (Asia/Taipei)  
Result: diagnostic framework passed on a physical GPU; bitwise FP64 transform parity **not passed**.

## Host and toolchain

- Host: Windows 11, build 26300, x64.
- GPU: NVIDIA GeForce RTX 3060, device 0, compute capability 8.6, 12,288 MiB installed VRAM.
- Display driver package: 610.88. CUDA reported driver API support 13.3 (`13030`) and runtime 13.3 (`13030`).
- CUDA Toolkit: 13.3.1, NVCC 13.3.73.
- Host compiler: Visual Studio 2022 Build Tools, MSVC 19.44.35230.
- CMake: 3.31.6-msvc6; generator: Visual Studio 17 2022.
- CPU oracle compiler: MSYS2 UCRT64 GCC 16.2.0.
- CPU-oracle CMake options required by the report: `-fno-fast-math`, `-ffp-contract=off`, `-frounding-math`, and `-fno-builtin`.
- CUDA architecture: `sm_86`; required flags: `--fmad=false`, `--ftz=false`, `--prec-div=true`, and `--prec-sqrt=true`. `--use_fast_math` was absent.

The Windows build used the locally prepared CUDA 13.3.1 Toolkit overlay at `D:\Dev\pepepow-cuda-toolkit-13.3.1-overlay` with MSVC 19.44. NVIDIA's CUDA 13.3.1 Windows installation guide lists the Visual Studio 2022 host compiler as MSVC 193x. MSVC 19.44 is outside that listed range. The successful build and device run are local evidence, not a claim that this host compiler combination is in NVIDIA's supported matrix.

## Test scope and method

The diagnostic calls the unmodified transform functions from pinned PEPEPOW Core through a test-only C wrapper and compares their full binary64 transcripts with a bounded CUDA kernel. It exercised 29,224 inputs sourced from pinned-Core matrices, seven fixed header/nonce cases, deterministic sweeps, branch boundaries, finite extremes, signed zeros, subnormals, infinities, and NaNs. Safe-transform special-value calls that can fail to terminate in the pinned Core implementation were excluded; non-finite inputs were still classified and compared in the other supported diagnostic functions.

CTest was run with `PEPEPOW_CUDA_REQUIRE_DEVICE=1`, so an unavailable-device response could not be accepted as a skip:

| Configuration | Build | CTest |
| --- | --- | --- |
| Debug | Passed | 6/6 passed, 2.87 s |
| Release | Passed | 6/6 passed, 2.69 s |

The six tests cover the CUDA BLAKE3 stage, pinned-Core matrix oracle, CUDA matrix stage, this transform diagnostic, device-probe JSON contract, and no-device skip policy. This establishes that the harness and diagnostic binaries ran successfully; it does not turn numeric mismatches into passes.

## Physical-device observations

- GPU status was `live`; device error was `null`.
- The selected Core branch matched for all 29,224 inputs: zero branch mismatches.
- NaN/Infinity classification matched for all compared values.
- `factor_one`, `factor_two`, and `transform_factor`: zero bitwise mismatches.
- `medium_complex_nonlinear`: 5,605 bitwise mismatches; maximum finite difference 2 ULP.
- `intermediate_complex_nonlinear`: 4,214 bitwise mismatches; maximum finite difference 3 ULP.
- `complex_nonlinear`: 3,094 bitwise mismatches; maximum finite difference 3 ULP.
- `safe_complex_transform`: 3,091 bitwise mismatches; maximum finite difference 3 ULP.
- `high_complex_nonlinear`: one bitwise mismatch. Both results are NaNs with identical payload bits and opposite sign bits for the negative-payload-NaN input.
- The largest finite absolute difference was `1.7763568394002505e-15`; the largest finite relative difference was `6.56561143256613e-16`.

The observed finite differences are small but still fail the required bitwise comparison. They must not be accepted with an epsilon. Full HooHash V110 digest parity, the 10,000-case GPU digest suite, candidate CPU recheck, hashrate measurements, and G4 acceptance remain untested and blocked on resolving this transform mismatch.

## Provenance and retained artifacts

- PEPEPOW Core source: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OpenLibm source: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- OpenLibm archive SHA-256: `9fb7b4caf1deb2136314508c3116adafd8a192a67d0ca4a95e22744fa0ca5f96`. This was a prebuilt local archive from the earlier validated Windows build, not rebuilt during this diagnostic run.
- The archive's earlier UCRT64 build flags are recorded in `D:\Dev\pepepow-miner-validation\20261124-windows11\openlibm-flags-release.txt`; they include `-ffp-contract=off`, `-fno-fast-math`, `-fno-rounding-math`, `-fno-math-errno`, `-fno-builtin`, `-m64`, `-msse2`, and `-mfpmath=sse`. The repository's tracked Windows pure-C OpenLibm portability patch SHA-256 is `f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2`.
- Debug and Release reports and transcripts are retained locally at `D:\Dev\pepepow-cuda-transform-evidence-20261127\debug` and `D:\Dev\pepepow-cuda-transform-evidence-20261127\release`.
- The Debug and Release report SHA-256 is `0e3971a0719f8ec0ccb46223edcb3ca6c97ea94cef9eaa3a62040e4e54bf036`.
- Pinned-Core/OpenLibm CPU transcript SHA-256: `c73773a67325f58658a3dbdbcf065206637de45f4f501bd3ffbcd9811248c13b`.
- CUDA transcript SHA-256: `5d9f6811da7fd166dd95466a15e58dd3615ae9fc3fcc4fb50765abea356caa74`.
- Input binary SHA-256: `fbdbc09d8215338a0acd48acbc492d4c289a319400ea66281246a40ecd250013`.
- Input catalog SHA-256: `d1de16c97b8091905618e10610c02905f415d4710bc25950f116ab9964e8855b`.
- Matrix transcript SHA-256 for the 32-seed matrix corpus: `cc127d05b1fddb65b4b5181943c6906bed0a448407d8853bde0d91ed2826dfff`.

These are short diagnostic runs. They did not connect to a mining pool, mine, submit shares, run a long GPU load, alter hardware settings, or establish a usable GPU miner.
