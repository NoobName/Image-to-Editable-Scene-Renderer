param([string]$Python='python')
$ErrorActionPreference='Stop'
& $Python -c "import torch; assert torch.__version__.split('+')[0] == '2.8.0', 'Use the dedicated project torch 2.8.0 environment'"
if($LASTEXITCODE -ne 0){throw 'Project environment required.'}
& $Python -m pip install -r (Join-Path $PSScriptRoot 'requirements-material.txt')
if($LASTEXITCODE -ne 0){throw 'Material dependencies failed.'}
& $Python (Join-Path $PSScriptRoot 'fetch_material.py')
if($LASTEXITCODE -ne 0){throw 'Official material checkpoint download failed.'}
& $Python -m pip check
if($LASTEXITCODE -ne 0){throw 'Dependency check failed.'}
