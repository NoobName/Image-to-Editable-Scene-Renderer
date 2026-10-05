param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$environmentPath = Join-Path $PSScriptRoot '.venv'
$environmentPython = Join-Path $environmentPath 'Scripts/python.exe'
& $Python -c 'import sys; assert sys.version_info >= (3, 12), "Reconstruction requires Python 3.12 or newer"'
if ($LASTEXITCODE -ne 0) { throw 'Select Python 3.12+ with -Python <path-to-python.exe>.' }
if (!(Test-Path -LiteralPath $environmentPython)) {
    & $Python -m venv $environmentPath
    if ($LASTEXITCODE -ne 0) { throw 'Could not create the local virtual environment.' }
}
& $environmentPython -c 'import sys; assert sys.prefix != sys.base_prefix; assert sys.version_info >= (3, 12)'
if ($LASTEXITCODE -ne 0) { throw 'Existing .venv is incompatible; use a Python 3.12+ virtual environment.' }
& $environmentPython -m pip install --only-binary=:all: -r (Join-Path $PSScriptRoot 'requirements.txt')
if ($LASTEXITCODE -ne 0) { throw 'Dependency installation failed.' }
& $environmentPython -m pip check
if ($LASTEXITCODE -ne 0) { throw 'Dependency check failed.' }
Write-Output "Environment ready: $environmentPython"
