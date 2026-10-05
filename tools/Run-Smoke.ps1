param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('clear','triangle','scene')][string]$Demo = 'scene',
    [ValidateRange(1,1000000)][int]$Frames = 120,
    [ValidatePattern('^[a-zA-Z0-9][a-zA-Z0-9_.-]*$')][string]$LogName = 'smoke',
    [switch]$Interactive,
    [switch]$Warp,
    [switch]$Capture,
    [switch]$ReverseOrder,
    [switch]$CameraSmoke,
    [string]$Model,
    [string]$Package,
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
$executable = Join-Path $projectRoot "build/$Configuration/ImageSceneRenderer.exe"
$arguments = "--log generated/$LogName.log"
$arguments += " --demo $Demo"
$arguments += " --render-mode $RenderMode --ambient $($Ambient.ToString([System.Globalization.CultureInfo]::InvariantCulture)) --lights $Lights"
if (!$Package -or $PSBoundParameters.ContainsKey('Exposure')) { $arguments += " --exposure $($Exposure.ToString([System.Globalization.CultureInfo]::InvariantCulture))" }
if ($MaterialSmoke) { $arguments += ' --material-smoke' }
if ($ObjectSmoke) {
    if ($ObjectSmoke -notmatch '^[a-zA-Z0-9_-]{1,64}$') { throw 'Object smoke requires a safe stable object ID.' }
    $arguments += " --object-smoke $ObjectSmoke"
}
if ($UI) { $arguments += ' --ui' }
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
if ($Package) {
    if ($Package.Contains('"')) { throw 'Package path cannot contain quotes.' }
    $arguments += ' --package "' + $Package + '"'
}
$process = Start-Process -FilePath $executable -WorkingDirectory $projectRoot -ArgumentList $arguments -PassThru
if ($Interactive) { Write-Output "Started PID=$($process.Id)"; exit 0 }
if (!$process.WaitForExit(60000)) { throw 'Smoke process did not finish within 60 seconds. Inspect the application.' }
$logText = Get-Content (Join-Path $projectRoot "generated/$LogName.log") -Raw
Write-Output $logText
if ($process.ExitCode -ne 0) { throw "Application failed: $($process.ExitCode)" }
if ($Configuration -eq 'Debug' -and $logText -notmatch 'Validation summary: errors=0 warnings=0') {
    throw 'Debug validation was not clean. Inspect the log.'
}
