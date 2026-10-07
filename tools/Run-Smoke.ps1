param(
    [ValidatePattern('^(build|generated/[a-zA-Z0-9][a-zA-Z0-9_.-]*)$')][string]$BuildDirectory='build',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('clear','triangle','scene')][string]$Demo = 'scene',
    [ValidateRange(1,1000000)][int]$Frames = 120,
    [ValidatePattern('^[a-zA-Z0-9][a-zA-Z0-9_.-]*$')][string]$LogName = 'smoke',
    [switch]$Interactive,
    [switch]$Warp,
    [switch]$Capture,
    [switch]$Profile,
    [ValidateSet('scene','image')][string]$WorkMode='scene',
    [ValidateSet('original','grid','depth','geometry-normal','world-normal','position','validity','region','albedo','roughness','metallic','geometry-confidence','material-confidence','region-confidence','tangent-normal','shading-proxy','old-shading','lighting-residual','fit-mask','calculated-old','calculated-new','shading-difference','normal-light-dot','shading-validity','ratio','relighted','relit-difference','geometry-weight','boundary-weight','material-weight','fit-weight','signal-weight','shadow-risk-weight','relighting-confidence','raw-log-ratio','effective-log-ratio','clamp-mask','protection','intrinsic-albedo','intrinsic-shading','intrinsic-residual','intrinsic-uncertainty','intrinsic-error','intrinsic-validity','diffuse-support','protected-residual','specular-candidate','specular-confidence','diffuse-anchor','old-specular','new-specular','specular-delta','specular-clip','specular-difference','specular-protected','shadow-candidate','shadow-visibility','shadow-geometry','shadow-confidence','shadow-unknown','shadow-manual-confirm','shadow-manual-protect','shadow-effective','shadow-overlay','cast-old-map','cast-new-map','cast-old-visibility','cast-new-visibility','cast-old-estimate','cast-confidence','cast-change','cast-difference','cast-final','cast-baseline','fog-distance','fog-transmittance','fog-confidence','fog-airlight')][string]$ImageView='original',
    [ValidateSet('cycle','input','transaction','views','lighting-target','lighting-source','profile-drag','ratio-drag','ratio-reset','ratio-zero','ratio-color','ratio-luminance','stability-baseline','stability-opposite','stability-preset','specular-move','specular-off','specular-rough','specular-reset','specular-invalid','shadow-move','shadow-off','shadow-zero','shadow-confidence-zero','shadow-reset','shadow-direct-off','shadow-pcf','shadow-scene-edit','workspace-side','workspace-wipe','workspace-zoom','workspace-protect','workspace-clear','workspace-exposure','workspace-overlay','workspace-global','fog-off','fog-zero','fog-reset','fog-drag')][string]$ImageSmoke,
    [ValidatePattern('^[0-9]+x[0-9]+$')][string]$WindowSize='1280x720',
    [switch]$FixedSize,
    [ValidateRange(0,1000)][float]$ImageFogDensity=0,
    [switch]$ImageFogRelative,
    [switch]$NoSpecular,
    [switch]$ReverseOrder,
    [switch]$CameraSmoke,
    [string]$Model,
    [string]$Package,
    [string]$ProtectionMask,
    [string]$Recipe,
    [string]$SaveRecipe,
    [string]$ExportImage,
    [string]$RecipeReload,
    [switch]$RecipeCancel,
    [switch]$ReplaceRecipe,
    [string]$ReferenceInput,
    [string]$ReferenceProposal,
    [ValidateSet('same-scene','different-content')][string]$ReferenceRelation='different-content',
    [switch]$ApplyReference,
    [switch]$ResetReference,
    [switch]$ReferenceDummy,
    [int]$ReferenceCancelFrame=0,
    [ValidateRange(1,5)][int]$ReferenceRepeat=1,
    [string]$OptimizeReference,
    [switch]$OptimizationRegistered,
    [ValidateRange(1,200)][int]$OptimizationIterations=100,
    [int]$OptimizationEditFrame=0,
    [ValidateSet('final','albedo','normal','roughness','metallic','depth','wireframe','original','estimated-albedo','estimated-normal','estimated-roughness')][string]$RenderMode='final',
    [float]$Exposure=0,
    [ValidateSet('none','reinhard','aces')][string]$ToneMapping='aces',
    [ValidateRange(0.1,4)][float]$Gamma=1,
    [ValidateRange(-1,1)][float]$Temperature=0,
    [ValidateRange(-1,1)][float]$Tint=0,
    [ValidateRange(0,2)][float]$Saturation=1,
    [ValidateRange(0.25,2)][float]$Contrast=1,
    [switch]$Bloom,
    [ValidateRange(0,2)][float]$BloomIntensity=0.08,
    [ValidateRange(0,20)][float]$BloomThreshold=1,
    [ValidateRange(0,1)][float]$BloomKnee=0.5,
    [ValidateRange(0.5,2)][float]$BloomRadius=1,
    [ValidateRange(0,1)][float]$Vignette=0,
    [ValidateRange(0,1)][float]$VignetteRadius=0.35,
    [ValidateRange(0.01,1)][float]$VignetteSoftness=0.65,
    [switch]$LookSmoke,
    [float]$Ambient=0,
    [ValidateSet('all','directional','point','none')][string]$Lights='all',
    [switch]$MaterialSmoke,
    [string]$ObjectSmoke,
    [ValidateSet('baseline','move','rotate','scale','material','roughness','metallic','normal','sun','light','environment','exposure','restore-material','restore-transform')][string]$EditorSmoke,
    [ValidatePattern('^[a-zA-Z0-9_-]{1,64}$')][string]$EditorObject,
    [switch]$UI,
    [string]$Environment,
    [switch]$NoIBL,
    [switch]$NoSky,
    [switch]$NoShadows,
    [ValidateRange(0,2)][int]$ShadowPcf=1,
    [float]$ShadowBias=0.0005,
    [float]$NormalBias=0.02,
    [float]$EnvironmentIntensity=1,
    [float]$EnvironmentRotation=0,
    [switch]$EnvironmentSmoke,
    [switch]$LightingTest
)
$ErrorActionPreference = 'Stop'
if ($Interactive -and $Capture) { throw 'Capture requires a finite smoke run; omit -Interactive.' }
if ($Model -and $Package) { throw 'Use either -Model or -Package.' }
$projectRoot = Split-Path $PSScriptRoot -Parent
$executable = Join-Path $projectRoot "$BuildDirectory/$Configuration/ImageSceneRenderer.exe"
$arguments = "--log generated/$LogName.log"
$arguments += " --demo $Demo"
if($Profile){$arguments += " --profile generated/$LogName.profile.json"}
$arguments += " --work-mode $WorkMode --image-view $ImageView --window-size $WindowSize"
if($FixedSize){$arguments += ' --fixed-size'}
if($PSBoundParameters.ContainsKey('ImageFogDensity')){$arguments += ' --image-fog-density '+$ImageFogDensity.ToString([System.Globalization.CultureInfo]::InvariantCulture)}
if($ImageFogRelative){$arguments += ' --image-fog-relative'}
if($NoSpecular){$arguments += ' --no-specular'}
if($ImageSmoke){$arguments += " --image-smoke $ImageSmoke"}
$arguments += " --render-mode $RenderMode --ambient $($Ambient.ToString([System.Globalization.CultureInfo]::InvariantCulture)) --lights $Lights"
if (!$Package -or $PSBoundParameters.ContainsKey('Exposure')) { $arguments += " --exposure $($Exposure.ToString([System.Globalization.CultureInfo]::InvariantCulture))" }
if ($MaterialSmoke) { $arguments += ' --material-smoke' }
if ($ObjectSmoke) {
    if ($ObjectSmoke -notmatch '^[a-zA-Z0-9_-]{1,64}$') { throw 'Object smoke requires a safe stable object ID.' }
    $arguments += " --object-smoke $ObjectSmoke"
}
if ($UI) { $arguments += ' --ui' }
if ($EditorSmoke) { $arguments += " --editor-smoke $EditorSmoke" }
if ($EditorObject) { $arguments += " --editor-object $EditorObject" }
if ($NoIBL) { $arguments += ' --no-ibl' }
if ($NoSky) { $arguments += ' --no-sky' }
if ($NoShadows) { $arguments += ' --no-shadows' }
if ($EnvironmentSmoke) { $arguments += ' --environment-smoke' }
if ($LightingTest) { $arguments += ' --lighting-test' }
if ($Bloom) { $arguments += ' --bloom' }
if ($LookSmoke) { $arguments += ' --look-smoke' }
$culture=[System.Globalization.CultureInfo]::InvariantCulture
if (!$Package -or $PSBoundParameters.ContainsKey('ToneMapping')) { $arguments += " --tone-mapping $ToneMapping" }
if (!$Package -or $PSBoundParameters.ContainsKey('Gamma')) { $arguments += " --gamma $($Gamma.ToString($culture))" }
$lookValues=@{'temperature'=$Temperature;'tint'=$Tint;'saturation'=$Saturation;'contrast'=$Contrast;
    'bloom-intensity'=$BloomIntensity;'bloom-threshold'=$BloomThreshold;'bloom-knee'=$BloomKnee;'bloom-radius'=$BloomRadius;
    'vignette'=$Vignette;'vignette-radius'=$VignetteRadius;'vignette-softness'=$VignetteSoftness}
$lookNames=@{'temperature'='Temperature';'tint'='Tint';'saturation'='Saturation';'contrast'='Contrast';
    'bloom-intensity'='BloomIntensity';'bloom-threshold'='BloomThreshold';'bloom-knee'='BloomKnee';'bloom-radius'='BloomRadius';
    'vignette'='Vignette';'vignette-radius'='VignetteRadius';'vignette-softness'='VignetteSoftness'}
foreach($key in $lookValues.Keys){if (!$Package -or $PSBoundParameters.ContainsKey($lookNames[$key])) {$arguments += " --$key $($lookValues[$key].ToString($culture))"}}
$arguments += " --shadow-pcf $ShadowPcf --shadow-bias $($ShadowBias.ToString($culture)) --normal-bias $($NormalBias.ToString($culture))"
if (!$Package -or $PSBoundParameters.ContainsKey('EnvironmentIntensity')) { $arguments += " --env-intensity $($EnvironmentIntensity.ToString($culture))" }
if (!$Package -or $PSBoundParameters.ContainsKey('EnvironmentRotation')) { $arguments += " --env-rotation $($EnvironmentRotation.ToString($culture))" }
if ($Environment) {
    if ($Environment.Contains('"')) { throw 'Environment path cannot contain quotes.' }
    $arguments += ' --env "' + $Environment + '"'
}
if (!$Interactive) { $arguments += " --frames $Frames --smoke" }
if ($Warp) { $arguments += ' --warp' }
if ($Capture) { $arguments += " --capture generated/$LogName.bmp" }
if ($ReverseOrder) { $arguments += ' --reverse-order' }
if ($CameraSmoke) { $arguments += ' --camera-smoke' }
if ($Model) {
    if ($Model.Contains('"')) { throw 'Model path cannot contain quotes.' }
    $arguments += ' --model "' + $Model + '"'
}
if ($ProtectionMask) {
    if ($ProtectionMask.Contains('"')) { throw 'Invalid protection path' }
    $arguments += ' --protection-mask "' + $ProtectionMask + '"'
}
if ($Package) {
    if ($Package.Contains('"')) { throw 'Package path cannot contain quotes.' }
    $arguments += ' --package "' + $Package + '"'
}
$windowStyle = if ($Interactive) { 'Normal' } else { 'Hidden' }
foreach($pair in @(@('recipe',$Recipe),@('save-recipe',$SaveRecipe),@('export-image',$ExportImage),@('recipe-reload',$RecipeReload))){
    if($pair[1]){if($pair[1].Contains('"')){throw 'Recipe/export path cannot contain quotes.'};$arguments += ' --'+$pair[0]+' "'+$pair[1]+'"'}
}
if($RecipeCancel){$arguments += ' --recipe-cancel'}
if($ReplaceRecipe){$arguments += ' --replace-recipe'}
foreach($pair in @(@('reference-input',$ReferenceInput),@('reference-proposal',$ReferenceProposal))){
    if($pair[1]){if($pair[1].Contains('"')){throw 'Invalid reference path'};$arguments += ' --'+$pair[0]+' "'+$pair[1]+'"'}
}
if($ReferenceInput -or $ReferenceProposal){$arguments += " --reference-relation $ReferenceRelation --reference-repeat $ReferenceRepeat"}
if($ApplyReference){$arguments += ' --apply-reference'}
if($ResetReference){$arguments += ' --reset-reference'}
if($ReferenceDummy){$arguments += ' --reference-dummy'}
if($ReferenceCancelFrame -gt 0){$arguments += " --reference-cancel-frame $ReferenceCancelFrame"}
if($OptimizeReference){if($OptimizeReference.Contains('"')){throw 'Invalid optimization path'};$arguments += ' --optimize-reference "'+$OptimizeReference+'" --optimization-iterations '+$OptimizationIterations}
if($OptimizationRegistered){$arguments += ' --optimization-registered'}
if($OptimizationEditFrame -gt 0){$arguments += " --optimization-edit-frame $OptimizationEditFrame"}
$process = Start-Process -FilePath $executable -WorkingDirectory $projectRoot -ArgumentList $arguments -WindowStyle $windowStyle -PassThru
if ($Interactive) { Write-Output "Started PID=$($process.Id)"; exit 0 }
if (!$process.WaitForExit(60000)) { throw 'Smoke process did not finish within 60 seconds. Inspect the application.' }
$logText = Get-Content (Join-Path $projectRoot "generated/$LogName.log") -Raw
Write-Output $logText
if ($process.ExitCode -ne 0) { throw "Application failed: $($process.ExitCode)" }
if ($Configuration -eq 'Debug' -and $logText -notmatch 'Validation summary: errors=0 warnings=0') {
    throw 'Debug validation was not clean. Inspect the log.'
}
