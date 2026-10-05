param([string]$Python='python',[string]$Package='generated/scene13',
      [string]$FullPipelinePackage='generated/scene13-inference',[string]$ObjectId='chair')
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $fixture='generated/material13-validation-'+[guid]::NewGuid().ToString('N')
    & $Python tools/reconstruction/verify_material_rendering.py --make-fixture $fixture
    if($LASTEXITCODE -ne 0){throw 'Analytic material fixture export failed.'}
    $cases=@(
        @{LogName='prompt13-original';RenderMode='original'},
        @{LogName='prompt13-albedo';RenderMode='estimated-albedo'},
        @{LogName='prompt13-normal';RenderMode='estimated-normal'},
        @{LogName='prompt13-roughness';RenderMode='estimated-roughness'},
        @{LogName='prompt13-metallic';RenderMode='metallic'},
        @{LogName='prompt13-final';RenderMode='final'},
        @{LogName='prompt13-original-edit';RenderMode='original';ObjectSmoke=$ObjectId},
        @{LogName='prompt13-albedo-edit';RenderMode='estimated-albedo';ObjectSmoke=$ObjectId},
        @{LogName='prompt13-roughness-edit';RenderMode='estimated-roughness';ObjectSmoke=$ObjectId},
        @{LogName='prompt13-original-look';RenderMode='original';LookSmoke=$true},
        @{LogName='prompt13-original-ui';RenderMode='original';UI=$true},
        @{LogName='prompt13-albedo-ui';RenderMode='estimated-albedo';UI=$true},
        @{LogName='prompt13-release';RenderMode='final';Configuration='Release'},
        @{LogName='prompt13-warp';RenderMode='original';Warp=$true})
    foreach($case in $cases){ & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -Frames 90 -Capture @case }
    $probes=@{'original'='original';'albedo'='estimated-albedo';'normal'='estimated-normal';'roughness'='estimated-roughness';'metallic'='metallic'}
    foreach($key in $probes.Keys){ & "$PSScriptRoot/Run-Smoke.ps1" -Package $fixture -Frames 90 -Capture -RenderMode $probes[$key] -LogName "prompt13-probe-$key" }
    & "$PSScriptRoot/Run-Smoke.ps1" -Package generated/scene12 -Frames 90 -Capture -RenderMode albedo -LogName prompt13-legacy
    & "$PSScriptRoot/Run-Smoke.ps1" -LightingTest -NoSky -Lights none -Frames 90 -Capture -LogName prompt13-ibl-regression
    & "$PSScriptRoot/Run-Smoke.ps1" -LightingTest -NoSky -NoIBL -Lights directional -ShadowPcf 1 -Frames 90 -Capture -LogName prompt13-shadow-regression
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $FullPipelinePackage -Frames 90 -Capture -RenderMode estimated-roughness -LogName prompt13-full-pipeline
    & $Python tools/reconstruction/verify_material_rendering.py --package $Package
    if($LASTEXITCODE -ne 0){throw 'GPU material verification failed.'}
} finally { Pop-Location }
