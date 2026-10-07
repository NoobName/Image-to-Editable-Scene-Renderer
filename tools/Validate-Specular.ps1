$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try{
    foreach($shape in @('plane','sphere')){
        foreach($mode in @('specular-move','specular-off','specular-rough','specular-reset','specular-invalid')){
            & ./tools/Run-Smoke.ps1 -Package "generated/prompt25-fixtures/$shape" -WorkMode image -ImageView relighted -ImageSmoke $mode -FixedSize -WindowSize 129x97 -Frames 120 -Capture -LogName "prompt25-$shape-$mode"
        }
    }
    foreach($view in @('specular-candidate','specular-confidence','diffuse-anchor','old-specular','new-specular','specular-delta','specular-clip','specular-difference','specular-protected')){
        & ./tools/Run-Smoke.ps1 -Package generated/scene24-final-indoor -WorkMode image -ImageView $view -ImageSmoke specular-move -Frames 120 -Capture -LogName "prompt25-$view"
    }
    foreach($mode in @('views','cycle','transaction','ratio-drag','lighting-source')){
        & ./tools/Run-Smoke.ps1 -Package generated/scene24-final-indoor -WorkMode image -ImageView relighted -ImageSmoke $mode -Frames 120 -Capture -LogName "prompt25-$mode"
    }
    & ./tools/Run-Smoke.ps1 -Package generated/scene24-final-indoor -WorkMode image -ImageView relighted -ImageSmoke ratio-color -NoSpecular -Frames 120 -Capture -LogName prompt25-disabled-baseline
    & ./tools/Run-Smoke.ps1 -Package generated/prompt25-fixtures/sphere -WorkMode image -ImageView relighted -ImageSmoke specular-move -ProtectionMask generated/prompt25-fixtures/protect-all.png -Frames 120 -Capture -LogName prompt25-protection
    & ./tools/Run-Smoke.ps1 -Package generated/prompt25-fixtures/sphere -WorkMode image -ImageView new-specular -ImageSmoke specular-move -Warp -Frames 120 -Capture -LogName prompt25-warp
    & ./tools/Run-Smoke.ps1 -Package generated/scene24-final-indoor -WorkMode image -ImageView specular-confidence -UI -Frames 120 -Capture -LogName prompt25-ui
    & ./tools/Run-Smoke.ps1 -Package generated/prompt25-fixtures/sphere -WorkMode image -ImageView relighted -ImageSmoke specular-move -Configuration Release -Frames 120 -Capture -LogName prompt25-release
    & ./tools/Run-Smoke.ps1 -Package assets/ScenePackage -Frames 120 -Capture -LogName prompt25-final
    & ./tools/Run-Smoke.ps1 -Package generated/scene24-final-indoor -WorkMode image -ImageView relighted -ImageSmoke specular-move -ProtectionMask generated/prompt25-fixtures/real-protection.png -Frames 120 -Capture -LogName prompt25-real-protected
    & ./tools/Run-ReconstructionSmoke.ps1 -Package generated/scene24-final-indoor -Image generated/scene24-final-indoor -LightingOnly -LightingBackend intrinsic-assisted -WorkMode image -ImageView relighted -Repeat 2 -LogName prompt25-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -Package generated/scene24-final-indoor -Image generated/missing25 -LightingOnly -WorkMode image -ImageView original -ExpectError -LogName prompt25-failed
}finally{Pop-Location}
