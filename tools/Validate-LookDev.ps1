$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
function RunCase($name,$extra){
    & "$PSScriptRoot/Run-Smoke.ps1" -Frames 90 -Capture -LogName "lookdev-$name" @extra |
        Out-File (Join-Path $projectRoot "generated/lookdev-$name-console.txt")
    Write-Output "PASS: $name"
}
foreach($tone in @('none','reinhard','aces')){RunCase $tone @{ToneMapping=$tone}}
RunCase 'gamma' @{Gamma=2}
RunCase 'exposure' @{Exposure=2}
RunCase 'normal' @{RenderMode='normal'}
RunCase 'normal-display' @{RenderMode='normal';ToneMapping='reinhard';Gamma=2;Exposure=2}
RunCase 'albedo' @{RenderMode='albedo'}
RunCase 'albedo-display' @{RenderMode='albedo';ToneMapping='none';Gamma=2;Exposure=2}
RunCase 'ui' @{UI=$true}
RunCase 'ui-release' @{UI=$true;Configuration='Release'}
RunCase 'ui-warp' @{UI=$true;Warp=$true}
RunCase 'ui-gltf' @{UI=$true;Model='assets/models/MaterialLab/MaterialLab.gltf'}
RunCase 'ui-glb' @{UI=$true;Model='assets/models/MaterialLab/MaterialLab.glb'}
RunCase 'ui-environment' @{UI=$true;EnvironmentSmoke=$true}
function Hash($name){(Get-FileHash (Join-Path $projectRoot "generated/lookdev-$name.bmp")).Hash}
if((@('none','reinhard','aces','gamma','exposure')|ForEach-Object {Hash $_}|Select-Object -Unique).Count -ne 5){throw 'Tone/exposure/gamma did not produce distinct Final output.'}
foreach($mode in @('normal','albedo')){if((Hash $mode) -ne (Hash "$mode-display")){throw "Display controls corrupted $mode data view."}}
foreach($name in @('ui','ui-release','ui-warp','ui-gltf','ui-glb','ui-environment')){
    $log=Get-Content (Join-Path $projectRoot "generated/lookdev-$name.log") -Raw
    if($log -notmatch 'Viewport resize: 689x628' -or $log -notmatch 'Viewport resize: 425x448' -or $log -notmatch 'Smoke: restored'){
        throw "Viewport resize/restore not exercised: $name"
    }
}
Write-Output 'PASS: 15 Look Development runs; tone/gamma/exposure; unaltered data views; UI viewport resize/restore; Debug/Release/WARP; glTF/GLB; environment switching.'
