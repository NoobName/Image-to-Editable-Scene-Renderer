param([string]$Package='generated/scene19-final',[string]$Fixture='generated/prompt22-fixture')
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try{
    foreach($view in @('geometry-weight','boundary-weight','material-weight','fit-weight','signal-weight','shadow-risk-weight','relighting-confidence','raw-log-ratio','effective-log-ratio','clamp-mask','protection')){
        & ./tools/Run-Smoke.ps1 -Package $Fixture -WorkMode image -ImageView $view -ImageSmoke stability-preset -ProtectionMask "$Fixture.protect.png" -FixedSize -WindowSize 195x99 -Frames 90 -Capture -LogName "prompt22-$view"
    }
    foreach($test in @(@('baseline','stability-baseline'),@('preset','stability-preset'),@('opposite','stability-opposite'),@('drag','ratio-drag'),@('drag-repeat','ratio-drag'),@('reset','ratio-reset'),@('zero','ratio-zero'),@('color','ratio-color'),@('source','lighting-source'),@('cycle','cycle'),@('transaction','transaction'))){
        & ./tools/Run-Smoke.ps1 -Package $Fixture -WorkMode image -ImageView relighted -ImageSmoke $test[1] -FixedSize -WindowSize 195x99 -Frames 100 -Capture -LogName "prompt22-$($test[0])"
    }
    & ./tools/Run-Smoke.ps1 -Package $Fixture -WorkMode image -ImageView relighted -ImageSmoke stability-preset -ProtectionMask "$Fixture.all-protect.png" -FixedSize -WindowSize 195x99 -Frames 90 -Capture -LogName prompt22-protected
    & ./tools/Run-Smoke.ps1 -Package $Fixture -WorkMode image -ImageView relighted -ImageSmoke ratio-drag -Warp -Frames 100 -Capture -LogName prompt22-warp
    & ./tools/Run-Smoke.ps1 -Package $Fixture -WorkMode image -ImageView relighted -ImageSmoke stability-preset -Configuration Release -Frames 90 -Capture -LogName prompt22-release
    foreach($s in @('stability-baseline','stability-preset')){
        & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -ImageSmoke $s -FixedSize -WindowSize 512x341 -Frames 90 -Capture -LogName "prompt22-real-$s"
    }
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighting-confidence -ImageSmoke stability-preset -UI -Frames 90 -Capture -LogName prompt22-ui
    & ./tools/Run-Smoke.ps1 -Package assets/ScenePackage -Frames 90 -Capture -LogName prompt22-final
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image generated/scene18 -LightingOnly -WorkMode image -ImageView relighted -Repeat 2 -LogName prompt22-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image generated/missing22 -LightingOnly -WorkMode image -ImageView relighted -ExpectError -LogName prompt22-failed
}finally{Pop-Location}
