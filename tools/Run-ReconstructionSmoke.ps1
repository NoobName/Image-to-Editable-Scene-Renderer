param(
    [ValidatePattern('^(build|generated/[a-zA-Z0-9][a-zA-Z0-9_.-]*)$')][string]$BuildDirectory='build',
    [Parameter(Mandatory=$true)][string]$Image,
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateSet('full','dummy')][string]$Preset='dummy',
    [string]$Python,
    [string]$Package,
    [string]$NextImage,
    [ValidateSet('scene','image')][string]$WorkMode='scene',
    [ValidateSet('original','depth','geometry-normal','position','validity','region','albedo','roughness','metallic','shading-proxy','old-shading','lighting-residual','fit-mask','calculated-old','calculated-new','shading-difference','normal-light-dot','shading-validity','ratio','relighted','relit-difference','geometry-weight','boundary-weight','material-weight','fit-weight','signal-weight','shadow-risk-weight','relighting-confidence','raw-log-ratio','effective-log-ratio','clamp-mask','protection','intrinsic-albedo','intrinsic-shading','intrinsic-residual','intrinsic-uncertainty','intrinsic-error','intrinsic-validity','diffuse-support','protected-residual','specular-candidate','specular-confidence','diffuse-anchor','old-specular','new-specular','specular-delta','specular-clip','specular-difference','specular-protected','shadow-candidate','shadow-visibility','shadow-geometry','shadow-confidence','shadow-unknown','shadow-manual-confirm','shadow-manual-protect','shadow-effective','shadow-overlay','cast-old-map','cast-new-map','cast-old-visibility','cast-new-visibility','cast-old-estimate','cast-confidence','cast-change','cast-difference','cast-final','cast-baseline')][string]$ImageView='original',
    [ValidatePattern('^[a-zA-Z0-9][a-zA-Z0-9_.-]*$')][string]$LogName='prompt14-dummy',
    [switch]$Warp,
    [switch]$UI,
    [switch]$LightingOnly,
    [ValidateSet('robust-directional-ambient','intrinsic-assisted','manual-test')][string]$LightingBackend='robust-directional-ambient',
    [switch]$IntrinsicOnly,
    [switch]$ShadowOnly,
    [ValidateSet('marigold-lighting','proxy','saved')][string]$IntrinsicBackend='proxy',
    [switch]$ExpectError,
    [ValidateSet('original','final','estimated-albedo','estimated-normal','estimated-roughness')][string]$RenderMode='original',
    [uint32]$CancelFrame=0,
    [ValidateRange(1,10)][int]$Repeat=1
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$executable=Join-Path $projectRoot "$BuildDirectory/$Configuration/ImageSceneRenderer.exe"
if($Image.Contains('"') -or $Python.Contains('"')){throw 'A Windows filename cannot contain a double quote.'}
if(([int]$LightingOnly.IsPresent+[int]$IntrinsicOnly.IsPresent+[int]$ShadowOnly.IsPresent) -gt 1){throw 'Choose one offline stage'}
$entry=if($ShadowOnly){'--estimate-shadows'}elseif($IntrinsicOnly){'--estimate-intrinsic'}elseif($LightingOnly){'--fit-lighting'}else{'--reconstruct'}
$arguments=$entry+' "'+$Image+'" --reconstruction-preset '+$Preset+' --frames 90 --smoke --render-mode '+$RenderMode
$arguments+=' --log generated/'+$LogName+'.log --capture generated/'+$LogName+'.bmp'
if($IntrinsicOnly){$arguments+=' --intrinsic-backend '+$IntrinsicBackend}
if($LightingOnly){$arguments+=' --lighting-backend '+$LightingBackend}
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
    if($log -notmatch '(Source image output|Image output): RGBA8 UNORM'){throw 'Missing independent source capture'}
}elseif($log -notmatch 'nonfinite=0'){throw 'Missing finite HDR capture'}
