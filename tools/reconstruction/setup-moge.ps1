param([string]$Python = 'python', [ValidateSet('cu128','cpu')][string]$TorchBackend = 'cu128')
$ErrorActionPreference = 'Stop'
$environmentPython = Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
if (!(Test-Path -LiteralPath $environmentPython)) { & (Join-Path $PSScriptRoot 'setup.ps1') -Python $Python }
& $environmentPython -m pip install --only-binary=:all: "torch==2.8.0+$TorchBackend" --index-url "https://download.pytorch.org/whl/$TorchBackend"
if ($LASTEXITCODE -ne 0) { throw 'PyTorch installation failed.' }
& $environmentPython -m pip install --only-binary=:all: -r (Join-Path $PSScriptRoot 'requirements-moge.txt')
if ($LASTEXITCODE -ne 0) { throw 'MoGe inference dependencies failed to install.' }
& $environmentPython (Join-Path $PSScriptRoot 'fetch_moge_sources.py')
if ($LASTEXITCODE -ne 0) { throw 'Pinned MoGe source download failed.' }
& $environmentPython -m pip check
if ($LASTEXITCODE -ne 0) { throw 'Dependency check failed.' }
Write-Output 'MoGe-2 inference ready. First --geometry-backend moge run downloads the pinned official small normal checkpoint into .cache/.'
