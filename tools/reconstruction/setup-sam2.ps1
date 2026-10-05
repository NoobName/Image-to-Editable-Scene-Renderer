param([string]$Python='python',[ValidateSet('cu129','cu128','cpu')][string]$TorchBackend='cu129')
$ErrorActionPreference='Stop'
# Called within the existing dedicated project environment; no torch upgrade is allowed.
& $Python -c "import torch; assert torch.__version__ == '2.8.0+$TorchBackend', 'Use the project torch 2.8.0 environment with the matching TorchBackend'"
if($LASTEXITCODE -ne 0){throw 'PyTorch version mismatch.'}
& $Python -m pip install --only-binary=:all: --no-deps "torchvision==0.23.0+$TorchBackend" --index-url "https://download.pytorch.org/whl/$TorchBackend"
if($LASTEXITCODE -ne 0){throw 'torchvision installation failed.'}
& $Python -m pip install -r (Join-Path $PSScriptRoot 'requirements-sam2.txt')
if($LASTEXITCODE -ne 0){throw 'SAM 2 dependencies failed.'}
& $Python (Join-Path $PSScriptRoot 'fetch_sam2.py')
if($LASTEXITCODE -ne 0){throw 'SAM 2 asset download failed.'}
& $Python -m pip check
if($LASTEXITCODE -ne 0){throw 'Dependency check failed.'}
Write-Output 'SAM 2.1 Tiny ready. Native Windows image inference uses no optional CUDA extension.'
