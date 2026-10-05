param([string]$FixtureRoot='generated/prompt16')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try{
    $source="$FixtureRoot/analysis512"
    & "$PSScriptRoot/Run-Smoke.ps1" -Package assets/ScenePackage -Capture -Frames 90 -LogName prompt17-after
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -Capture -Frames 90 -LogName prompt17-fit
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 1500x1000 -Capture -Frames 90 -LogName prompt17-native
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 1500x1000 -Configuration Release -Capture -Frames 90 -LogName prompt17-native-release
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 1500x1000 -Warp -Capture -Frames 90 -LogName prompt17-native-warp
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 1500x1000 -ImageSmoke input -Capture -Frames 90 -LogName prompt17-input
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 1500x1000 -ImageSmoke cycle -Capture -Frames 90 -LogName prompt17-cycle
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 1500x1000 -Exposure 2 -Gamma 2.2 -Bloom -LookSmoke -Capture -Frames 90 -LogName prompt17-look
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 1800x600 -Capture -Frames 90 -LogName prompt17-wide
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 600x1000 -Capture -Frames 90 -LogName prompt17-narrow
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -ImageView grid -UI -Capture -Frames 90 -LogName prompt17-grid-ui
    & "$PSScriptRoot/Run-Smoke.ps1" -Package assets/ScenePackage -WorkMode image -Capture -Frames 90 -LogName prompt17-legacy
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/legacy-upgraded" -WorkMode image -FixedSize -WindowSize 512x341 -Capture -Frames 90 -LogName prompt17-legacy-anchor
    # Existing panel margins leave a 512x341 viewport: verify ImGui's UNORM handoff at native size.
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/legacy-upgraded" -WorkMode image -FixedSize -WindowSize 1103x475 -UI -Capture -Frames 90 -LogName prompt17-native-ui
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/exif6" -WorkMode image -ImageView grid -Capture -Frames 90 -LogName prompt17-exif
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $source -WorkMode image -FixedSize -WindowSize 1500x1000 -ImageSmoke transaction -Capture -Frames 90 -LogName prompt17-transaction
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Package $source -WorkMode image -Image "$FixtureRoot/inputs/not-present.png" -Preset dummy -ExpectError -LogName prompt17-failed
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Package $source -WorkMode image -Image "$FixtureRoot/inputs/细字 ICC.png" -Preset dummy -CancelFrame 30 -ExpectError -LogName prompt17-cancelled
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -WorkMode image -Image "$FixtureRoot/inputs/细字 ICC.png" -NextImage "$FixtureRoot/inputs/旋转 EXIF6.jpg" -Preset dummy -Repeat 2 -UI -LogName prompt17-reload
}finally{Pop-Location}
