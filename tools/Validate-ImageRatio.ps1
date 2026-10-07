param([string]$Package='generated/scene19-final',[string]$Fixture='generated/prompt21-detail')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try{
    & ./build/Debug/ImageRatioTests.exe generated/prompt21-hardware --hardware
    if($LASTEXITCODE){throw 'Ratio numeric hardware test failed'}
    foreach($view in @('original','calculated-old','calculated-new','ratio','relighted','relit-difference')){
        & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView $view -ImageSmoke ratio-drag -FixedSize -WindowSize 512x341 -Frames 90 -Capture -LogName "prompt21-real-$view"
    }
    foreach($scenario in @('ratio-reset','ratio-zero','ratio-color','ratio-luminance','cycle','transaction','views')){
        & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -ImageSmoke $scenario -FixedSize -WindowSize 512x341 -Frames 100 -Capture -LogName "prompt21-$scenario"
    }
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -ImageSmoke ratio-drag -Warp -Frames 90 -Capture -LogName prompt21-warp
    & ./tools/Run-Smoke.ps1 -Package $Fixture -WorkMode image -ImageView relighted -FixedSize -WindowSize 1500x1000 -Frames 90 -Capture -LogName prompt21-detail-identity
    & ./tools/Run-Smoke.ps1 -Package $Fixture -WorkMode image -ImageView relighted -ImageSmoke ratio-drag -FixedSize -WindowSize 1500x1000 -Frames 90 -Capture -LogName prompt21-detail-changed
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -ImageSmoke ratio-drag -UI -Frames 90 -Capture -LogName prompt21-ui
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -WindowSize 500x900 -FixedSize -Frames 90 -Capture -LogName prompt21-narrow
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView relighted -Configuration Release -Frames 90 -Capture -LogName prompt21-release
    & ./tools/Run-Smoke.ps1 -Package assets/ScenePackage -Frames 90 -Capture -LogName prompt21-final
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image generated/scene18 -LightingOnly -WorkMode image -ImageView relighted -Repeat 2 -LogName prompt21-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image generated/missing21 -LightingOnly -WorkMode image -ImageView relighted -ExpectError -LogName prompt21-failed
}finally{Pop-Location}
