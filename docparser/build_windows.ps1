# Rebuild client/DocParser.exe from the Python sources in this directory.
# Requires Python 3.12 (the version used for the committed Windows binary).
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if (-not (Get-Command py -ErrorAction SilentlyContinue)) {
    throw "Python launcher 'py' not found. Install Python 3.12 first."
}

py -3.12 -m venv .venv-win
$python = Join-Path $PSScriptRoot '.venv-win\Scripts\python.exe'

& $python -m pip install --upgrade pip
& $python -m pip install -r requirements.txt nuitka zstandard
& $python -m nuitka --onefile --standalone --remove-output DocParser.py

$built = Join-Path $PSScriptRoot 'DocParser.exe'
$target = Join-Path $PSScriptRoot '..\client\DocParser.exe'
if (-not (Test-Path $built)) {
    throw "Build failed: $built was not produced"
}
Copy-Item -LiteralPath $built -Destination $target -Force
Write-Host "Updated $target"
