param([string]$Package='generated/scene24-final-indoor')
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try{
    foreach($view in @('intrinsic-shading','calculated-old','diffuse-support','protected-residual','effective-log-ratio','relighted')){
        & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView $view -FixedSize -WindowSize 512x341 -Frames 100 -Capture -LogName "prompt24-$view"
    }
    foreach($test in @('ratio-drag','ratio-color','ratio-reset','views','transaction','cycle')){
        & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -ImageSmoke $test -Frames 120 -Capture -LogName "prompt24-$test"
    }
    foreach($test in @('ratio-drag','ratio-color','ratio-reset')){
        & ./tools/Run-Smoke.ps1 -Package generated/prompt24-synthetic-final/assisted -WorkMode image -ImageView relighted -ImageSmoke $test -FixedSize -WindowSize 97x65 -Frames 100 -Capture -LogName "prompt24-synthetic-$test"
    }
    & ./tools/Run-Smoke.ps1 -Package generated/prompt24-synthetic-final/assisted -WorkMode image -ImageView diffuse-support -Warp -Frames 100 -Capture -LogName prompt24-warp
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView diffuse-support -UI -Frames 100 -Capture -LogName prompt24-ui
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -Configuration Release -Frames 100 -Capture -LogName prompt24-release
    & ./tools/Run-Smoke.ps1 -Package generated/prompt24-missing-final -WorkMode image -ImageView diffuse-support -Frames 100 -Capture -LogName prompt24-missing
    & ./tools/Run-Smoke.ps1 -Package assets/ScenePackage -Frames 100 -Capture -LogName prompt24-final
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image $Package -LightingOnly -LightingBackend intrinsic-assisted -WorkMode image -ImageView relighted -Repeat 2 -LogName prompt24-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image generated/missing24 -LightingOnly -LightingBackend intrinsic-assisted -WorkMode image -ImageView original -ExpectError -LogName prompt24-failed
}finally{Pop-Location}
