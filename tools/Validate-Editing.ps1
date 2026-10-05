param([string]$Package='generated/scene13',[string]$ObjectId='chair')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $common=@{Package=$Package;EditorObject=$ObjectId;Frames=90;Capture=$true}
    foreach($mode in @('baseline','move','rotate','scale','material','sun','light','environment','exposure','restore-material','restore-transform')) {
        & "$PSScriptRoot/Run-Smoke.ps1" @common -EditorSmoke $mode -LogName "prompt15-$mode" > "generated/prompt15-$mode-run.txt"
        Write-Output "PASS runtime edit: $mode"
    }
    foreach($mode in @('albedo','roughness','metallic','estimated-roughness')) {
        & "$PSScriptRoot/Run-Smoke.ps1" @common -RenderMode $mode -EditorSmoke baseline -LogName "prompt15-$mode-base" > "generated/prompt15-$mode-base-run.txt"
        $edit=if($mode -in @('roughness','metallic')){$mode}else{'material'}
        & "$PSScriptRoot/Run-Smoke.ps1" @common -RenderMode $mode -EditorSmoke $edit -LogName "prompt15-$mode-edit" > "generated/prompt15-$mode-edit-run.txt"
        Write-Output "PASS data view edit: $mode"
    }
    & "$PSScriptRoot/Run-Smoke.ps1" @common -EditorSmoke baseline -UI -LogName prompt15-ui > generated/prompt15-ui-run.txt
    & "$PSScriptRoot/Run-Smoke.ps1" @common -EditorSmoke material -Configuration Release -LogName prompt15-release > generated/prompt15-release-run.txt
    & "$PSScriptRoot/Run-Smoke.ps1" @common -EditorSmoke material -Warp -LogName prompt15-warp > generated/prompt15-warp-run.txt
    foreach($mode in @('baseline','normal')) {
        & "$PSScriptRoot/Run-Smoke.ps1" -Model assets/models/MaterialLab/MaterialLab.gltf -EditorSmoke $mode -RenderMode normal -Frames 90 -Capture -LogName "prompt15-lab-$mode" > "generated/prompt15-lab-$mode-run.txt"
    }
    Write-Output 'PASS: 24 window runs; next run tools/reconstruction/check_editing_captures.py for pixel assertions.'
} finally { Pop-Location }
