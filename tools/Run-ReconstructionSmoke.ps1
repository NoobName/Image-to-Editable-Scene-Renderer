param(
    [Parameter(Mandatory=$true)][string]$Image,
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateSet('full','dummy')][string]$Preset='dummy',
    [string]$Python,
    [string]$Package,
    [string]$NextImage,
    [ValidateSet('scene','image')][string]$WorkMode='scene',
    [ValidateSet('original','depth','geometry-normal','position','validity','region','albedo','roughness','metallic','shading-proxy','old-shading','lighting-residual','fit-mask')][string]$ImageView='original',
    [ValidatePattern('^[a-zA-Z0-9][a-zA-Z0-9_.-]*$')][string]$LogName='prompt14-dummy',
    [switch]$Warp,
    [switch]$UI,
    [switch]$LightingOnly,
    [switch]$ExpectError,
    [ValidateSet('original','final','estimated-albedo','estimated-normal','estimated-roughness')][string]$RenderMode='original',
    [uint32]$CancelFrame=0,
    [ValidateRange(1,10)][int]$Repeat=1
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$executable=Join-Path $projectRoot "build/$Configuration/ImageSceneRenderer.exe"
if($Image.Contains('"') -or $Python.Contains('"')){throw 'A Windows filename cannot contain a double quote.'}
$entry=if($LightingOnly){'--fit-lighting'}else{'--reconstruct'}
$arguments=$entry+' "'+$Image+'" --reconstruction-preset '+$Preset+' --frames 90 --smoke --render-mode '+$RenderMode
$arguments+=' --log generated/'+$LogName+'.log --capture generated/'+$LogName+'.bmp'
if($Python){$arguments+=' --reconstruction-python "'+$Python+'"'}
if($Package){if($Package.Contains('"')){throw 'Invalid package path'};$arguments+=' --package "'+$Package+'"'}
if($NextImage){if($NextImage.Contains('"')){throw 'Invalid next image path'};$arguments+=' --reconstruction-next-image "'+$NextImage+'"'}
$arguments+=' --work-mode '+$WorkMode
$arguments+=' --image-view '+$ImageView
if($Warp){$arguments+=' --warp'}
if($UI){$arguments+=' --ui'}
if($CancelFrame){$arguments+=' --reconstruction-cancel-frame '+$CancelFrame}
$arguments+=' --reconstruction-repeat '+$Repeat
$process=Start-Process -FilePath $executable -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
Write-Output "Reconstruction smoke PID=$($process.Id)"
while(!$process.WaitForExit(1000)){}
$log=Get-Content -LiteralPath (Join-Path $projectRoot "generated/$LogName.log") -Raw
Write-Output $log
$expected=if($ExpectError){2}else{0}
if($process.ExitCode -ne $expected){throw "Expected exit $expected, received $($process.ExitCode)"}
if($Configuration -eq 'Debug' -and $log -notmatch 'Validation summary: errors=0 warnings=0'){throw 'DX12 validation was not clean'}
if(!$ExpectError -and $log -notmatch 'Reconstruction Ready:'){throw 'The generated scene was not loaded'}
if($WorkMode -eq 'image'){
    if($log -notmatch 'Source image output: RGBA8 UNORM'){throw 'Missing independent source capture'}
}elseif($log -notmatch 'nonfinite=0'){throw 'Missing finite HDR capture'}
