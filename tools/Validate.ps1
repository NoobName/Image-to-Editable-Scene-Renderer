$ErrorActionPreference = 'Stop'
# Build first with Build.ps1. These are real GUI tests and open temporary windows.
foreach ($configuration in @('Debug','Release')) {
    foreach ($demo in @('clear','triangle','scene')) {
        & "$PSScriptRoot/Run-Smoke.ps1" -Configuration $configuration -Demo $demo -Frames 180 -Capture -LogName "final-$demo-$configuration"
    }
}
& "$PSScriptRoot/Run-Smoke.ps1" -Demo scene -Frames 180 -Capture -ReverseOrder -LogName final-scene-reversed
& "$PSScriptRoot/Run-Smoke.ps1" -Demo scene -Frames 180 -Capture -CameraSmoke -LogName final-scene-camera
& "$PSScriptRoot/Run-Smoke.ps1" -Demo scene -Frames 90 -Warp -LogName final-scene-warp
$projectRoot = Split-Path $PSScriptRoot -Parent
$normal = (Get-FileHash (Join-Path $projectRoot 'generated/final-scene-Debug.bmp')).Hash
$reversed = (Get-FileHash (Join-Path $projectRoot 'generated/final-scene-reversed.bmp')).Hash
$camera = (Get-FileHash (Join-Path $projectRoot 'generated/final-scene-camera.bmp')).Hash
if ($normal -ne $reversed) { throw 'Depth test failed: reversing draw order changed the image.' }
if ($normal -eq $camera) { throw 'Camera test failed: input did not change the image.' }
Write-Output "PASS: 9 renderer runs; depth order invariant; camera image changed. Scene SHA256=$normal"
