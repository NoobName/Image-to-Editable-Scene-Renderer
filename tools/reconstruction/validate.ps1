param([switch]$SkipWarp, [switch]$SkipWindows, [string]$Python)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$environmentPython=Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
if ($Python) { $environmentPython = (Get-Command $Python -CommandType Application -ErrorAction Stop).Source }
if (!(Test-Path -LiteralPath $environmentPython)) { throw 'Run tools/reconstruction/setup.ps1 first.' }
$debugValidator=Join-Path $projectRoot 'build/Debug/ValidateScenePackage.exe'
$releaseValidator=Join-Path $projectRoot 'build/Release/ValidateScenePackage.exe'
if (!(Test-Path -LiteralPath $debugValidator) -or !(Test-Path -LiteralPath $releaseValidator)) { throw 'Build Debug and Release C++ targets first.' }
Push-Location $projectRoot
try {
    & $environmentPython (Join-Path $PSScriptRoot 'test_reconstruction.py') --validator $debugValidator --validator $releaseValidator
    if ($LASTEXITCODE -ne 0) { throw 'Pipeline tests failed.' }
    if (!$SkipWindows) {
        $runRoot=Join-Path $projectRoot ('generated/reconstruction-validation-'+[guid]::NewGuid().ToString('N'))
        $inputImage=Join-Path $runRoot 'input.jpg'
        $package=Join-Path $runRoot 'ScenePackage'
        & $environmentPython (Join-Path $PSScriptRoot 'test_reconstruction.py') --make-input $inputImage
        if ($LASTEXITCODE -ne 0) { throw 'Could not create test input.' }
        & $environmentPython (Join-Path $PSScriptRoot 'reconstruct.py') $inputImage --output $package
        if ($LASTEXITCODE -ne 0) { throw 'Reconstruction failed.' }
        & .\tools\Run-Smoke.ps1 -Package $package -Frames 90 -Capture -LogName reconstruction09-debug
        & .\tools\Run-Smoke.ps1 -Package $package -Frames 90 -Capture -UI -LogName reconstruction09-ui
        & .\tools\Run-Smoke.ps1 -Package $package -Frames 90 -Capture -MaterialSmoke -LogName reconstruction09-material
        & .\tools\Run-Smoke.ps1 -Package $package -Frames 90 -Capture -Configuration Release -LogName reconstruction09-release
        if (!$SkipWarp) { & .\tools\Run-Smoke.ps1 -Package $package -Frames 90 -Capture -Warp -LogName reconstruction09-warp }
        Write-Output "Validated package: $package"
    }
} finally { Pop-Location }
