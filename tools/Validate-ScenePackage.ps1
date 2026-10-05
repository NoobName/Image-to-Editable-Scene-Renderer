param([switch]$SkipWarp)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    & .\tools\Run-Smoke.ps1 -Package 'assets/ScenePackage' -Frames 90 -Capture -LogName package08-debug
    & .\tools\Run-Smoke.ps1 -Package 'assets/ScenePackage/scene.json' -Frames 90 -UI -Capture -LogName package08-ui
    & .\tools\Run-Smoke.ps1 -Package 'assets/ScenePackage' -Frames 90 -CameraSmoke -Capture -LogName package08-camera
    & .\tools\Run-Smoke.ps1 -Package 'assets/ScenePackage' -Frames 90 -Exposure 1 -EnvironmentIntensity 0.5 -Capture -LogName package08-override
    & .\tools\Run-Smoke.ps1 -Package 'assets/ScenePackage' -Frames 90 -Configuration Release -Capture -LogName package08-release
    if (!$SkipWarp) { & .\tools\Run-Smoke.ps1 -Package 'assets/ScenePackage' -Frames 90 -Warp -Capture -LogName package08-warp }
    $baseline=Get-Content generated/package08-debug.log -Raw
    $override=Get-Content generated/package08-override.log -Raw
    if ($baseline -notmatch 'Package look: exposure=0.300000; environment intensity=0.700000') {throw 'Package defaults were overwritten.'}
    if ($override -notmatch 'Package look: exposure=1.000000; environment intensity=0.500000') {throw 'Explicit CLI overrides were not applied.'}
    Write-Output 'ScenePackage window checks passed. Captures and logs: generated/package08-*'
} finally { Pop-Location }
