param([string]$Package='generated/scene23-indoor')
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try{
    foreach($view in @('intrinsic-albedo','intrinsic-shading','intrinsic-residual','intrinsic-uncertainty','intrinsic-error','intrinsic-validity','albedo','original','relighted')){
        & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView $view -FixedSize -WindowSize 512x341 -Frames 100 -Capture -LogName "prompt23-$view"
    }
    foreach($test in @('views','transaction','cycle','ratio-drag')){
        & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -ImageSmoke $test -FixedSize -WindowSize 512x341 -Frames 120 -Capture -LogName "prompt23-$test"
    }
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView intrinsic-residual -Warp -Frames 100 -Capture -LogName prompt23-warp
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView intrinsic-albedo -UI -Frames 100 -Capture -LogName prompt23-ui
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView intrinsic-shading -Configuration Release -Frames 100 -Capture -LogName prompt23-release
    & ./tools/Run-Smoke.ps1 -Package generated/prompt23-proxy -WorkMode image -ImageView intrinsic-residual -Frames 100 -Capture -LogName prompt23-proxy-missing
    & ./tools/Run-Smoke.ps1 -Package generated/scene19-final -WorkMode image -ImageView intrinsic-shading -Frames 100 -Capture -LogName prompt23-legacy-missing
    & ./tools/Run-Smoke.ps1 -Package generated/scene19-final -WorkMode image -ImageView relighted -ImageSmoke ratio-drag -FixedSize -WindowSize 512x341 -Frames 120 -Capture -LogName prompt23-baseline
    foreach($name in @('outdoor','painting')){
        & ./tools/Run-Smoke.ps1 -Package "generated/scene23-$name" -WorkMode image -ImageView intrinsic-error -Frames 100 -Capture -LogName "prompt23-$name"
    }
    & ./tools/Run-Smoke.ps1 -Package assets/ScenePackage -Frames 100 -Capture -LogName prompt23-final
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image $Package -IntrinsicOnly -IntrinsicBackend saved -WorkMode image -ImageView intrinsic-albedo -Repeat 2 -LogName prompt23-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image generated/missing23 -IntrinsicOnly -WorkMode image -ImageView original -ExpectError -LogName prompt23-failed
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image $Package -IntrinsicOnly -IntrinsicBackend marigold-lighting -WorkMode image -ImageView original -CancelFrame 2 -ExpectError -LogName prompt23-cancel
}finally{Pop-Location}
