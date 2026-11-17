<#
Builds PatientBase for Windows 10 (x64) and produces a self-contained folder + zip.

Run from a Qt command prompt, e.g. "Qt 6.x.x (MSVC 2022 64-bit)", so qmake, nmake
and windeployqt are on PATH:

    powershell -ExecutionPolicy Bypass -File build_windows.ps1 -PgBin "C:\Program Files\PostgreSQL\16\bin"

-PgBin  bin folder of a 64-bit PostgreSQL install (or the EDB binaries zip); libpq.dll
        and its dependencies are copied from it. Without it the app cannot connect.
#>
param(
    [string]$PgBin = ""
)
$ErrorActionPreference = "Stop"

$Root   = $PSScriptRoot
$Client = Join-Path $Root "client"
$Build  = Join-Path $Root "build-win"
$Dist   = Join-Path $Root "dist-win"
# Russian folder name written as escapes so this file stays ASCII-only
$TemplatesName = [regex]::Unescape('\u0448\u0430\u0431\u043b\u043e\u043d\u044b_\u043e\u0441\u043d\u043e\u0432\u043d\u044b\u0445_\u0434\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u043e\u0432')

foreach ($t in "qmake", "windeployqt") {
    if (-not (Get-Command $t -ErrorAction SilentlyContinue)) {
        throw "$t not found in PATH. Run this from the Qt command prompt (e.g. 'Qt 6.x MSVC 2022 64-bit')."
    }
}
$make = $null
foreach ($m in "jom", "nmake", "mingw32-make") {
    if (Get-Command $m -ErrorAction SilentlyContinue) { $make = $m; break }
}
if (-not $make) { throw "No make tool found (jom / nmake / mingw32-make)." }

# --- build ---------------------------------------------------------------
if (Test-Path $Build) { Remove-Item -Recurse -Force $Build }
New-Item -ItemType Directory -Path $Build | Out-Null
Push-Location $Build
try {
    qmake (Join-Path $Client "PatientBase.pro") CONFIG+=release
    if ($LASTEXITCODE -ne 0) { throw "qmake failed" }
    & $make
    if ($LASTEXITCODE -ne 0) { throw "$make failed" }
} finally { Pop-Location }

$exe = Get-ChildItem -Path $Build -Recurse -Filter PatientBase.exe | Select-Object -First 1
if (-not $exe) { throw "PatientBase.exe not found after build" }

# --- assemble dist -------------------------------------------------------
if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Path $Dist | Out-Null
New-Item -ItemType Directory -Path (Join-Path $Dist "Output\ALL_HYSTORY") -Force | Out-Null

Copy-Item $exe.FullName $Dist
Copy-Item (Join-Path $Client "DocParser.exe") $Dist
Copy-Item (Join-Path $Client "settings.ini")  $Dist
Copy-Item (Join-Path $Root "tools\install.bat") $Dist
Copy-Item (Join-Path $Client "Sources_files") $Dist -Recurse
$tpl = Join-Path $Client $TemplatesName
if (-not (Test-Path $tpl)) { throw "Templates folder not found: $tpl (check the folder name after unzipping)" }
Copy-Item $tpl $Dist -Recurse

# --- Qt runtime + plugins (platforms, sqldrivers/qsqlpsql, ...) ----------
windeployqt --release --no-translations --compiler-runtime (Join-Path $Dist "PatientBase.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

if (-not (Test-Path (Join-Path $Dist "sqldrivers\qsqlpsql.dll"))) {
    Write-Warning "sqldrivers\qsqlpsql.dll was not deployed - the Qt kit may lack the PostgreSQL driver."
}

# --- libpq and dependencies ---------------------------------------------
if ($PgBin -and (Test-Path $PgBin)) {
    $patterns = "libpq.dll", "libssl-*.dll", "libcrypto-*.dll", "libintl-*.dll",
                "libiconv-*.dll", "libwinpthread-*.dll", "zlib1.dll", "libzstd.dll", "vcruntime*.dll"
    foreach ($p in $patterns) {
        Get-ChildItem -Path $PgBin -Filter $p -ErrorAction SilentlyContinue |
            ForEach-Object { Copy-Item $_.FullName $Dist -Force }
    }
    if (-not (Test-Path (Join-Path $Dist "libpq.dll"))) {
        Write-Warning "libpq.dll not found in $PgBin"
    }
} else {
    Write-Warning "-PgBin not given: libpq.dll NOT bundled. The app will fail with 'driver not loaded' on PCs without PostgreSQL."
}

# --- archive -------------------------------------------------------------
$zip = Join-Path $Root "PatientBase-windows10-x64.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $Dist "*") -DestinationPath $zip

Write-Host ""
Write-Host "Built:   $Dist"
Write-Host "Archive: $zip"
Write-Host "Edit settings.ini (host/user/password), then run install.bat once to get the desktop and Start menu icons."
