param([string]$FixtureRoot='generated/prompt16')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    # Finite windows use the existing resize/minimize/restore and HDR readback checks.
    & "$PSScriptRoot/Run-Smoke.ps1" -Package assets/ScenePackage -Capture -Frames 90 -LogName prompt16-after
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/analysis512" -UI -Capture -Frames 90 -LogName prompt16-new-ui
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/exif6" -RenderMode original -Capture -Frames 90 -LogName prompt16-exif
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/analysis512" -Warp -Capture -Frames 90 -LogName prompt16-warp
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/搬移目录/analysis256" -Configuration Release -Capture -Frames 90 -LogName prompt16-release
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Image "$FixtureRoot/inputs/细字 ICC.png" -Preset dummy -Repeat 2 -UI -RenderMode final -LogName prompt16-reload
} finally { Pop-Location }
