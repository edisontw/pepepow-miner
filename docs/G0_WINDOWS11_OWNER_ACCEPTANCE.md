# Windows 11 x64 G0 owner acceptance

This procedure is for the Owner's native Windows 11 x64 machine at `D:\Dev\pepepow-miner`. It is separate from Windows Server 2022 GitHub Actions evidence. Record the current provenance/legal review status separately; this test does not resolve it. No pool connection or share submission is used.

Prerequisites: Git for Windows, Python 3.12 x64, CMake, Ninja, and MSYS2 UCRT64 with `gcc`, `g++`, `ar`, `nm`, and `objdump`. No wallet credentials, pool endpoint, or live share is used.

## Pin the checkout and dependencies

Open PowerShell:

```powershell
$ErrorActionPreference = 'Stop'
$Repo = 'D:\Dev\pepepow-miner'
git -C $Repo fetch origin main
git -C $Repo checkout --detach origin/main
if ((git -C $Repo status --porcelain).Length -ne 0) { throw 'Miner checkout is not clean' }
$MinerSha = (git -C $Repo rev-parse HEAD).Trim()
$Evidence = Join-Path $Repo "g0-windows11-$MinerSha"
$Deps = Join-Path $env:TEMP "pepepow-g0-deps-$MinerSha"
New-Item -ItemType Directory -Force $Evidence, $Deps | Out-Null

git clone --no-checkout https://github.com/edisontw/PePe-core.git (Join-Path $Deps 'core')
git -C (Join-Path $Deps 'core') checkout --detach bc08108ceccd957c79040dc7fedc1d56d8d47f97
git clone --no-checkout https://github.com/JuliaMath/openlibm.git (Join-Path $Deps 'openlibm')
git -C (Join-Path $Deps 'openlibm') checkout --detach 9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d

$Core = Join-Path $Deps 'core'
$OpenLibm = Join-Path $Deps 'openlibm'
$Patch = Join-Path $Repo '.github\patches\openlibm-portable-c-windows.patch'
if ((git -C $Core rev-parse HEAD).Trim() -ne 'bc08108ceccd957c79040dc7fedc1d56d8d47f97') { throw 'Core pin mismatch' }
if ((git -C $OpenLibm rev-parse HEAD).Trim() -ne '9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d') { throw 'OpenLibm pin mismatch' }
if ((git -C $OpenLibm status --porcelain).Length -ne 0) { throw 'OpenLibm must be clean before patching' }
Get-FileHash $Patch -Algorithm SHA256 | Format-List | Out-File (Join-Path $Evidence 'patch-sha256.txt')
git -C $OpenLibm apply --check $Patch
git -C $OpenLibm apply $Patch
if ((git -C $OpenLibm diff --name-only) -ne 'CMakeLists.txt') { throw 'Unexpected patched paths' }

$Ucrt = 'C:\msys64\ucrt64\bin'
$env:PATH = "$Ucrt;$env:PATH"
$Python = (Get-Command python.exe).Source
gcc --version | Out-File (Join-Path $Evidence 'toolchain.txt')
gcc -dumpmachine | Out-File (Join-Path $Evidence 'target.txt') -Append
cmake --version | Out-File (Join-Path $Evidence 'toolchain.txt') -Append
ninja --version | Out-File (Join-Path $Evidence 'toolchain.txt') -Append
& $Python --version | Out-File (Join-Path $Evidence 'toolchain.txt') -Append
if ((gcc -dumpmachine).Trim() -ne 'x86_64-w64-mingw32') { throw 'Expected native Windows x64 UCRT64 target' }
& $Python -c "import platform,sys; assert sys.platform == 'win32' and platform.machine().lower() in ('amd64','x86_64'); print(sys.version, platform.machine())" | Out-File (Join-Path $Evidence 'toolchain.txt') -Append
if ((git -C $Core status --porcelain).Length -ne 0) { throw 'Core checkout is not clean' }
```

## Build Debug and Release, then compare full digests

Run this for both configurations. Keep the evidence directory; do not overwrite results from an earlier SHA.

```powershell
$ExplorerCheck = @'
import json, pathlib, subprocess, sys
repo, kat = pathlib.Path(sys.argv[1]), sys.argv[2]
path = repo / 'tests/vectors/candidates/explorer-chain-4371217-4371226.jsonl'
rows = [json.loads(x) for x in path.read_text(encoding='utf-8').splitlines() if x.strip()]
assert len(rows) == 10
for row in rows:
    digest = row['expected_core_function_output_hex']
    result = subprocess.run([kat, row['header_hex'], digest], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0 and 'actual=' + digest in result.stdout.splitlines(), (row['case_id'], result.stdout, result.stderr)
print('PASS: 10 Explorer headers, complete 32-byte digests')
'@
$ExplorerScript = Join-Path $Evidence 'check-explorer.py'
Set-Content -Path $ExplorerScript -Value $ExplorerCheck -Encoding ascii

foreach ($Config in @('Debug', 'Release')) {
    $Build = Join-Path $Evidence $Config
    $OpenBuild = Join-Path $Build 'openlibm'
    $MinerBuild = Join-Path $Build 'miner'
    cmake -S $OpenLibm -B $OpenBuild -G Ninja -DCMAKE_BUILD_TYPE=$Config -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_SHARED_LIBS=OFF -DOPENLIBM_USE_ASM=OFF
    if ($LASTEXITCODE -ne 0) { throw "OpenLibm configure failed: $Config" }
    cmake --build $OpenBuild --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "OpenLibm build failed: $Config" }
    cmake -S $Repo -B $MinerBuild -G Ninja -DCMAKE_BUILD_TYPE=$Config -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DHARNESS_CONTRACT_TESTS_ONLY=OFF "-DOPENLIBM_INCLUDE_DIR=$OpenLibm\include" "-DOPENLIBM_LIBRARY=$OpenBuild\libopenlibm.a" "-DPEPEPOW_CORE_SOURCE_DIR=$Core" -DPEPEPOW_CORE_EXPECTED_SHA=bc08108ceccd957c79040dc7fedc1d56d8d47f97
    if ($LASTEXITCODE -ne 0) { throw "Miner configure failed: $Config" }
    cmake --build $MinerBuild --target pepepow-kat --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "KAT build failed: $Config" }

    $Kat = Join-Path $MinerBuild 'pepepow-kat.exe'
    if (!(Test-Path $Kat)) { throw "Missing native KAT: $Config" }
    & $Python -m unittest discover -s (Join-Path $Repo 'tests') -p test_g0_v110_independent.py -v
    if ($LASTEXITCODE -ne 0) { throw "Independent V110 regression failed: $Config" }
    & $Python $ExplorerScript $Repo $Kat
    if ($LASTEXITCODE -ne 0) { throw "Explorer chain header check failed: $Config" }

    $Headers = Join-Path $Build 'headers.tsv'
    $Independent = Join-Path $Build 'independent.tsv'
    $CoreDigests = Join-Path $Build 'core.tsv'
    & $Python (Join-Path $Repo 'tests\generate_g0_headers.py') --count 10000 | Set-Content -Path $Headers -Encoding ascii
    if ($LASTEXITCODE -ne 0 -or (Get-Content $Headers).Count -ne 10000) { throw "Header corpus generation failed: $Config" }
    & $Python (Join-Path $Repo 'tests\export_independent_v110_digests.py') $Headers $Independent --count 10000
    if ($LASTEXITCODE -ne 0) { throw "Independent digest export failed: $Config" }
    & $Python (Join-Path $Repo 'tests\export_core_reference_digests.py') --kat $Kat --corpus $Headers --output $CoreDigests --limit 10000
    if ($LASTEXITCODE -ne 0) { throw "Core digest export failed: $Config" }
    & $Python (Join-Path $Repo 'tests\compare_windows_digest_parity.py') $CoreDigests $Independent --count 10000 | Tee-Object -FilePath (Join-Path $Build 'comparison.txt')
    if ($LASTEXITCODE -ne 0) { throw "Full-digest parity failed: $Config" }

    $Commands = @((Get-Content (Join-Path $OpenBuild 'compile_commands.json') -Raw | ConvertFrom-Json), (Get-Content (Join-Path $MinerBuild 'compile_commands.json') -Raw | ConvertFrom-Json))
    $AllCommands = $Commands | ForEach-Object { $_.command }
    $CoreCommand = $AllCommands | Where-Object { $_ -match 'hoohash\.c' } | Select-Object -First 1
    $OpenCommand = $AllCommands | Where-Object { $_ -match 'src[/\\]e_exp\.c' } | Select-Object -First 1
    if (!$CoreCommand -or !$OpenCommand) { throw "Missing native Core/OpenLibm compile command: $Config" }
    foreach ($Command in @($CoreCommand, $OpenCommand)) {
        if ($Command -notmatch '-fno-fast-math' -or $Command -notmatch '-ffp-contract=off' -or $Command -match '-ffast-math') { throw "FP flags mismatch: $Command" }
    }
    if ($OpenCommand -notmatch '-fno-rounding-math') { throw 'OpenLibm rounding flag missing' }

    Get-FileHash $Headers, $Independent, $CoreDigests, $Kat -Algorithm SHA256 | Format-Table -AutoSize | Out-File (Join-Path $Build 'sha256.txt')
}
```

The final-byte negative control must reject a poisoned independent digest file:

```powershell
$PoisonScript = Join-Path $Evidence 'poison-last-byte.py'
@'
from pathlib import Path
import sys
rows = Path(sys.argv[1]).read_text(encoding='ascii').splitlines()
assert len(rows) == 10000
case, header, digest = rows[-1].split('\t')
assert case == 'g0-synthetic-09999' and len(digest) == 64
rows[-1] = '\t'.join((case, header, digest[:-2] + ('00' if digest[-2:] != '00' else '01')))
Path(sys.argv[2]).write_text('\n'.join(rows) + '\n', encoding='ascii', newline='\n')
'@ | Set-Content -Path $PoisonScript -Encoding ascii
foreach ($Config in @('Debug', 'Release')) {
    $Build = Join-Path $Evidence $Config
    $Poisoned = Join-Path $Build 'poisoned.tsv'
    & $Python $PoisonScript (Join-Path $Build 'independent.tsv') $Poisoned
    if ($LASTEXITCODE -ne 0) { throw "Failed to create negative control: $Config" }
    & $Python (Join-Path $Repo 'tests\compare_windows_digest_parity.py') (Join-Path $Build 'core.tsv') $Poisoned --count 10000
    if ($LASTEXITCODE -ne 1) { throw "Comparator did not reject last-byte corruption: $Config" }
}

Get-FileHash (Join-Path $Repo '.github\patches\openlibm-portable-c-windows.patch') -Algorithm SHA256 | Out-File (Join-Path $Evidence 'patch-sha256-final.txt')
Get-ChildItem $Evidence -Recurse -File | Where-Object Extension -ne '.py' | Get-FileHash -Algorithm SHA256 | Export-Csv -NoTypeInformation (Join-Path $Evidence 'evidence-checksums.csv')
```

## PASS standard

Accept the Windows 11 owner run only if both Debug and Release use the exact current main SHA, pinned Core and OpenLibm SHAs above, a clean source checkout, UCRT64 `x86_64-w64-mingw32`, FP64/binary64, round-to-nearest, `-fno-fast-math`, `-ffp-contract=off`, and OpenLibm `-fno-rounding-math`; each configuration must build and execute the real Core KAT, compare 10,000/10,000 complete 32-byte digests with zero mismatches, pass all 10 Explorer headers, and reject the poisoned last-byte corpus with exit 1. Preserve the logs and SHA-256 file. Record this as Windows 11 owner evidence, separate from Windows Server CI. Do not close Issue #1 or mark formal G0/G1 PASS from this procedure alone.
