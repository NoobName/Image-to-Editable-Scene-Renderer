param([string]$FixtureRoot='generated/prompt18',[string]$RealPackage='generated/scene18')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try{
    foreach($kind in @('plane','tilted','step')){
        foreach($device in @('warp','hardware')){
            $extra=if($device -eq 'hardware'){@('--hardware')}else{@()}
            & ./build/Debug/AnalysisGpuTests.exe "$FixtureRoot/$kind" "generated/prompt18-numeric-$kind-$device" @extra
            if($LASTEXITCODE -ne 0){throw "Numeric GPU test failed: $kind/$device"}
        }
    }
    & "$PSScriptRoot/Run-Smoke.ps1" -Package assets/ScenePackage -Capture -Frames 90 -LogName prompt18-after
    $views=@('original','grid','depth','geometry-normal','world-normal','position','validity','region','albedo','roughness','metallic','geometry-confidence','material-confidence','region-confidence','tangent-normal')
    foreach($view in $views){
        & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView $view -FixedSize -WindowSize 512x341 -Capture -Frames 90 -LogName "prompt18-real-$view"
    }
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView geometry-normal -UI -Capture -Frames 90 -LogName prompt18-ui
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView depth -Warp -Capture -Frames 90 -LogName prompt18-warp-resize
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView position -ImageSmoke views -Capture -Frames 90 -LogName prompt18-views
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView position -ImageSmoke cycle -FixedSize -WindowSize 512x341 -Capture -Frames 90 -LogName prompt18-cycle
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView original -ImageSmoke cycle -FixedSize -WindowSize 512x341 -Capture -Frames 90 -LogName prompt18-source-cycle
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView position -ImageSmoke transaction -Capture -Frames 90 -LogName prompt18-transaction
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView depth -Configuration Release -Capture -Frames 90 -LogName prompt18-release
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView region -FixedSize -WindowSize 1800x600 -Capture -Frames 90 -LogName prompt18-wide
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $RealPackage -WorkMode image -ImageView geometry-normal -FixedSize -WindowSize 600x1000 -Capture -Frames 90 -LogName prompt18-narrow
    & "$PSScriptRoot/Run-Smoke.ps1" -Package generated/prompt16/analysis512 -WorkMode image -ImageView depth -UI -Capture -Frames 90 -LogName prompt18-unavailable
    & "$PSScriptRoot/Run-Smoke.ps1" -Package generated/prompt16/analysis512 -WorkMode image -FixedSize -WindowSize 1500x1000 -Capture -Frames 90 -LogName prompt18-highres
    foreach($view in @('original','depth','geometry-normal','position','validity','region')){
        & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/step" -WorkMode image -ImageView $view -FixedSize -WindowSize 390x198 -Capture -Frames 90 -LogName "prompt18-step-$view"
    }
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$FixtureRoot/step" -WorkMode image -FixedSize -WindowSize 195x99 -Capture -Frames 90 -LogName prompt18-step-native
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Package $RealPackage -WorkMode image -ImageView position -Image "$FixtureRoot/missing.png" -Preset dummy -ExpectError -LogName prompt18-failed
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Package $RealPackage -WorkMode image -ImageView position -Image "$FixtureRoot/数值输入.png" -Preset dummy -CancelFrame 30 -ExpectError -LogName prompt18-cancelled
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -WorkMode image -ImageView geometry-normal -Image "$FixtureRoot/数值输入.png" -NextImage 'generated/prompt16/inputs/旋转 EXIF6.jpg' -Preset dummy -Repeat 2 -UI -LogName prompt18-reload
}finally{Pop-Location}
