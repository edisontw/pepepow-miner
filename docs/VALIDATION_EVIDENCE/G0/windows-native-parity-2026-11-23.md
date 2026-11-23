# Windows native G0 parity evidence

**Result:** Windows Server 2022 x64 native CI passed in Debug and Release. This is platform CI evidence, not Windows 10/11 device acceptance and not formal G0 sign-off.

## Exact-head runs

| Run | Commit | Result | Evidence |
| --- | --- | --- | --- |
| PR #19 exact head | `fca225035b40bc8bfd3b4e5a08095317bbded7f0` | Debug and Release succeeded | [GitHub Actions run 45179968956](https://github.com/edisontw/pepepow-miner/actions/runs/45179968956) |
| Post-merge main | `565d7002678905acdaa4eb21c82b1c7fb60dc4dc` | Debug and Release succeeded; every step in both jobs succeeded | [GitHub Actions run 45181410916](https://github.com/edisontw/pepepow-miner/actions/runs/45181410916) |

The workflow definition at the tested code baseline is [.github/workflows/g0-windows-native-parity.yml](https://github.com/edisontw/pepepow-miner/blob/565d7002678905acdaa4eb21c82b1c7fb60dc4dc/.github/workflows/g0-windows-native-parity.yml). It records the full CMake configuration, build commands, Python commands, per-step timeouts, artifact collection, and evidence boundary.

## Pinned inputs and native toolchain

- Miner code baseline: `565d7002678905acdaa4eb21c82b1c7fb60dc4dc`; checkout clean.
- PEPEPOW Core: `bc08108ceccd957c79040dc7fedc1d56d8d47f97`; checkout clean.
- OpenLibm v0.8.7: `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`; checkout clean.
- Windows-only portable-C OpenLibm patch: SHA-256 `f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2`. Upstream sources remain pinned; CMake assembly detection is disabled and no system libm fallback is used.
- Runner: GitHub-hosted Windows Server 2022 x64, Windows version `10.0.20348`, MSYS2 UCRT64; target `x86_64-w64-mingw32`.
- GCC/G++ `16.2.0`; CMake `4.5.1`; Ninja `1.13.2`; native Windows CPython `3.12.10` AMD64.
- CMake built OpenLibm as a static library with `OPENLIBM_USE_ASM=OFF`, then built and executed the native `pepepow-kat.exe` linked to the pinned Core and OpenLibm.
- Core and OpenLibm compile commands were checked for `-fno-fast-math` and `-ffp-contract=off`; OpenLibm also used `-fno-rounding-math`. No `-ffast-math` was present. The native probe reported `FLT_ROUNDS=1`, `FE_TONEAREST=0`, `fegetround=0`, `DBL_MANT_DIG=53`, and `sizeof(double)=8`.

## Results per configuration

| Check | Debug | Release |
| --- | ---: | ---: |
| Independent Python V110 vs pinned native Core, full 32-byte digest | 10,000/10,000 PASS | 10,000/10,000 PASS |
| Explorer-derived 80-byte headers, full expected digest | 10/10 PASS | 10/10 PASS |
| Empty corpus | Rejected with exit 1; no output file | Rejected with exit 1; no output file |
| Last digest byte corrupted | Rejected with exit 1 at row 10,000 | Rejected with exit 1 at row 10,000 |
| Workflow job steps | 20/20 successful | 20/20 successful |

The 10,000-case corpus is deterministic and identical across configurations. Header TSV SHA-256: `967466e658872f27c3d30996bf19fc433b06a2bde5fed9c4edc7489134ce224c`. Core and independent Python digest TSV SHA-256 (each): `73007ec4247215653c54a665dfa8c96f98a08d18593bf36e2d81bfc895c58afb`.

The negative control changed only the last byte of the final candidate digest. The comparator reported row 10,000, case `g0-synthetic-09999`, and returned nonzero. The negative fixture is synthetic by design; it tests comparator failure behavior and is not counted as a consensus hash vector.

## Archived run artifacts

Artifacts can be downloaded from each run's GitHub Actions page. SHA-256 below is the archive digest reported by GitHub Actions.

| Run / configuration | Artifact ID | Archive SHA-256 |
| --- | ---: | --- |
| Main run / Release | 14821916780 | `b48e8d643a592733854aaa0d8abefb24e01c07230827bf23e968b29b031238fd` |
| Main run / Debug | 14820563640 | `6796680bfcc0ea47408b9e2385b06a1c5a23d15a229b3db2f124838f4d80e249` |
| PR-head run / Release | 14820547167 | `5fb5d17a412554d1019aed52ab21b36ec585e583c3a7974c9d752bba8713bb85` |
| PR-head run / Debug | 14818814580 | `ea2c50da98d2ddec067007c0b84f55041fc8f3f4f219a4f6494c54c1f6144553` |

Each artifact contains the generated corpus, Core and Python digest TSVs, comparator output, source/toolchain/FP evidence, and build records. The main-run artifacts expire 2026-12-23.

## Scope and remaining gates

This is native execution on a GitHub-hosted Windows Server 2022 runner. It must not be described as Windows 10 or Windows 11 device validation. The recorded Windows 11 owner result remains partial: Release build, frozen full-digest KAT, and an eight-nonce offline search passed on 2026-11-22; the full native parity matrix and formal checklist sign-off are not established by that result.

The CI demonstrates parity for these pinned inputs and the tested corpus. It does not complete the per-file source provenance, license/NOTICE, or redistribution review. [Issue #1](https://github.com/edisontw/pepepow-miner/issues/1) remains open, and G0/G1 remain **NOT PASSED**.
