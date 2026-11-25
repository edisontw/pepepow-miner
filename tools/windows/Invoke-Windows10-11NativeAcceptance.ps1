[CmdletBinding()]
param(
    [string]$RepositoryPath = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$Msys2Root = 'C:\msys64',
    [string]$OutputRoot = (Join-Path $env:LOCALAPPDATA 'PEPEPOW Miner\Validation'),
    [string]$ExpectedHead,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

$script:Steps = New-Object 'System.Collections.Generic.List[object]'
$script:TestCounts = New-Object 'System.Collections.Generic.List[object]'
$script:RunDirectory = $null
$script:Report = $null

function Save-RunReport {
    if ($null -eq $script:Report -or [string]::IsNullOrWhiteSpace($script:RunDirectory)) {
        return
    }

    $script:Report.steps = @($script:Steps)
    $script:Report.test_counts = @($script:TestCounts)
    $script:Report.updated_at_utc = [DateTime]::UtcNow.ToString('o')
    $reportPath = Join-Path $script:RunDirectory 'acceptance-report.json'
    $script:Report | ConvertTo-Json -Depth 8 | Set-Content -Path $reportPath -Encoding UTF8
}

function Add-TestCount {
    param(
        [string]$Name,
        [int]$Count,
        [string]$Configuration,
        [string]$Log
    )
    $null = $script:TestCounts.Add([pscustomobject]@{
        name = $Name
        count = $Count
        configuration = $Configuration
        log = $Log
    })
    Save-RunReport
}

function Invoke-Logged {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$ArgumentList,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [string]$LogName,
        [int]$ExpectedExitCode = 0
    )

    if ([string]::IsNullOrWhiteSpace($LogName)) {
        $LogName = $Name
    }
    $logPath = Join-Path $script:RunDirectory "logs\$LogName.log"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logPath) | Out-Null
    $argumentText = ($ArgumentList | ForEach-Object { "'$_'" }) -join ' '
    "COMMAND: `"$FilePath`" $argumentText" | Set-Content -Path $logPath -Encoding UTF8
    "WORKING DIRECTORY: $WorkingDirectory" | Add-Content -Path $logPath -Encoding UTF8
    Write-Host "[$Name] $FilePath $argumentText"

    Push-Location $WorkingDirectory
    try {
        & $FilePath @ArgumentList 2>&1 | Tee-Object -FilePath $logPath -Append | ForEach-Object { Write-Host $_ }
        $exitCode = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }

    $passed = ($exitCode -eq $ExpectedExitCode)
    $null = $script:Steps.Add([pscustomobject]@{
        name = $Name
        executable = $FilePath
        arguments = @($ArgumentList)
        working_directory = $WorkingDirectory
        expected_exit_code = $ExpectedExitCode
        actual_exit_code = $exitCode
        status = $(if ($passed) { 'PASS' } else { 'FAIL' })
        log = $logPath
    })
    Save-RunReport
    if (-not $passed) {
        throw "$Name exited with $exitCode; expected $ExpectedExitCode. See $logPath"
    }
}

function Invoke-CapturedToFile {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$ArgumentList,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [string]$LogName
    )

    if ([string]::IsNullOrWhiteSpace($LogName)) {
        $LogName = $Name
    }
    $logPath = Join-Path $script:RunDirectory "logs\$LogName.log"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logPath) | Out-Null
    $argumentText = ($ArgumentList | ForEach-Object { "'$_'" }) -join ' '
    "COMMAND: `"$FilePath`" $argumentText" | Set-Content -Path $logPath -Encoding UTF8
    "WORKING DIRECTORY: $WorkingDirectory" | Add-Content -Path $logPath -Encoding UTF8
    Write-Host "[$Name] capturing output to $OutputPath"

    Push-Location $WorkingDirectory
    try {
        $captured = @(& $FilePath @ArgumentList 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }

    $lines = @($captured | ForEach-Object { [string]$_ })
    $lines | Set-Content -Path $OutputPath -Encoding ASCII
    $lines | Add-Content -Path $logPath -Encoding UTF8
    $null = $script:Steps.Add([pscustomobject]@{
        name = $Name
        executable = $FilePath
        arguments = @($ArgumentList)
        working_directory = $WorkingDirectory
        expected_exit_code = 0
        actual_exit_code = $exitCode
        status = $(if ($exitCode -eq 0) { 'PASS' } else { 'FAIL' })
        output = $OutputPath
        log = $logPath
    })
    Save-RunReport
    if ($exitCode -ne 0) {
        throw "$Name exited with $exitCode. See $logPath"
    }
}

function Get-GitText {
    param([string]$Path, [string[]]$GitArguments)
    $value = & git -C $Path @GitArguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArguments -join ' ') failed in $Path"
    }
    return (($value | Out-String).Trim())
}

function Get-RequiredCommandPath {
    param([string]$Name)
    $command = Get-Command $Name -ErrorAction Stop
    return $command.Source
}

function Get-CanonicalPatchSha256 {
    param([byte[]]$Bytes)
    $text = [System.Text.Encoding]::UTF8.GetString($Bytes)
    $canonicalLf = [System.Text.RegularExpressions.Regex]::Replace($text, "`r?`n", "`n")
    $canonicalCrlf = $canonicalLf.Replace("`n", "`r`n")
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $canonicalBytes = [System.Text.Encoding]::UTF8.GetBytes($canonicalCrlf)
        return ([BitConverter]::ToString($sha256.ComputeHash($canonicalBytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
}

if ($SelfTest) {
    $patchPath = Join-Path $PSScriptRoot '..\..\.github\patches\openlibm-portable-c-windows.patch'
    $patchBytes = [System.IO.File]::ReadAllBytes((Resolve-Path $patchPath).Path)
    $patchText = [System.Text.Encoding]::UTF8.GetString($patchBytes)
    $lfText = [System.Text.RegularExpressions.Regex]::Replace($patchText, "`r?`n", "`n")
    $variants = @(
        [System.Text.Encoding]::UTF8.GetBytes($lfText),
        [System.Text.Encoding]::UTF8.GetBytes($lfText.Replace("`n", "`r`n"))
    )
    foreach ($variant in $variants) {
        if ((Get-CanonicalPatchSha256 -Bytes $variant) -ne 'f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2') {
            throw 'Canonical OpenLibm patch checksum self-test failed.'
        }
    }
    Write-Host 'PASS: canonical patch checksum is stable for LF and CRLF inputs.'
    return
}

function Invoke-CtestAndRecordCount {
    param(
        [string]$Name,
        [string]$BuildDirectory,
        [string]$Configuration,
        [string]$LogName
    )
    Invoke-Logged -Name $Name -FilePath $script:Ctest -ArgumentList @('--test-dir', $BuildDirectory, '--output-on-failure') -WorkingDirectory $script:RunDirectory -LogName $LogName
    $logPath = Join-Path $script:RunDirectory "logs\$LogName.log"
    $log = Get-Content -Path $logPath -Raw
    if ($log -notmatch 'out of\s+(\d+)') {
        throw "Could not extract CTest test count from $logPath"
    }
    Add-TestCount -Name $Name -Count ([int]$Matches[1]) -Configuration $Configuration -Log $logPath
}

try {
    $repo = (Resolve-Path $RepositoryPath).Path
    $git = Get-RequiredCommandPath 'git.exe'
    $ucrtBin = Join-Path $Msys2Root 'ucrt64\bin'
    if (-not (Test-Path (Join-Path $ucrtBin 'gcc.exe'))) {
        throw "Missing MSYS2 UCRT64 GCC at $(Join-Path $ucrtBin 'gcc.exe')"
    }
    $env:PATH = "$ucrtBin;$env:PATH"
    $script:Gcc = Get-RequiredCommandPath 'gcc.exe'
    $script:Gxx = Get-RequiredCommandPath 'g++.exe'
    $script:Ninja = Get-RequiredCommandPath 'ninja.exe'
    $script:Ar = Get-RequiredCommandPath 'ar.exe'
    $script:Nm = Get-RequiredCommandPath 'nm.exe'
    $script:Objdump = Get-RequiredCommandPath 'objdump.exe'
    $script:Cmake = Get-RequiredCommandPath 'cmake.exe'
    $script:Ctest = Get-RequiredCommandPath 'ctest.exe'
    $script:Python = Get-RequiredCommandPath 'python.exe'
    $openSslRoot = Join-Path $Msys2Root 'ucrt64'
    $openSslImportLibrary = Join-Path $openSslRoot 'lib\libcrypto.dll.a'
    if (-not (Test-Path $openSslImportLibrary)) {
        throw "Missing UCRT64 OpenSSL import library: $openSslImportLibrary"
    }

    $os = Get-CimInstance Win32_OperatingSystem
    if ($os.Caption -notmatch 'Windows (10|11)') {
        throw "This runner is '$($os.Caption)'. Use a physical Windows 10 or 11 host."
    }
    $system = Get-CimInstance Win32_ComputerSystem
    if ($system.Model -match '(?i)virtual|vmware|virtualbox|qemu|kvm') {
        throw "The reported computer model '$($system.Model)' looks virtualized; physical-host acceptance requires a physical machine."
    }
    $attestation = Read-Host 'Confirm this is a physical Windows 10/11 computer, not a VM or WSL. Type PHYSICAL to continue'
    if ($attestation -cne 'PHYSICAL') {
        throw 'Physical-host attestation was not provided.'
    }
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $pythonInfo = & $script:Python -c "import platform,sys; print(sys.version); print(sys.platform); print(platform.machine()); print(platform.architecture()[0])"
    if ($LASTEXITCODE -ne 0 -or (($pythonInfo -join "`n") -notmatch 'win32') -or (($pythonInfo -join "`n") -notmatch '(?i)amd64|x86_64')) {
        throw 'Python must be a native Windows x64 interpreter, not MSYS Python or a 32-bit interpreter.'
    }
    $targetTriple = (& $script:Gcc -dumpmachine | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $targetTriple -ne 'x86_64-w64-mingw32') {
        throw "Expected native UCRT64 target x86_64-w64-mingw32, got '$targetTriple'."
    }

    New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
    $fetchedAt = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $script:RunDirectory = Join-Path $OutputRoot "windows-physical-$fetchedAt"
    New-Item -ItemType Directory -Force -Path $script:RunDirectory | Out-Null
    $script:Report = [ordered]@{
        schema = 'pepepow-windows-physical-acceptance-v1'
        status = 'RUNNING'
        started_at_utc = [DateTime]::UtcNow.ToString('o')
        host = [ordered]@{
            os_caption = $os.Caption
            os_version = $os.Version
            os_build = $os.BuildNumber
            computer_model = $system.Model
            computer_manufacturer = $system.Manufacturer
            hypervisor_present = [bool]$system.HypervisorPresent
            physical_host_attested_by_operator = $true
            cpu_name = $cpu.Name
            physical_cores = $cpu.NumberOfCores
            logical_processors = $cpu.NumberOfLogicalProcessors
            total_physical_memory_bytes = $system.TotalPhysicalMemory
            powershell_version = $PSVersionTable.PSVersion.ToString()
            python = @($pythonInfo)
            mingw_target = $targetTriple
            msys2_root = $Msys2Root
        }
        source = [ordered]@{}
        dependencies = [ordered]@{
            pepepow_core = 'bc08108ceccd957c79040dc7fedc1d56d8d47f97'
            openlibm = '9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d'
            openlibm_windows_patch_sha256 = 'f780e659c0a94a8a28a942553c2b0bc7da8c4119b2a897842c361ce97f19c5b2'
        }
        commands = [ordered]@{
            git = (& $git --version | Out-String).Trim()
            gcc = (& $script:Gcc --version | Select-Object -First 1 | Out-String).Trim()
            gxx = (& $script:Gxx --version | Select-Object -First 1 | Out-String).Trim()
            nm = (& $script:Nm --version | Select-Object -First 1 | Out-String).Trim()
            objdump = (& $script:Objdump --version | Select-Object -First 1 | Out-String).Trim()
            cmake = (& $script:Cmake --version | Select-Object -First 1 | Out-String).Trim()
            ninja = (& $script:Ninja --version | Out-String).Trim()
        }
        test_counts = @()
        steps = @()
        limitations = @(
            'Technical acceptance evidence only; this report does not grant legal clearance or formal G0/G1 acceptance.',
            'No production pool was contacted and no real share was submitted.',
            'Operator attestation and Windows system metadata do not replace Owner review of the preserved raw logs.'
        )
        output_directory = $script:RunDirectory
    }
    Save-RunReport

    Invoke-Logged -Name 'fetch-current-main' -FilePath $git -ArgumentList @('-C', $repo, 'fetch', '--prune', 'origin', '+refs/heads/main:refs/remotes/origin/main') -WorkingDirectory $repo -LogName '00-fetch-main'
    $mainSha = Get-GitText -Path $repo -GitArguments @('rev-parse', 'refs/remotes/origin/main')
    if ($ExpectedHead -and $mainSha -ne $ExpectedHead.ToLowerInvariant()) {
        throw "Fetched origin/main is $mainSha, expected $ExpectedHead. No tests were run against a different head."
    }
    $script:Report.source = [ordered]@{
        repository_path = $repo
        branch = 'origin/main'
        exact_head = $mainSha
        expected_head = $(if ($ExpectedHead) { $ExpectedHead } else { $null })
    }
    Save-RunReport

    $sourceWorktree = Join-Path $script:RunDirectory 'source'
    Invoke-Logged -Name 'create-isolated-main-worktree' -FilePath $git -ArgumentList @('-C', $repo, 'worktree', 'add', '--detach', $sourceWorktree, $mainSha) -WorkingDirectory $repo -LogName '01-create-worktree'
    $actualWorktreeSha = Get-GitText -Path $sourceWorktree -GitArguments @('rev-parse', 'HEAD')
    if ($actualWorktreeSha -ne $mainSha) {
        throw "Isolated source worktree is $actualWorktreeSha, expected $mainSha."
    }

    $core = Join-Path $script:RunDirectory 'dependencies\PePe-core'
    $openLibm = Join-Path $script:RunDirectory 'dependencies\openlibm'
    New-Item -ItemType Directory -Force -Path (Join-Path $script:RunDirectory 'dependencies') | Out-Null
    $coreSha = 'bc08108ceccd957c79040dc7fedc1d56d8d47f97'
    $openLibmSha = '9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d'
    Invoke-Logged -Name 'clone-pinned-core' -FilePath $git -ArgumentList @('clone', '--no-checkout', 'https://github.com/edisontw/PePe-core.git', $core) -WorkingDirectory $script:RunDirectory -LogName '02-clone-core'
    Invoke-Logged -Name 'checkout-pinned-core' -FilePath $git -ArgumentList @('-C', $core, 'checkout', '--detach', $coreSha) -WorkingDirectory $script:RunDirectory -LogName '03-checkout-core'
    Invoke-Logged -Name 'clone-pinned-openlibm' -FilePath $git -ArgumentList @('clone', '--no-checkout', 'https://github.com/JuliaMath/openlibm.git', $openLibm) -WorkingDirectory $script:RunDirectory -LogName '04-clone-openlibm'
    Invoke-Logged -Name 'checkout-pinned-openlibm' -FilePath $git -ArgumentList @('-C', $openLibm, 'checkout', '--detach', $openLibmSha) -WorkingDirectory $script:RunDirectory -LogName '05-checkout-openlibm'
    if ((Get-GitText -Path $core -GitArguments @('rev-parse', 'HEAD')) -ne $coreSha) {
        throw 'PEPEPOW Core pin mismatch.'
    }
    if ((Get-GitText -Path $openLibm -GitArguments @('rev-parse', 'HEAD')) -ne $openLibmSha) {
        throw 'OpenLibm pin mismatch.'
    }
    $patch = Join-Path $sourceWorktree '.github\patches\openlibm-portable-c-windows.patch'
    $patchBytes = [System.IO.File]::ReadAllBytes($patch)
    $patchHash = Get-CanonicalPatchSha256 -Bytes $patchBytes
    $rawPatchHash = (Get-FileHash -Path $patch -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($patchHash -ne $script:Report.dependencies.openlibm_windows_patch_sha256) {
        throw "OpenLibm Windows patch canonical SHA-256 mismatch: $patchHash"
    }
    Invoke-Logged -Name 'check-openlibm-patch' -FilePath $git -ArgumentList @('-C', $openLibm, 'apply', '--check', $patch) -WorkingDirectory $script:RunDirectory -LogName '06-check-openlibm-patch'
    Invoke-Logged -Name 'apply-openlibm-patch' -FilePath $git -ArgumentList @('-C', $openLibm, 'apply', $patch) -WorkingDirectory $script:RunDirectory -LogName '07-apply-openlibm-patch'
    $patchPaths = Get-GitText -Path $openLibm -GitArguments @('diff', '--name-only')
    if ($patchPaths -ne 'CMakeLists.txt') {
        throw "Unexpected OpenLibm patch paths: $patchPaths"
    }
    $script:Report.dependencies.openlibm_windows_patch_sha256 = $patchHash
    $script:Report.dependencies.openlibm_windows_patch_raw_sha256 = $rawPatchHash
    Save-RunReport

    $evidenceScript = Join-Path $script:RunDirectory 'support'
    New-Item -ItemType Directory -Force -Path $evidenceScript | Out-Null
    $explorerScript = Join-Path $evidenceScript 'check-explorer.py'
    @'
import json, pathlib, subprocess, sys
repo, kat = pathlib.Path(sys.argv[1]), sys.argv[2]
path = repo / 'tests/vectors/candidates/explorer-chain-4371217-4371226.jsonl'
rows = [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines() if line.strip()]
assert len(rows) == 10, len(rows)
for row in rows:
    digest = row['expected_core_function_output_hex']
    result = subprocess.run([kat, row['header_hex'], digest], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0 and 'MATCH' in result.stdout.splitlines(), (row['case_id'], result.returncode, result.stdout, result.stderr)
    assert 'actual=' + digest in result.stdout.splitlines(), row['case_id']
print('PASS: 10 Explorer headers matched complete 32-byte digests.')
'@ | Set-Content -Path $explorerScript -Encoding ASCII

    $poisonScript = Join-Path $evidenceScript 'poison-last-byte.py'
    @'
from pathlib import Path
import sys
source, destination = map(Path, sys.argv[1:])
rows = source.read_text(encoding='ascii').splitlines()
assert len(rows) == 10000, len(rows)
case, header, digest = rows[-1].split('\t')
assert case == 'g0-synthetic-09999' and len(digest) == 64
rows[-1] = '\t'.join((case, header, digest[:-2] + ('00' if digest[-2:] != '00' else '01')))
destination.write_text('\n'.join(rows) + '\n', encoding='ascii', newline='\n')
'@ | Set-Content -Path $poisonScript -Encoding ASCII

    $fpProbeSource = Join-Path $evidenceScript 'fp-mode.c'
    @'
#include <fenv.h>
#include <float.h>
#include <stdio.h>
int main(void) {
  printf("FLT_ROUNDS=%d FE_TONEAREST=%d fegetround=%d DBL_MANT_DIG=%d sizeof(double)=%zu\n",
         FLT_ROUNDS, FE_TONEAREST, fegetround(), DBL_MANT_DIG, sizeof(double));
  return fegetround() == FE_TONEAREST && FLT_ROUNDS == 1 &&
         DBL_MANT_DIG == 53 && sizeof(double) == 8 ? 0 : 1;
}
'@ | Set-Content -Path $fpProbeSource -Encoding ASCII

    $fpCheckScript = Join-Path $evidenceScript 'check-fp-flags.py'
    @'
import json, pathlib, sys
miner, openlibm = map(pathlib.Path, sys.argv[1:3])
selected = []
for path in (miner, openlibm):
    entries = json.loads(path.read_text(encoding='utf-8'))
    found_hoohash = found_openlibm_exp = False
    for entry in entries:
        command = entry.get('command', ' '.join(entry.get('arguments', [])))
        normalized = command.lower().replace('\\', '/')
        if 'core/src/crypto/' in normalized and 'hoohash.c' in normalized:
            assert '-fno-fast-math' in command and '-ffp-contract=off' in command, command
            assert '-ffast-math' not in command, command
            selected.append('Core HooHash: ' + command)
            found_hoohash = True
        if 'openlibm' in normalized and 'src/e_exp.c' in normalized:
            assert '-fno-fast-math' in command and '-ffp-contract=off' in command, command
            assert '-fno-rounding-math' in command, command
            assert '-ffast-math' not in command, command
            selected.append('OpenLibm exp: ' + command)
            found_openlibm_exp = True
    if path == miner:
        assert found_hoohash, 'Pinned Core HooHash compile command not found'
    else:
        assert found_openlibm_exp, 'Pinned OpenLibm exp compile command not found'
pathlib.Path(sys.argv[3]).write_text('\n'.join(selected) + '\n', encoding='utf-8')
print('PASS: Core and OpenLibm floating-point flags verified.')
'@ | Set-Content -Path $fpCheckScript -Encoding ASCII

    foreach ($configuration in @('Debug', 'Release')) {
        $buildRoot = Join-Path $script:RunDirectory "build\$configuration"
        $openBuild = Join-Path $buildRoot 'openlibm'
        $minerBuild = Join-Path $buildRoot 'miner'
        $stateBuild = Join-Path $buildRoot 'job-state'
        $kat = Join-Path $minerBuild 'pepepow-kat.exe'
        $headers = Join-Path $buildRoot 'headers.tsv'
        $independent = Join-Path $buildRoot 'independent.tsv'
        $coreDigests = Join-Path $buildRoot 'core.tsv'
        $pythonTests = Join-Path $sourceWorktree 'tests'
        New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null

        Invoke-Logged -Name "openlibm-configure-$configuration" -FilePath $script:Cmake -ArgumentList @('-S', $openLibm, '-B', $openBuild, '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$configuration", '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DBUILD_SHARED_LIBS=OFF', '-DOPENLIBM_USE_ASM=OFF') -WorkingDirectory $script:RunDirectory -LogName "10-openlibm-configure-$configuration"
        $openCache = Get-Content -Path (Join-Path $openBuild 'CMakeCache.txt') -Raw
        if ($openCache -match '(?m)^CMAKE_ASM_COMPILER:') {
            throw "OpenLibm unexpectedly configured an assembly compiler in $configuration."
        }
        Invoke-Logged -Name "openlibm-build-$configuration" -FilePath $script:Cmake -ArgumentList @('--build', $openBuild, '--parallel', '2') -WorkingDirectory $script:RunDirectory -LogName "11-openlibm-build-$configuration"
        if (-not (Test-Path (Join-Path $openBuild 'libopenlibm.a'))) {
            throw "Missing OpenLibm archive for $configuration."
        }

        $minerArgs = @(
            '-S', $sourceWorktree,
            '-B', $minerBuild,
            '-G', 'Ninja',
            "-DCMAKE_BUILD_TYPE=$configuration",
            '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
            '-DCMAKE_EXE_LINKER_FLAGS=-Wl,-Map=dependency-link.map',
            '-DHARNESS_CONTRACT_TESTS_ONLY=OFF',
            '-DPEPEPOW_NATIVE_STRATUM_SEARCH=ON',
            "-DPEPEPOW_CORE_SOURCE_DIR=$core",
            "-DPEPEPOW_CORE_EXPECTED_SHA=$coreSha",
            "-DOPENLIBM_INCLUDE_DIR=$(Join-Path $openLibm 'include')",
            "-DOPENLIBM_LIBRARY=$(Join-Path $openBuild 'libopenlibm.a')",
            "-DOPENSSL_ROOT_DIR=$openSslRoot",
            '-DOPENSSL_USE_STATIC_LIBS=FALSE',
            "-DOPENSSL_CRYPTO_LIBRARY=$openSslImportLibrary",
            "-DPython3_EXECUTABLE=$script:Python"
        )
        Invoke-Logged -Name "miner-configure-$configuration" -FilePath $script:Cmake -ArgumentList $minerArgs -WorkingDirectory $script:RunDirectory -LogName "12-miner-configure-$configuration"
        Invoke-Logged -Name "miner-build-$configuration" -FilePath $script:Cmake -ArgumentList @('--build', $minerBuild, '--parallel', '2') -WorkingDirectory $script:RunDirectory -LogName "13-miner-build-$configuration"
        if (-not (Test-Path $kat)) {
            throw "Missing native KAT executable for $configuration."
        }

        Invoke-CtestAndRecordCount -Name 'miner-ctest' -BuildDirectory $minerBuild -Configuration $configuration -LogName "14-miner-ctest-$configuration"
        Invoke-Logged -Name "independent-v110-unit-$configuration" -FilePath $script:Python -ArgumentList @('-m', 'unittest', 'discover', '-s', $pythonTests, '-p', 'test_g0_v110_independent.py', '-v') -WorkingDirectory $sourceWorktree -LogName "15-independent-v110-unit-$configuration"
        $unitLog = Get-Content -Path (Join-Path $script:RunDirectory "logs\15-independent-v110-unit-$configuration.log") -Raw
        if ($unitLog -match 'Ran\s+(\d+) tests?') {
            Add-TestCount -Name 'independent-v110-unittest' -Count ([int]$Matches[1]) -Configuration $configuration -Log "logs\15-independent-v110-unit-$configuration.log"
        }
        Invoke-Logged -Name "explorer-header-kat-$configuration" -FilePath $script:Python -ArgumentList @($explorerScript, $sourceWorktree, $kat) -WorkingDirectory $sourceWorktree -LogName "16-explorer-kat-$configuration"

        Invoke-CapturedToFile -Name "generate-10000-headers-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'generate_g0_headers.py'), '--count', '10000') -WorkingDirectory $sourceWorktree -OutputPath $headers -LogName "17-generate-headers-$configuration"
        if (@(Get-Content -Path $headers).Count -ne 10000) {
            throw "Expected 10,000 generated headers in $configuration."
        }
        Invoke-Logged -Name "export-independent-digests-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'export_independent_v110_digests.py'), $headers, $independent, '--count', '10000') -WorkingDirectory $sourceWorktree -LogName "18-independent-digests-$configuration"
        Invoke-Logged -Name "export-core-digests-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'export_core_reference_digests.py'), '--kat', $kat, '--corpus', $headers, '--output', $coreDigests, '--limit', '10000') -WorkingDirectory $sourceWorktree -LogName "19-core-digests-$configuration"
        Invoke-Logged -Name "compare-10000-full-digests-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'compare_windows_digest_parity.py'), $coreDigests, $independent, '--count', '10000') -WorkingDirectory $sourceWorktree -LogName "20-compare-10000-$configuration"
        Add-TestCount -Name 'full-32-byte-parity-cases' -Count 10000 -Configuration $configuration -Log "logs\20-compare-10000-$configuration.log"

        $emptyFile = Join-Path $buildRoot 'empty.tsv'
        $emptyOutput = Join-Path $buildRoot 'empty-output.tsv'
        Set-Content -Path $emptyFile -Value '' -Encoding ASCII
        Invoke-Logged -Name "empty-corpus-rejected-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'export_independent_v110_digests.py'), $emptyFile, $emptyOutput, '--count', '10000') -WorkingDirectory $sourceWorktree -ExpectedExitCode 1 -LogName "21-empty-corpus-$configuration"
        if (Test-Path $emptyOutput) {
            throw "Empty corpus unexpectedly produced output in $configuration."
        }
        $poisoned = Join-Path $buildRoot 'poisoned.tsv'
        Invoke-Logged -Name "poison-final-byte-$configuration" -FilePath $script:Python -ArgumentList @($poisonScript, $independent, $poisoned) -WorkingDirectory $sourceWorktree -LogName "22-poison-final-byte-$configuration"
        Invoke-Logged -Name "poisoned-final-byte-rejected-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'compare_windows_digest_parity.py'), $coreDigests, $poisoned, '--count', '10000') -WorkingDirectory $sourceWorktree -ExpectedExitCode 1 -LogName "23-poisoned-compare-$configuration"
        $poisonLog = Join-Path $script:RunDirectory "logs\23-poisoned-compare-$configuration.log"
        if (-not (Select-String -Path $poisonLog -Pattern 'MISMATCH row=10000 case=g0-synthetic-09999' -Quiet)) {
            throw "Negative control did not report the final byte mismatch for $configuration."
        }

        Invoke-Logged -Name "verify-fp-flags-$configuration" -FilePath $script:Python -ArgumentList @($fpCheckScript, (Join-Path $minerBuild 'compile_commands.json'), (Join-Path $openBuild 'compile_commands.json'), (Join-Path $buildRoot 'native-fp-flags.txt')) -WorkingDirectory $sourceWorktree -LogName "24-fp-flags-$configuration"
        $fpExe = Join-Path $buildRoot 'fp-mode.exe'
        Invoke-Logged -Name "build-fp-probe-$configuration" -FilePath $script:Gcc -ArgumentList @('-std=c11', '-O2', '-fno-fast-math', '-ffp-contract=off', $fpProbeSource, '-o', $fpExe) -WorkingDirectory $script:RunDirectory -LogName "25-build-fp-probe-$configuration"
        Invoke-Logged -Name "run-fp-probe-$configuration" -FilePath $fpExe -ArgumentList @() -WorkingDirectory $script:RunDirectory -LogName "26-fp-probe-$configuration"
        Invoke-Logged -Name "objdump-native-kat-$configuration" -FilePath $script:Objdump -ArgumentList @('-f', $kat) -WorkingDirectory $script:RunDirectory -LogName "27-objdump-kat-$configuration"
        if (-not (Select-String -Path (Join-Path $script:RunDirectory "logs\27-objdump-kat-$configuration.log") -Pattern 'pei-x86-64' -Quiet)) {
            throw "KAT executable is not a native Windows x64 PE image for $configuration."
        }

        $minerTests = Join-Path $script:RunDirectory "dependencies\$configuration"
        New-Item -ItemType Directory -Force -Path $minerTests | Out-Null
        Copy-Item (Join-Path $minerBuild 'compile_commands.json') (Join-Path $minerTests 'miner-compile-commands.json')
        Copy-Item (Join-Path $openBuild 'compile_commands.json') (Join-Path $minerTests 'openlibm-compile-commands.json')
        Invoke-Logged -Name "capture-miner-ninja-deps-$configuration" -FilePath $script:Ninja -ArgumentList @('-C', $minerBuild, '-t', 'deps') -WorkingDirectory $script:RunDirectory -LogName "28-miner-ninja-deps-$configuration"
        Copy-Item (Join-Path $script:RunDirectory "logs\28-miner-ninja-deps-$configuration.log") (Join-Path $minerTests 'miner-ninja-deps.txt')
        Invoke-Logged -Name "capture-openlibm-ninja-deps-$configuration" -FilePath $script:Ninja -ArgumentList @('-C', $openBuild, '-t', 'deps') -WorkingDirectory $script:RunDirectory -LogName "29-openlibm-ninja-deps-$configuration"
        Copy-Item (Join-Path $script:RunDirectory "logs\29-openlibm-ninja-deps-$configuration.log") (Join-Path $minerTests 'openlibm-ninja-deps.txt')
        Invoke-Logged -Name "capture-core-archive-members-$configuration" -FilePath $script:Ar -ArgumentList @('t', (Join-Path $minerBuild 'libpepepow_core_reference.a')) -WorkingDirectory $script:RunDirectory -LogName "30-core-archive-members-$configuration"
        Copy-Item (Join-Path $script:RunDirectory "logs\30-core-archive-members-$configuration.log") (Join-Path $minerTests 'core-archive-members.txt')
        Invoke-Logged -Name "capture-openlibm-archive-members-$configuration" -FilePath $script:Ar -ArgumentList @('t', (Join-Path $openBuild 'libopenlibm.a')) -WorkingDirectory $script:RunDirectory -LogName "31-openlibm-archive-members-$configuration"
        Copy-Item (Join-Path $script:RunDirectory "logs\31-openlibm-archive-members-$configuration.log") (Join-Path $minerTests 'openlibm-archive-members.txt')
        $mapDirectory = Join-Path $minerTests 'link-maps'
        New-Item -ItemType Directory -Force -Path $mapDirectory | Out-Null
        foreach ($target in @('pepepow-kat', 'pepepow-offline-search', 'offline_stratum_replay', 'test_native_stratum_search', 'offline_stratum_miner')) {
            $targetExe = Join-Path $minerBuild "$target.exe"
            if (Test-Path (Join-Path $minerBuild 'dependency-link.map')) {
                Remove-Item -Force (Join-Path $minerBuild 'dependency-link.map')
            }
            if (Test-Path $targetExe) {
                Remove-Item -Force $targetExe
            }
            Invoke-Logged -Name "relink-$target-$configuration" -FilePath $script:Cmake -ArgumentList @('--build', $minerBuild, '--target', $target, '--parallel', '2') -WorkingDirectory $script:RunDirectory -LogName "32-relink-$target-$configuration"
            if (-not (Test-Path $targetExe) -or -not (Test-Path (Join-Path $minerBuild 'dependency-link.map'))) {
                throw "Missing executable or linker map for $target ($configuration)."
            }
            Copy-Item (Join-Path $minerBuild 'dependency-link.map') (Join-Path $mapDirectory "$target.map")
            Invoke-Logged -Name "capture-imports-$target-$configuration" -FilePath $script:Objdump -ArgumentList @('-p', $targetExe) -WorkingDirectory $script:RunDirectory -LogName "33-imports-$target-$configuration"
            Copy-Item (Join-Path $script:RunDirectory "logs\33-imports-$target-$configuration.log") (Join-Path $minerTests "$target-imports.txt")
            Get-FileHash -Path $targetExe -Algorithm SHA256 | Select-Object Algorithm, Hash, Path | Export-Csv -Path (Join-Path $minerTests 'executables-sha256.csv') -NoTypeInformation -Append
        }
        Get-FileHash -Path (Join-Path $minerBuild 'libpepepow_core_reference.a'), (Join-Path $openBuild 'libopenlibm.a') -Algorithm SHA256 | Select-Object Algorithm, Hash, Path | Export-Csv -Path (Join-Path $minerTests 'archives-sha256.csv') -NoTypeInformation
        $openSslDlls = @(Get-ChildItem (Join-Path $Msys2Root 'ucrt64\bin') -Filter 'libcrypto*.dll' | Select-Object -ExpandProperty FullName)
        if ($openSslDlls.Count -eq 0) {
            throw 'No UCRT64 libcrypto runtime DLL was found.'
        }
        $openSslFiles = @($openSslImportLibrary) + $openSslDlls
        Get-FileHash -Path $openSslFiles -Algorithm SHA256 | Select-Object Algorithm, Hash, Path | Export-Csv -Path (Join-Path $minerTests 'openssl-sha256.csv') -NoTypeInformation

        Invoke-Logged -Name "offline-miner-loopback-e2e-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'test_offline_stratum_miner_e2e.py'), (Join-Path $minerBuild 'offline_stratum_miner.exe')) -WorkingDirectory $sourceWorktree -LogName "34-offline-miner-e2e-$configuration"
        Invoke-Logged -Name "offline-pool-replay-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'check_pool_loopback_replay.py'), (Join-Path $minerBuild 'offline_stratum_replay.exe'), $kat) -WorkingDirectory $sourceWorktree -LogName "35-offline-pool-replay-$configuration"
        Invoke-Logged -Name "offline-cpu-search-$configuration" -FilePath $script:Python -ArgumentList @((Join-Path $pythonTests 'check_offline_cpu_search.py'), (Join-Path $minerBuild 'pepepow-offline-search.exe')) -WorkingDirectory $sourceWorktree -LogName "36-offline-cpu-search-$configuration"

        Invoke-Logged -Name "job-state-configure-$configuration" -FilePath $script:Cmake -ArgumentList @('-S', (Join-Path $sourceWorktree 'tests\job_state'), '-B', $stateBuild, '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$configuration", '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON') -WorkingDirectory $script:RunDirectory -LogName "37-job-state-configure-$configuration"
        Invoke-Logged -Name "job-state-build-$configuration" -FilePath $script:Cmake -ArgumentList @('--build', $stateBuild, '--parallel', '2') -WorkingDirectory $script:RunDirectory -LogName "38-job-state-build-$configuration"
        Invoke-CtestAndRecordCount -Name 'job-state-ctest' -BuildDirectory $stateBuild -Configuration $configuration -LogName "39-job-state-ctest-$configuration"
    }

    $opensslVersion = & (Join-Path $Msys2Root 'ucrt64\bin\openssl.exe') 'version'
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not query the UCRT64 OpenSSL runtime version.'
    }
    $script:Report.commands.openssl = ($opensslVersion | Out-String).Trim()
    $script:Report.status = 'PASS'
    $script:Report.finished_at_utc = [DateTime]::UtcNow.ToString('o')
    Save-RunReport
    Write-Host "PASS: Windows 10/11 physical-host technical validation completed."
    Write-Host "Exact main SHA: $mainSha"
    Write-Host "Evidence directory: $script:RunDirectory"
    Write-Host 'No production pool was contacted and no real share was submitted.'
}
catch {
    if ($null -ne $script:Report) {
        $script:Report.status = 'FAIL'
        $script:Report.failure = $_.Exception.Message
        $script:Report.finished_at_utc = [DateTime]::UtcNow.ToString('o')
        Save-RunReport
    }
    Write-Host "FAIL: $($_.Exception.Message)"
    if ($script:RunDirectory) {
        Write-Host "Partial evidence directory: $script:RunDirectory"
    }
    exit 1
}
