# Windows 10/11 Physical Host Acceptance Runbook

This runbook prepares technical acceptance evidence on a physical Windows 10 or Windows 11 x64 computer. Hosted Windows Server CI and cloud Linux builds do not replace this run. The script uses the exact `origin/main` commit fetched at the start, builds it in a separate Git worktree, and writes its evidence outside the repository checkout.

This is a local-only validation. Tests use the repository's loopback mock servers and pinned fixtures. The script does not contact a mining pool, submit a real share, use a wallet, install packages, publish binaries, or upload evidence. It clones only the pinned public Core and OpenLibm source repositories.

## Host requirements

- A physical Windows 10 or Windows 11 x64 computer. Do not run the procedure in WSL, a VM, or Windows Server.
- Git for Windows.
- Native Windows x64 CPython 3.12 or 3.13. The report records the exact interpreter version.
- MSYS2 UCRT64 with GCC/G++, CMake, Ninja, GNU `ar`, `nm`, `objdump`, and OpenSSL Crypto.
- Enough free space for two Debug/Release build trees and two pinned source checkouts. Allow several hours for both 10,000-case parity runs and all tests.
- Network access to GitHub for `origin/main`, the two pinned source repositories, and their Git metadata. Test executables only use loopback sockets.

Install the UCRT64 tools from an MSYS2 UCRT64 terminal if they are not present:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-openssl
```

Install Git for Windows and x64 CPython separately. Do not run PowerShell as Administrator unless the host requires it for unrelated setup; the validation itself should run as a normal user.

## Run from Codex Local on the Windows host

Open the repository in Codex Local on the physical Windows computer. In its PowerShell terminal, run:

```powershell
$repo = (Get-Location).Path
& "$repo\tools\windows\Invoke-Windows10-11NativeAcceptance.ps1" -RepositoryPath $repo
```

To check PowerShell parsing and the pinned patch checksum without building anything, run:

```powershell
& "$repo\tools\windows\Invoke-Windows10-11NativeAcceptance.ps1" -SelfTest
```

The script fetches `origin/main`, records its full SHA, and refuses to continue if the optional `-ExpectedHead` value does not match. To pin a reviewed main SHA explicitly:

```powershell
& "$repo\tools\windows\Invoke-Windows10-11NativeAcceptance.ps1" `
  -RepositoryPath $repo `
  -ExpectedHead '<full-40-character-main-commit-sha>'
```

At the prompt, type `PHYSICAL` only on the physical Windows computer. The script verifies the Windows version, rejects common virtual-machine model strings, records CPU/core/memory information without recording the machine name or serial number, and records the operator's physical-host attestation. It also checks that the compiler target is `x86_64-w64-mingw32` and Python is native Windows x64.

The run does not change the current checkout or its branch. It adds a detached temporary worktree under the evidence directory, then uses separate pinned PEPEPOW Core and OpenLibm checkouts. The default evidence directory is `%LOCALAPPDATA%\PEPEPOW Miner\Validation\windows-physical-<timestamp>`.

## Checks performed

For each of Debug and Release, the script:

- Builds pinned OpenLibm v0.8.7 with the repository's Windows portable-C patch, then builds the pinned PEPEPOW Core HooHash reference and the complete offline miner/test targets using native UCRT64.
- Records the compiler target, actual compile commands, floating-point flags, FP environment probe, linker maps, compiler dependency output, archive members, executable imports, and SHA-256 hashes for linked artifacts. The OpenLibm patch checksum is verified after normalizing its line endings and the raw file checksum is recorded.
- Runs the native miner CTest suite, the independent HooHash V110 Python tests, ten Explorer-derived full-digest vectors, 10,000 complete 32-byte Core/Python comparisons, empty-input rejection, and a negative control that corrupts the final digest byte.
- Runs the localhost miner end-to-end test, loopback Stratum replay, offline CPU search checks, and the dedicated job-state/socket CTest suite.

CTest totals are read from the actual output and saved to `acceptance-report.json`. Each command has a separate log under `logs\`; source/dependency SHAs, machine facts, outcomes, and test counts are stored in the JSON report. The report is marked `PASS` only if all required commands pass. A nonzero result stops the run and leaves a `FAIL` report and partial logs for diagnosis.

## Return the evidence

When the run finishes, provide Codex Local with the printed evidence directory and ask it to review `acceptance-report.json` and the command logs. Keep the full folder until the evidence is reviewed. For separate Windows 10 and Windows 11 evidence, run the same script once on each physical host and retain both output directories.

A technical pass is evidence for that exact commit and that one physical host. It does not resolve the MIT/GPL file-header discrepancy, OpenLibm/BLAKE3/pool-derived NOTICE questions, redistribution obligations, or formal Owner sign-off. It does not establish production-pool interoperability or accepted real shares. Keep Issue #1 open and G0/G1 formal acceptance pending until their other closure conditions and Owner decisions are complete.
