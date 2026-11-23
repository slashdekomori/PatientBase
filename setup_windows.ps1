<#
Prepares the Windows build environment for PatientBase and runs build_windows.ps1.

Default behaviour (no switches):
  - looks for an already installed Qt 6.x llvm-mingw kit, its toolchain and PostgreSQL
  - verifies them (qmake spec, clang, make, libpq bitness)
  - sets PATH for the child process only and runs build_windows.ps1
  - never downloads or changes the system

With -Download:
  - installs only what is missing (aqtinstall -> Qt + toolchain, winget/EDB zip -> PostgreSQL)
  - existing installations are reused, no duplicates are created

Examples:
  powershell -ExecutionPolicy Bypass -File .\setup_windows.ps1
  powershell -ExecutionPolicy Bypass -File .\setup_windows.ps1 -Download
  powershell -ExecutionPolicy Bypass -File .\setup_windows.ps1 -Download -QtMirror https://mirrors.ustc.edu.cn/qtproject/
  powershell -ExecutionPolicy Bypass -File .\setup_windows.ps1 -PgBin "C:\Program Files\PostgreSQL\18\bin" -SkipBuild
#>
param(
    [string]$QtRoot   = "C:\Qt",
    [string]$QtVersion = "6.7.3",
    [string]$PgBin    = "",
    [string]$QtMirror = "",
    [string]$AqtExe   = "",
    [string]$PgZipUrl = "https://get.enterprisedb.com/postgresql/postgresql-18.1-1-windows-x64-binaries.zip",
    [string[]]$ExtraPgRoots = @(),
    [switch]$Download,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$AqtDefault = Join-Path $Root "tools\aqt.exe"
$PgExtract  = Join-Path $Root "tools\pg"

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host "    OK: $msg" -ForegroundColor Green }
function Write-Warn2($msg) { Write-Host "    WARN: $msg" -ForegroundColor Yellow }
function Write-Fail($msg)  { Write-Host "    FAIL: $msg" -ForegroundColor Red }

# Read the PE machine type: 0x8664 = x64, 0xAA64 = arm64.
function Test-IsPe64([string]$Path) {
    try {
        $fs = [System.IO.File]::OpenRead($Path)
        try {
            $br = [System.IO.BinaryReader]::new($fs)
            [void]$fs.Seek(0x3C, [System.IO.SeekOrigin]::Begin)
            $peOffset = $br.ReadInt32()
            [void]$fs.Seek($peOffset + 4, [System.IO.SeekOrigin]::Begin)
            $machine = $br.ReadUInt16()
            return ($machine -eq 0x8664 -or $machine -eq 0xAA64)
        } finally { $br.Dispose() }
    } catch { return $false }
}

function Get-QmakeSpec([string]$Qmake) {
    try {
        $out = (& $Qmake -query QMAKE_XSPEC 2>$null | Out-String).Trim()
        return $out
    } catch { return "" }
}

function Find-QtKit {
    $versionRoot = Join-Path $QtRoot $QtVersion
    if (-not (Test-Path $versionRoot)) { return $null }
    $found = @()
    Get-ChildItem -Path $versionRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        $qmake = Join-Path $_.FullName "bin\qmake.exe"
        if (-not (Test-Path $qmake)) { return }
        $spec = Get-QmakeSpec $qmake
        if ($spec -match 'clang.*g\+\+') {
            $found += [pscustomobject]@{
                Root = $_.FullName
                Bin  = Split-Path $qmake -Parent
                Spec = $spec
            }
        }
    }
    if (-not $found) { return $null }
    # Prefer the folder whose name mentions llvm, otherwise the first one.
    $preferred = $found | Where-Object { $_.Root -match 'llvm' } | Select-Object -First 1
    if ($preferred) { return $preferred }
    return $found[0]
}

function Find-Toolchain {
    $toolsRoot = Join-Path $QtRoot "Tools"
    if (-not (Test-Path $toolsRoot)) { return $null }
    $found = @()
    Get-ChildItem -Path $toolsRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        $bin = Join-Path $_.FullName "bin"
        if ((Test-Path (Join-Path $bin "clang++.exe")) -and
            (Test-Path (Join-Path $bin "mingw32-make.exe")) -and
            (Test-Path (Join-Path $bin "libc++.dll"))) {
            $found += [pscustomobject]@{ Root = $_.FullName; Bin = $bin; Name = $_.Name }
        }
    }
    if (-not $found) { return $null }
    $preferred = $found | Where-Object { $_.Name -match 'llvm' } | Select-Object -First 1
    if ($preferred) { return $preferred }
    return $found[0]
}

function Find-PgBin([string[]]$Roots) {
    $dirs = @()
    if ($PgBin) { $dirs += $PgBin }
    foreach ($r in $Roots) {
        if (-not $r -or -not (Test-Path $r)) { continue }
        $direct = Join-Path $r "bin"
        if (Test-Path (Join-Path $direct "libpq.dll")) { $dirs += $direct }
        Get-ChildItem -Path $r -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $top = $_
            # Covers both <root>\PostgreSQL\bin and <root>\PostgreSQL\18\bin
            foreach ($base in @($top.FullName) + @(Get-ChildItem -Path $top.FullName -Directory -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })) {
                $candidate = Join-Path $base "bin"
                if (($top.Name -match 'PostgreSQL|EDB' -or $base -match 'PostgreSQL|EDB') -and
                    (Test-Path (Join-Path $candidate "libpq.dll"))) {
                    $dirs += $candidate
                }
            }
        }
    }
    $dirs = $dirs | Where-Object { Test-Path (Join-Path $_ "libpq.dll") } | Select-Object -Unique
    if (-not $dirs) { return "" }
    $x64 = $dirs | Where-Object { Test-IsPe64 (Join-Path $_ "libpq.dll") } | Select-Object -First 1
    if ($x64) { return $x64 }
    Write-Warn2 "libpq.dll found but not detected as x64: $($dirs[0])"
    return $dirs[0]
}

function Get-PgSearchRoots {
    $roots = @(
        ${env:ProgramFiles},
        ${env:ProgramFiles(x86)},
        (Join-Path $env:LOCALAPPDATA "Programs"),
        $PgExtract
    ) + $ExtraPgRoots
    return $roots | Where-Object { $_ } | Select-Object -Unique
}

function Ensure-Aqt {
    if ($AqtExe -and (Test-Path $AqtExe)) { return $AqtExe }
    if (Test-Path $AqtDefault) { return $AqtDefault }
    if (-not $Download) { return "" }
    Write-Step "Downloading aqtinstall (standalone aqt.exe)"
    $dir = Split-Path $AqtDefault -Parent
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    try {
        Invoke-WebRequest -Uri "https://github.com/miurahr/aqtinstall/releases/download/Continuous/aqt.exe" `
            -OutFile $AqtDefault -UseBasicParsing
    } catch {
        throw "Cannot download aqt.exe: $($_.Exception.Message)"
    }
    return $AqtDefault
}

function New-AqtConfig {
    if (-not $QtMirror) { return @() }
    $cfg = Join-Path $env:TEMP "patientbase-aqt.ini"
    @(
        "[aqt]",
        "baseurl = $QtMirror",
        "",
        "[mirrors]",
        "trusted_mirrors =",
        "    $QtMirror"
    ) | Set-Content -Path $cfg -Encoding ASCII
    return @("-c", $cfg)
}

function Install-QtKit {
    $aqt = Ensure-Aqt
    if (-not $aqt) { throw "aqt.exe not found. Pass -AqtExe <path> or use -Download." }
    $cfgArgs = New-AqtConfig

    Write-Step "Querying available Qt architectures for $QtVersion"
    $list = & $aqt @cfgArgs list-qt windows desktop --arch $QtVersion 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw ("Cannot query Qt repository. Check internet/proxy or pass -QtMirror <url>.`n" + ($list | Out-String))
    }
    $arch = ($list | ForEach-Object { $_ -split '\s+' } | Where-Object { $_ -eq 'win64_llvm_mingw' } | Select-Object -First 1)
    if (-not $arch) { $arch = "win64_llvm_mingw" }

    Write-Step "Installing Qt $QtVersion ($arch) into $QtRoot"
    & $aqt @cfgArgs install-qt windows desktop $QtVersion $arch -O $QtRoot
    if ($LASTEXITCODE -ne 0) { throw "aqt install-qt failed." }

    Write-Step "Installing the llvm-mingw toolchain"
    $tools = & $aqt @cfgArgs list-tool windows desktop 2>&1
    $tool = ($tools | ForEach-Object { $_ -split '\s+' } | Where-Object { $_ -match 'llvm' } | Select-Object -First 1)
    if (-not $tool) { $tool = "tools_llvm-mingw" }
    $variants = & $aqt @cfgArgs list-tool windows desktop $tool -l 2>&1
    $variant = ($variants | ForEach-Object { $_ -split '\s+' } | Where-Object { $_ -match 'llvm_mingw' } | Select-Object -First 1)
    if (-not $variant) { $variant = "qt.tools.win64_llvm_mingw1706_64" }
    & $aqt @cfgArgs install-tool windows desktop $tool $variant -O $QtRoot
    if ($LASTEXITCODE -ne 0) {
        Write-Warn2 "Toolchain install failed; trying to continue with what is already in $QtRoot\Tools."
    }
}

function Install-PostgreSQL {
    Write-Step "Trying winget: PostgreSQL 18"
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if ($winget) {
        & $winget.Source install --id PostgreSQL.PostgreSQL.18 -e `
            --accept-source-agreements --accept-package-agreements --silent 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) { Write-Ok "winget install finished" }
        else { Write-Warn2 "winget install failed (exit $LASTEXITCODE)" }
    } else {
        Write-Warn2 "winget not found"
    }

    if (Find-PgBin (Get-PgSearchRoots)) { return }

    Write-Step "Downloading PostgreSQL binaries: $PgZipUrl"
    $zip = Join-Path $env:TEMP "patientbase-pg-binaries.zip"
    try {
        Invoke-WebRequest -Uri $PgZipUrl -OutFile $zip -UseBasicParsing
        if (-not (Test-Path $PgExtract)) { New-Item -ItemType Directory -Path $PgExtract -Force | Out-Null }
        Expand-Archive -Path $zip -DestinationPath $PgExtract -Force
    } catch {
        throw ("Cannot download/unpack PostgreSQL binaries: $($_.Exception.Message). " +
               "Set -PgBin to an existing bin folder or override -PgZipUrl.")
    }
}

# ---------------------------------------------------------------- main ----
Write-Step "Looking for an existing Qt $QtVersion llvm-mingw kit"
$kit = Find-QtKit
if ($kit) { Write-Ok "Qt kit: $($kit.Root) [$($kit.Spec)]" }
elseif ($Download) { Install-QtKit; $kit = Find-QtKit }
if (-not $kit) {
    throw "Qt llvm-mingw kit not found. Run with -Download, install it via Qt Online Installer, or pass -QtRoot/-QtVersion."
}

Write-Step "Looking for the llvm-mingw toolchain"
$tc = Find-Toolchain
if ($tc) { Write-Ok "Toolchain: $($tc.Root)" }
elseif ($Download) { $kit = $null; $kit = Find-QtKit; $tc = Find-Toolchain }
if (-not $tc) {
    throw "Toolchain with clang++ + mingw32-make + libc++.dll not found under $QtRoot\Tools. Run with -Download or install Qt's 'LLVM MinGW 17.0.6 64-bit' tool."
}

Write-Step "Looking for PostgreSQL binaries (libpq.dll)"
$pg = Find-PgBin (Get-PgSearchRoots)
if ($pg) { Write-Ok "PostgreSQL: $pg" }
elseif ($Download) { Install-PostgreSQL; $pg = Find-PgBin (Get-PgSearchRoots) }
if (-not $pg) {
    Write-Warn2 "libpq.dll not found - build will continue, but the app will need PostgreSQL installed on the target PC."
}

# PATH is changed for this process and its children only.
$env:PATH = "$($kit.Bin);$($tc.Bin);$env:PATH"
Write-Step "Environment"
Write-Ok "qmake      = $((Get-Command qmake -ErrorAction SilentlyContinue).Source)"
Write-Ok "windeployqt= $((Get-Command windeployqt -ErrorAction SilentlyContinue).Source)"
Write-Ok "clang++    = $((Get-Command clang++ -ErrorAction SilentlyContinue).Source)"
Write-Ok "make       = $((Get-Command mingw32-make -ErrorAction SilentlyContinue).Source)"

if ($SkipBuild) {
    Write-Host "`nPrepared PATH for this session:"
    Write-Host "set PATH=$($kit.Bin);$($tc.Bin);%PATH%" -ForegroundColor Yellow
    exit 0
}

$buildScript = Join-Path $Root "build_windows.ps1"
if (-not (Test-Path $buildScript)) { throw "build_windows.ps1 not found next to setup_windows.ps1" }

Write-Step "Running build_windows.ps1"
$buildArgs = @("-ExecutionPolicy", "Bypass", "-File", $buildScript)
if ($pg) { $buildArgs += @("-PgBin", $pg) }
& powershell @buildArgs
$code = $LASTEXITCODE
if ($code -ne 0) { Write-Fail "Build failed with exit code $code" }
exit $code
