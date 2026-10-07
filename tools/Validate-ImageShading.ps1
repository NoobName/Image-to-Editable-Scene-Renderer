param([string]$Package='generated/scene19-final')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try{
    & ./build/Debug/ImageShadingTests.exe generated/prompt20-hardware --hardware
    if($LASTEXITCODE){throw 'Hardware numeric test failed'}
    foreach($view in @('original','calculated-old','calculated-new','shading-difference','normal-light-dot','shading-validity')){
        & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView $view -FixedSize -WindowSize 512x341 -Frames 80 -Capture -LogName "prompt20-$view"
    }
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-new -ImageSmoke lighting-target -Frames 90 -Capture -LogName prompt20-target
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-old -ImageSmoke lighting-source -Frames 90 -Capture -LogName prompt20-source
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-new -ImageSmoke views -Warp -Frames 100 -Capture -LogName prompt20-warp
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-new -ImageSmoke cycle -Frames 90 -Capture -LogName prompt20-cycle
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-new -ImageSmoke transaction -Frames 90 -Capture -LogName prompt20-transaction
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-new -WindowSize 1500x500 -FixedSize -Frames 90 -Capture -LogName prompt20-wide
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-new -WindowSize 500x900 -FixedSize -Frames 90 -Capture -LogName prompt20-narrow
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-new -UI -Frames 90 -Capture -LogName prompt20-ui
    & ./tools/Run-Smoke.ps1 -Package $Package -WorkMode image -ImageView calculated-new -Configuration Release -Frames 90 -Capture -LogName prompt20-release
    & ./tools/Run-Smoke.ps1 -Package generated/scene18 -WorkMode image -ImageView calculated-new -Frames 90 -Capture -LogName prompt20-unavailable
    & ./tools/Run-Smoke.ps1 -Package assets/ScenePackage -Frames 90 -Capture -LogName prompt20-final
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image generated/scene18 -LightingOnly -WorkMode image -ImageView calculated-new -Repeat 2 -LogName prompt20-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -Package $Package -Image generated/missing20 -LightingOnly -WorkMode image -ImageView calculated-new -ExpectError -LogName prompt20-failed
}finally{Pop-Location}
