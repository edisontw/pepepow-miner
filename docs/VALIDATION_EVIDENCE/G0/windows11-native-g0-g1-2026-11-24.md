# Windows 11 native G0/G1 technical evidence

Date: 2026-11-24 (Asia/Taipei)

## Result and scope

Native Windows 11 x64 Debug and Release builds completed the technical G0 parity workflow and the offline G1 test set. This evidence is from the PR #22 worktree based on the fetched head 445ae7a3ce705affff93673c75aac5e4e16ef608. The HooHash and miner implementation sources were unchanged by the follow-up test, CI, and documentation edits.

This is technical evidence only. G0 remains NOT PASSED pending per-file license/NOTICE and provenance decisions and Owner sign-off. G1 remains OFFLINE PROTOTYPE ONLY; NOT PASSED. All accepted/rejected replies below came from local mock servers. No production pool was contacted and no real share was submitted.

## Host and pinned inputs

- Host: Microsoft Windows 11 Pro x64, version 10.0.26300, build 26300; native PowerShell, not WSL or a Windows Server runner.
- CPU: AMD Ryzen 5 7600X, 6 cores / 12 logical processors.
- RAM: 33,519,362,048 bytes (about 31.2 GiB). D: had 430,496,985,088 bytes free at inspection.
- Toolchain: MSYS2 UCRT64, GCC/G++ 16.2.0-7, CMake 4.5.1-1, Ninja 1.13.2-1, OpenSSL 4.1.0-1; native Windows CPython 3.13.17. No Zig toolchain was used.
- PEPEPOW Core checkout: bc08108ceccd957c79040dc7fedc1d56d8d47f97.
- OpenLibm v0.8.7 checkout: 9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d.
- Windows portable-C OpenLibm patch SHA-256: f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2.
- The independent OpenLibm build used separate Debug and Release directories, disabled ASM detection, and compiled 254 C translation units. Archive SHA-256: Debug 47CF1D3F452B6CD4DA0791161F1F4784A343C269E3DF90FC03CF49575F5C30F8; Release 9FB7B4CAF1DEB2136314508C3116ADAFD8A192A67D0CA4A95E22744FA0CA5F96.
- CMake configuration used HARNESS_CONTRACT_TESTS_ONLY=OFF, PEPEPOW_NATIVE_STRATUM_SEARCH=ON, the pinned Core source and expected SHA, the matching OpenLibm headers/archive, native UCRT64 OpenSSL Crypto, and native Windows Python.
- Core HooHash used -fno-fast-math and -ffp-contract=off. OpenLibm additionally used -fno-rounding-math. No -ffast-math was present. The FP probe in each configuration reported FLT_ROUNDS=1, FE_TONEAREST=0, fegetround=0, DBL_MANT_DIG=53, sizeof(double)=8; fegetround equaled FE_TONEAREST.

## G0 Debug and Release results

Each configuration passed the six independent Python V110 unit tests and all native comparisons:

| Check | Debug | Release |
| --- | ---: | ---: |
| Explorer-derived 80-byte headers with complete 32-byte KAT | 10/10 | 10/10 |
| Deterministic independent Python/Core parity | 10,000/10,000 | 10,000/10,000 |
| Empty corpus | Rejected, exit 1, no output | Rejected, exit 1, no output |
| Poisoned final digest byte | Rejected at row 10,000 | Rejected at row 10,000 |

The Windows CRLF header corpus SHA-256 was 967466e658872f27c3d30996bf19fc433b06a2bde5fed9c4edc7489134ce224c. Both independent and Core digest TSVs had SHA-256 73007ec4247215653c54a665dfa8c96f98a08d18593bf36e2d81bfc895c58afb. The initial checksum discrepancy was line-ending normalization; rerunning with the generated raw Windows CRLF bytes restored the expected checksums. No vectors or digest content were changed.

## G1, sockets, and Release test assertions

- Root CTest: Debug 2/2 and Release 2/2, including native_stratum_search and offline_stratum_miner_loopback.
- Dedicated job-state suite: Debug 22/22 and Release 22/22, including the Winsock loopback test.
- End-to-end loopback tests passed in both configurations: job replacement/stale cancellation, complete header and digest, mock accepted/rejected counts, bounded 1 ms cooperative timeout (exit 4), invalid CLI rejection, and bounded connection faults.
- Fault cases: connection refused, subscribe reply timeout, midstream close, truncated subscribe JSON, invalid subscribe reply, rejected authorize, invalid authorize response shape, and missing submit reply. The last case verified zero accepted/rejected accounting and socket closure. Every child process exited within its bound and was reaped.
- Offline replay passed captured pool-wire difficulty, first nonce 2, and full native KAT. Offline CPU search passed 15 CLI cases and 8 frozen full-digest checks, including nonce 4294967295.
- All mock listeners bound only to 127.0.0.1 on OS-assigned ports. These replies are not pool-accepted shares.
- Release job-state tests retain -Werror and active assertions via -UNDEBUG. GCC 16 emitted a false optional-analysis warning for the job_state test at -O3, so only that GNU Release test target receives a final -O2 option; production miner Release settings are unchanged.
- Winsock is linked by WIN32 platform condition. The test executable imports WS2_32.dll.
- The first static OpenSSL archive link exposed an unresolved BCryptGenRandom symbol. Using the matching UCRT64 OpenSSL 4.1.0 DLL import library and its DLL resolved the native link and runtime.
- Native executables were launched from ordinary Windows PowerShell with MSYS2 absent from PATH. The EXE directories included the matching UCRT64 runtime sidecars; they are validation outputs, not standalone release packages.

## Executable SHA-256

All paths below are local validation outputs and were not committed or published.

| Configuration | Executable | SHA-256 |
| --- | --- | --- |
| Debug | offline_stratum_miner.exe | 919DC8FD9D62F8366A38FDF656343EF558043DC7E8BFF47D819AFC05C989F39B |
| Debug | offline_stratum_replay.exe | CC8F54DEE10A84C69F4232B50AFCBB62E37C3435A1676F55DE02F17411EC8197 |
| Debug | pepepow-kat.exe | A69C2DD6BB932AF6E6E68C4E4095B88AD7468D4878E0C538AC28E78973E3E96C |
| Debug | pepepow-offline-search.exe | 07B63A69E8D8BF893934DDEDABDB2757F8C9F34C42C21BBB55939C4151B12B0E |
| Debug | test_native_stratum_search.exe | 34B458EAB15C4605225252CE8A63668C4BE6EEEC0468D6E3D635DC340E38BA0E |
| Release | offline_stratum_miner.exe | DE4F5F16BF49F5B60C441C1E2DA729EDC9F1FA4E7EE6E81F312A357C1CEB91BD |
| Release | offline_stratum_replay.exe | 635895CF336ACD7C06876DBB80C49F826846A7419F169849F57436640A7BBC31 |
| Release | pepepow-kat.exe | 5A7F3A17C196802EB4C9E6190645333229E5B66E8E2D383C03DDE11718717B6D |
| Release | pepepow-offline-search.exe | 3FCD34264313839CEB8AA25B4E00E96791D046620995BBFC4B1BB97D188B2616 |
| Release | test_native_stratum_search.exe | 9DACF2F6538DB523348AF0E765165CBE8338B617CAA54F63DD369BF63B39C8B1 |

Absolute executable directories:

- Debug: D:\Dev\pepepow-miner-validation\20261124-windows11\build\miner-debug-ucrt
- Release: D:\Dev\pepepow-miner-validation\20261124-windows11\build\miner-release-ucrt

Runtime sidecars beside each configuration's executables:

| DLL | SHA-256 |
| --- | --- |
| libcrypto-4-x64.dll | FF5BD426835ACF70A03E6FD8BB51B60441806BF228DE608CAF4355B57F0C2340 |
| libgcc_s_seh-1.dll | 44F6318F96EB2AAF6E90400BB1178AB6D55B01AE2CF24846D6B30D96E87E89DD |
| libstdc++-6.dll | 098E4964E3C8179847F7A1C313D2E8020CA12DF3F1D18878A417A8F54D41505A |
| libwinpthread-1.dll | 94A9A4FE27B4BC827A4B209E9DEB4CC3DC34BC025DF06B849144F9D4C588F8 |

The executables also depend on Windows kernel/UCRT API-set DLLs and WS2_32.dll. Exact sizes, absolute file paths, and checksums for all EXEs and sidecars are in the local native-binary-checksums.csv.

## Retained local logs

The full local build and test records are in D:\Dev\pepepow-miner-validation\20261124-windows11, outside the repository. Key files include g0-debug-parity-crlf.log, g0-release-parity-crlf.log, ctest-miner-debug.log, ctest-miner-release.log, job-state-debug-ctest.log, job-state-release-ctest.log, and the g1-e2e/replay/search Debug and Release logs. The directory also contains separate CMake build trees and compile_commands.json files.

## Acceptance boundary

The native Windows 11 technical G0 procedure passed on both configurations. Formal G0 remains NOT PASSED until the outstanding per-file license/NOTICE and provenance questions are resolved and the Owner records sign-off. G1 remains OFFLINE PROTOTYPE ONLY; NOT PASSED pending the G0 prerequisite, broader authorized protocol interoperability, reconnect/failover coverage, and real authorized platform evidence. No production pool, real share, public binary, GPU, or GUI work was part of this run.
