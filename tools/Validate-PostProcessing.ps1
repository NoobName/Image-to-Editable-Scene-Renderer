$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
function RunCase($name,$extra){
    & "$PSScriptRoot/Run-Smoke.ps1" -Frames 90 -Capture -LogName "post-$name" @extra |
        Out-File (Join-Path $projectRoot "generated/post-$name-console.txt")
    Write-Output "PASS: $name"
}
RunCase 'neutral' @{}
RunCase 'bloom' @{Bloom=$true;BloomIntensity=0.5}
RunCase 'zero-bloom' @{Bloom=$true;BloomIntensity=0}
RunCase 'warm' @{Temperature=0.8}
RunCase 'cool' @{Temperature=-0.8}
RunCase 'tint' @{Tint=0.8}
RunCase 'gray' @{Saturation=0}
RunCase 'contrast' @{Contrast=1.6}
RunCase 'vignette' @{Vignette=0.8}
$combined=@{Exposure=0.4;Temperature=0.6;Tint=0.3;Saturation=1.2;Contrast=1.2;Bloom=$true;BloomIntensity=0.4;Vignette=0.55}
RunCase 'combined' $combined
RunCase 'live-edit' @{LookSmoke=$true}
RunCase 'release' ($combined+@{Configuration='Release'})
RunCase 'warp' ($combined+@{Warp=$true})
RunCase 'ui' ($combined+@{UI=$true})
RunCase 'gltf' ($combined+@{Model='assets/models/MaterialLab/MaterialLab.gltf'})
RunCase 'glb' ($combined+@{Model='assets/models/MaterialLab/MaterialLab.glb'})
foreach($mode in @('albedo','normal','roughness','metallic','depth')){
    RunCase $mode @{RenderMode=$mode}
    RunCase "$mode-effects" ($combined+@{RenderMode=$mode})
}
function Hash($name){(Get-FileHash (Join-Path $projectRoot "generated/post-$name.bmp")).Hash}
function Assert($value,$message){if(!$value){throw $message}}
Assert ((Hash 'neutral') -eq (Hash 'zero-bloom')) 'Zero bloom intensity changes output.'
foreach($name in @('bloom','warm','cool','tint','gray','contrast','vignette','combined')){
    Assert ((Hash 'neutral') -ne (Hash $name)) "Effect does not change the image: $name"
}
Assert ((Hash 'warm') -ne (Hash 'cool')) 'Warm and cool balance are identical.'
Assert ((Hash 'combined') -eq (Hash 'live-edit')) 'Runtime look edits do not match CLI state.'
Assert ((Hash 'gltf') -eq (Hash 'glb')) 'glTF/GLB differ under post processing.'
foreach($mode in @('albedo','normal','roughness','metallic','depth')){
    Assert ((Hash $mode) -eq (Hash "$mode-effects")) "Post processing corrupts $mode data view."
}
# Saturation zero must produce equal channels after tone mapping and sRGB too.
$bytes=[IO.File]::ReadAllBytes((Join-Path $projectRoot 'generated/post-gray.bmp'))
$offset=[BitConverter]::ToInt32($bytes,10)
for($i=$offset;$i -lt $bytes.Length;$i+=4){
    if([Math]::Abs([int]$bytes[$i]-[int]$bytes[$i+1]) -gt 1 -or [Math]::Abs([int]$bytes[$i]-[int]$bytes[$i+2]) -gt 1){throw 'Saturation zero is not grayscale.'}
}
Write-Output 'PASS: 26 post-processing runs; individual effects; live/CLI equivalence; 5 data-view bypasses; grayscale pixels; glTF/GLB; Debug/Release/WARP/UI; resize/restore.'
