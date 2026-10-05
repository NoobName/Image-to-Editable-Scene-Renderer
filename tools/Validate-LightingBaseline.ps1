param([string]$Package='generated/scene19-final',[string]$Fixtures='generated/prompt19-final')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try{
    foreach($view in @('original','shading-proxy','old-shading','lighting-residual','fit-mask')){
        & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView $view -FixedSize -WindowSize 512x341 -Frames 90 -Capture -LogName "prompt19-real-$view"
    }
    & "$PSScriptRoot/Run-Smoke.ps1" -Package assets/ScenePackage -Capture -Frames 90 -LogName prompt19-after-final
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -Capture -Frames 90 -LogName prompt19-after-source
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -ImageSmoke lighting-target -Capture -Frames 90 -LogName prompt19-target
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -ImageSmoke lighting-source -Capture -Frames 90 -LogName prompt19-source-invalidated
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -ImageSmoke transaction -Capture -Frames 90 -LogName prompt19-transaction
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -ImageSmoke views -Warp -Capture -Frames 90 -LogName prompt19-warp-views
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -ImageSmoke cycle -Capture -Frames 90 -LogName prompt19-cycle
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView lighting-residual -UI -Capture -Frames 90 -LogName prompt19-ui
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView shading-proxy -FixedSize -WindowSize 1500x500 -Capture -Frames 90 -LogName prompt19-wide
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView shading-proxy -FixedSize -WindowSize 500x900 -Capture -Frames 90 -LogName prompt19-narrow
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -Configuration Release -Capture -Frames 90 -LogName prompt19-release
    & "$PSScriptRoot/Run-Smoke.ps1" -Package generated/scene18 -WorkMode image -ImageView old-shading -UI -Capture -Frames 90 -LogName prompt19-unavailable
    foreach($kind in @('lambert','plane','black','overexposed','low-validity','neutral')){
        & "$PSScriptRoot/Run-Smoke.ps1" -Package "$Fixtures/$kind" -WorkMode image -ImageView old-shading -FixedSize -WindowSize 388x260 -Capture -Frames 90 -LogName "prompt19-synthetic-$kind"
    }
    & "$PSScriptRoot/Run-Smoke.ps1" -Package "$Fixtures/lambert" -WorkMode image -ImageView old-shading -FixedSize -WindowSize 388x260 -Warp -Capture -Frames 90 -LogName prompt19-synthetic-warp
    # Same worker/process/Prepare/Commit path as the menu, with two offline package jobs.
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -Image generated/scene18 -LightingOnly -Repeat 2 -UI -LogName prompt19-reload
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -Image generated/not-present-package -LightingOnly -ExpectError -LogName prompt19-failed
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -Image generated/scene18 -LightingOnly -CancelFrame 10 -ExpectError -LogName prompt19-cancelled
    & "$PSScriptRoot/Run-ReconstructionSmoke.ps1" -Package $Package -WorkMode image -ImageView old-shading -Image "$Fixtures/lambert.png" -Preset dummy -Repeat 2 -LogName prompt19-reconstruct
}finally{Pop-Location}
