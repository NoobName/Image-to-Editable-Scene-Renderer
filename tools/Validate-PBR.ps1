$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$fixture='assets/models/MaterialLab/MaterialLab.gltf'
function RunCase($name,$extra) {
    & "$PSScriptRoot/Run-Smoke.ps1" -Model $fixture -Frames 90 -Capture -LogName "pbr-$name" @extra | Out-File (Join-Path $root "generated/pbr-$name-console.txt")
    Write-Output "PASS: $name"
}
foreach($mode in @('final','albedo','normal','roughness','metallic','depth')) { RunCase $mode @{RenderMode=$mode} }
RunCase 'release' @{Configuration='Release'}
RunCase 'reversed' @{ReverseOrder=$true}
RunCase 'camera' @{CameraSmoke=$true}
RunCase 'exposure' @{Exposure=2}
RunCase 'edit' @{MaterialSmoke=$true}
RunCase 'directional' @{Lights='directional';Ambient=0}
RunCase 'point' @{Lights='point';Ambient=0}
RunCase 'none' @{Lights='none';Ambient=0}
RunCase 'ui' @{UI=$true}
RunCase 'warp' @{Warp=$true}
& "$PSScriptRoot/Run-Smoke.ps1" -Model 'assets/models/MaterialLab/MaterialLab.glb' -Frames 90 -Capture -LogName 'pbr-glb' | Out-File (Join-Path $root 'generated/pbr-glb-console.txt')
function Hash($name){(Get-FileHash (Join-Path $root "generated/pbr-$name.bmp")).Hash}
$reference=Hash 'final'
if($reference -ne (Hash 'reversed')){throw 'Opaque draw order changed the image.'}
if($reference -ne (Hash 'glb')){throw 'GLB and glTF rendered differently.'}
foreach($name in @('camera','exposure','edit','ui')){if($reference -eq (Hash $name)){throw "No output change for $name"}}
$modeHashes=@('final','albedo','normal','roughness','metallic','depth') | ForEach-Object {Hash $_}
if(($modeHashes | Select-Object -Unique).Count -ne 6){throw 'Render modes are not distinct.'}
foreach($name in @('directional','point')){if((Hash $name) -eq (Hash 'none')){throw "$name light had no effect."}}
foreach($name in @('final','edit')){
    $log=Get-Content (Join-Path $root "generated/pbr-$name.log") -Raw
    if($log -notmatch 'HDR statistics: max=([0-9.]+).*nonfinite=0'){throw 'Missing finite HDR statistics.'}
    if([double]::Parse($Matches[1],[System.Globalization.CultureInfo]::InvariantCulture) -le 1){throw 'HDR values were clamped.'}
}
Write-Output 'PASS: 17 PBR runs; 6 distinct views; glTF/GLB equivalence; depth ordering; HDR; lights; live edit; exposure; camera; UI; WARP.'
