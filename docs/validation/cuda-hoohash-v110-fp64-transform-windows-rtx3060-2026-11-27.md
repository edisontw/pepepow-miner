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
| Debug | Passed | 6/6 passed, 3.50 s |
| Release | Passed | 6/6 passed, 3.32 s |

The six tests cover the CUDA BLAKE3 stage, pinned-Core matrix oracle, CUDA matrix stage, this transform diagnostic, device-probe JSON contract, and no-device skip policy. This establishes that the harness and diagnostic binaries ran successfully; it does not turn numeric mismatches into passes.

## Physical-device observations

- GPU status was `live`; device error was `null`.
- The selected Core branch matched for all 29,224 inputs: zero branch mismatches.
- NaN/Infinity classification matched for all compared values.
- `factor_one`, `factor_two`, and `transform_factor`: zero bitwise mismatches.
- `complex_argument`: zero bitwise mismatches, so the selected branch arithmetic produced the same binary64 argument.
- Direct `sin(value)`: 4,214 mismatches, at most 1 ULP; direct `cos(value)`: 4,872 mismatches, at most 1 ULP.
- Direct `sin(value) + cos(value)`: 4,296 mismatches, maximum absolute difference `2.220446049250313e-16`. Its maximum ULP count is 8,192 near cancellation around zero.
- Both pinned OpenLibm `sincos` and CUDA `sincos` produced bitwise-identical sine, cosine, sum, and `exp(sin + cos)` outputs to their respective separate `sin`/`cos` paths for all 29,224 inputs. Against paired Core/OpenLibm results, CUDA had 4,214 sine mismatches, 4,872 cosine mismatches, 4,296 sum mismatches, and 5,605 `exp(sin + cos)` mismatches (up to 2 ULP). The combined APIs did not resolve bitwise parity.
- A controlled `exp` comparison passed the exact same 64-bit OpenLibm `sincos` sum to CPU and CUDA. CUDA `exp` differed in 2,433/29,224 cases, at most 1 ULP, with maximum absolute error `8.881784197001252e-16`; this isolates an `exp` mismatch independent of trigonometric input differences.
- A separate diagnostic-only CUDA translation of pinned OpenLibm `__ieee754_exp` (`src/e_exp.c`, Git blob `639b9fe18dbae273d3182146bb9aebbd59c9e362`) received those same 29,224 CPU/OpenLibm `sincos` sums and matched the pinned OpenLibm result bit-for-bit in all 29,224 cases, including the corpus's special values. The translation retains the source copyright and permission notice. This isolates the native CUDA `exp` difference; it does not resolve the independent `sin`/`cos` mismatches or enter any production hash path.
- The first controlled `exp` mismatch is matrix row 0, column 6 for original input bits `43129660f32cd3f0`. Both implementations receive sum bits `3ff2fa83b829b9ec`; pinned OpenLibm returns `400a32257b456036` and CUDA returns `400a32257b456037` (1 ULP).
- Direct intermediate sine: 4,214 mismatches, at most 1 ULP; `fabs(value) + 1` and its square root matched for finite inputs.
- `medium_complex_nonlinear`: 5,605 bitwise mismatches; maximum finite difference 2 ULP.
- `intermediate_complex_nonlinear`: 4,214 bitwise mismatches; maximum finite difference 3 ULP.
- `complex_nonlinear`: 3,094 bitwise mismatches; maximum finite difference 3 ULP.
- `safe_complex_transform`: 3,091 bitwise mismatches; maximum finite difference 3 ULP.
- `high_complex_nonlinear`: one bitwise mismatch. Both results are NaNs with identical payload bits and opposite sign bits for the negative-payload-NaN input.
- The largest finite absolute difference was `1.7763568394002505e-15`; the largest finite relative difference was `6.56561143256613e-16`.

NVIDIA documents finite precision error bounds for CUDA device math, including double-precision trigonometric functions; it does not promise bitwise identity with OpenLibm. The official guide reports up to 2 ULP for double `sin` and `cos` and describes `sincos` as a paired operation. The measurements above therefore remain a correctness failure for this project even when each primitive is within the documented device-math bounds: [CUDA math-function accuracy](https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/mathematical-functions.html) and [CUDA 13.3.1 `__nv_sincos`](https://docs.nvidia.com/cuda/archive/13.3.1/libdevice-users-guide/__nv_sincos.html).

The first finite `medium_complex_nonlinear` mismatch is reproducible from this 80-byte header with nonce bytes 76–79 set to little-endian zero:

```text
0040002003020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c5b5d507845ead90ea4c4caeef4baef7817c0fb2169d20739883e22bb6d8cf8ef0be1697171e3021d00000000
```

The pinned Core matrix value at row 0, column 6 is `43129660f32cd3f0` (binary64 bits). At that input the direct sine matches, while Core/OpenLibm cosine is `3fcaa112da4b9464` and CUDA cosine is `3fcaa112da4b9465`. This changes the `sin + cos` result from `3ff2fa83b829b9ec` to `3ff2fa83b829b9ed`, after which Core/OpenLibm returns `400a32257b456036` and CUDA returns `400a32257b456038`, a 2-ULP difference. Both implementations select branch 4 and use the same branch argument. This stage produces no full HooHash digest; no CPU/GPU digest mismatch is claimed.

On the same header, the earliest direct cosine mismatch is matrix row 0, column 0: input bits `432955e1472d5ee5`, Core/OpenLibm bits `3fdb47337b59ac85`, CUDA bits `3fdb47337b59ac86`. The earliest direct sine mismatch is row 0, column 5: input bits `4350e9985e566702`, Core/OpenLibm bits `3fde142219d9cc20`, CUDA bits `3fde142219d9cc21`.

The observed finite differences are small but still fail the required bitwise comparison. They must not be accepted with an epsilon. Full HooHash V110 digest parity, the 10,000-case GPU digest suite, candidate CPU recheck, hashrate measurements, and G4 acceptance remain untested and blocked on resolving this transform mismatch.

## Provenance and retained artifacts

- PEPEPOW Core source: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`.
- OpenLibm source: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`.
- OpenLibm archive SHA-256: `9fb7b4caf1deb2136314508c3116adafd8a192a67d0ca4a95e22744fa0ca5f96`. This was a prebuilt local archive from the earlier validated Windows build, not rebuilt during this diagnostic run.
- The archive's earlier UCRT64 build flags are recorded in `D:\Dev\pepepow-miner-validation\20261124-windows11\openlibm-flags-release.txt`; they include `-ffp-contract=off`, `-fno-fast-math`, `-fno-rounding-math`, `-fno-math-errno`, `-fno-builtin`, `-m64`, `-msse2`, and `-mfpmath=sse`. The repository's tracked Windows pure-C OpenLibm portability patch SHA-256 is `f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2`.
- Debug and Release reports and transcripts are retained locally under `D:\Dev\pepepow-cuda-transform-evidence-20261127\debug\exp-port` and `D:\Dev\pepepow-cuda-transform-evidence-20261127\release\exp-port`.
- Debug/Release build logs are `build-debug-openlibm-exp-port.log` and `build-release-openlibm-exp-port.log`; physical-device CTest logs are `ctest-debug-openlibm-exp-port.log` and `ctest-release-openlibm-exp-port.log` in `D:\Dev\pepepow-cuda-transform-evidence-20261127`.
- Debug and Release diagnostic report SHA-256: `ffed92808d4ccf2185af99adf45df969a7877093fa56b28d4e18f0e78b95249b`.
- Pinned-Core/OpenLibm CPU transcript SHA-256: `f4c3d991d81619c7406b6a8542cf69b89066367edb64f313399183bc4eb50864`.
- CUDA transcript SHA-256: `e76f072b08596e23abe94140726b6783be5361c0a5626927359306879a37acd5`.
- Both OpenLibm-port diagnostic fields matched across all 29,224 records; the report's overall FP64 parity remains false because other transform fields still differ.
- CPU-oracle input binary SHA-256: `fbdbc09d8215338a0acd48acbc492d4c289a319400ea66281246a40ecd250013`.
- CUDA input binary, including the reference `exp` operand, SHA-256: `cae879e50f03d7e983a87c9ba2ae7e0c95097ba9752699c6117274befc76c695`.
- Input catalog SHA-256: `4fc1259d8a48dc4fb7ff0ed64c4485e84f29dabbf50cbc22f1f41f4f3f89cde7`.
- Matrix transcript SHA-256 for the 32-seed matrix corpus: `cc127d05b1fddb65b4b5181943c6906bed0a448407d8853bde0d91ed2826dfff`.

These are short diagnostic runs. They did not connect to a mining pool, mine, submit shares, run a long GPU load, alter hardware settings, or establish a usable GPU miner.
