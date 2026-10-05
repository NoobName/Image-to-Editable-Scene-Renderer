param([string]$InputImage, [ValidateSet('auto','cpu','cuda')][string]$Device='auto', [switch]$Offline, [switch]$SkipWindows, [string]$Python)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$environmentPython = Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
if ($Python) { $environmentPython = (Get-Command $Python -CommandType Application -ErrorAction Stop).Source }
if (!$InputImage) { $InputImage = Join-Path $PSScriptRoot '.vendor/MoGe-b942f00bdc2a2a23ebb474fbe034d487e6dcceec/example_images/01_HouseIndoor.jpg' }
$InputImage = (Resolve-Path -LiteralPath $InputImage).Path
$outputPackage = Join-Path $projectRoot ('generated/moge-validation-'+[guid]::NewGuid().ToString('N'))
$reconstructionArguments = @((Join-Path $PSScriptRoot 'reconstruct.py'), $InputImage, '--output', $outputPackage,
    '--geometry-backend','moge','--device',$Device,'--max-size','512','--grid-size','129','--resolution-level','0')
if ($Offline) { $reconstructionArguments += '--offline' }
Push-Location $projectRoot
try {
    & $environmentPython @reconstructionArguments
    if ($LASTEXITCODE -ne 0) { throw 'Real MoGe inference/export failed.' }
    & $environmentPython (Join-Path $PSScriptRoot 'inspect_geometry.py') $outputPackage --require-model
    if ($LASTEXITCODE -ne 0) { throw 'Exported numeric geometry validation failed.' }
    foreach ($configuration in @('Debug','Release')) {
        & (Join-Path $projectRoot "build/$configuration/ValidateScenePackage.exe") $outputPackage | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "$configuration C++ loading failed." }
    }
    if (!$SkipWindows) {
        & .\tools\Run-Smoke.ps1 -Package $outputPackage -Frames 90 -Capture -UI -LogName reconstruction10-ui
        & .\tools\Run-Smoke.ps1 -Package $outputPackage -Frames 90 -Capture -Configuration Release -LogName reconstruction10-release
    }
    Write-Output "Validated real geometry package: $outputPackage"
} finally { Pop-Location }
