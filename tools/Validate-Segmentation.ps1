param([string]$Package='generated/scene12',[string]$Python='python',[string]$ObjectId='chair',
      [string]$TrafficPackage='generated/scene12-traffic')
$ErrorActionPreference='Stop'
# Fixed original-camera captures. Existing generated examples are inputs; no inference or downloads.
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $cases=@(
        @{Frames=35;LogName='prompt12-base35';RenderMode='albedo'},
        @{Frames=35;LogName='prompt12-hide35';RenderMode='albedo';ObjectSmoke=$ObjectId},
        @{Frames=65;LogName='prompt12-base65';RenderMode='albedo'},
        @{Frames=65;LogName='prompt12-restore65';RenderMode='albedo';ObjectSmoke=$ObjectId},
        @{Frames=90;LogName='prompt12-base90';RenderMode='albedo'},
        @{Frames=90;LogName='prompt12-edit90';RenderMode='albedo';ObjectSmoke=$ObjectId},
        @{Frames=90;LogName='prompt12-rough-base';RenderMode='roughness'},
        @{Frames=90;LogName='prompt12-rough-edit';RenderMode='roughness';ObjectSmoke=$ObjectId},
        @{Frames=90;LogName='prompt12-indoor-ui';RenderMode='albedo';UI=$true},
        @{Frames=90;LogName='prompt12-release';RenderMode='final';Configuration='Release'},
        @{Frames=90;LogName='prompt12-warp';RenderMode='normal';Warp=$true})
    foreach($case in $cases){ & "$PSScriptRoot/Run-Smoke.ps1" -Package $Package -Capture @case }
    & "$PSScriptRoot/Run-Smoke.ps1" -Package $TrafficPackage -Capture -Frames 90 -UI -RenderMode albedo -LogName prompt12-traffic-ui
    & $Python tools/reconstruction/check_object_captures.py $Package --object-id $ObjectId
    if($LASTEXITCODE -ne 0){throw 'Object image comparison failed.'}
} finally { Pop-Location }
