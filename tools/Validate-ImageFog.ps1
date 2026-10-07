param([string]$BuildDirectory='build',[string]$Python='python',[string]$Inputs='generated/fog-inputs',
    [ValidatePattern('^generated/[a-zA-Z0-9][a-zA-Z0-9_.-]*$')][string]$Output='generated/fog-validation',
    [string]$RealPackage)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Choose a new output directory.'}
New-Item -ItemType Directory -Path $Output | Out-Null
if(!(Test-Path -LiteralPath $Inputs)){
    & $Python "$PSScriptRoot/reconstruction/fog_examples.py" $Inputs
    if($LASTEXITCODE -ne 0){throw 'Fog fixtures failed'}
}
$prefix=Split-Path $Output -Leaf
$cases=[System.Collections.Generic.List[object]]::new()
function Run([string]$Name,[string]$Package,[hashtable]$Extra){
    $a=@{BuildDirectory=$BuildDirectory;Frames=100;Capture=$true;WorkMode='image';ImageView='relighted';
        LogName="$prefix-$Name";ExportImage="$Output/$Name"}
    if($Package){$a.Package=$Package}
    foreach($key in $Extra.Keys){$a[$key]=$Extra[$key]}
    & "$PSScriptRoot/Run-Smoke.ps1" @a *> "$Output/$Name-run.log"
    $cases.Add(@{name=$Name;arguments=$a;status='pass'})
    $cases | ConvertTo-Json -Depth 8 | Set-Content "$Output/windows.json" -Encoding UTF8
}
foreach($kind in 'constant','gradient','plane','edge','relative','zero-confidence','missing'){
    Run $kind "$Inputs/$kind" @{ImageFogDensity=.3;SaveRecipe="$Output/$kind.json"}
}
Run 'relative-enabled' "$Inputs/relative" @{ImageFogDensity=.3;ImageFogRelative=$true}
Run 'off' "$Inputs/edge" @{ImageFogDensity=.3;ImageSmoke='fog-off'}
Run 'zero' "$Inputs/edge" @{ImageFogDensity=.3;ImageSmoke='fog-zero'}
Run 'reset' "$Inputs/edge" @{ImageFogDensity=.3;ImageSmoke='fog-reset'}
Run 'extreme' "$Inputs/edge" @{ImageFogDensity=1000}
Run 'protected' "$Inputs/edge" @{ImageFogDensity=.3;ImageSmoke='workspace-protect'}
Run 'reopen' '' @{Recipe="$Output/edge.json"}
Run 'cycle' '' @{Recipe="$Output/edge.json";ImageSmoke='cycle';UI=$true}
Run 'ui' '' @{Recipe="$Output/edge.json";UI=$true;WindowSize='1920x1080';FixedSize=$true}
Run 'reload' "$Inputs/constant" @{RecipeReload="$Output/edge.json";UI=$true}
Run 'warp' '' @{Recipe="$Output/edge.json";Warp=$true}
Run 'release' '' @{Recipe="$Output/edge.json";Configuration='Release'}
foreach($view in 'fog-distance','fog-transmittance','fog-confidence','fog-airlight'){
    Run $view "$Inputs/edge" @{ImageFogDensity=.3;ImageView=$view;UI=$true}
}
Run 'views' "$Inputs/edge" @{ImageFogDensity=.3;ImageSmoke='views';Frames=180;UI=$true}
if($RealPackage){
    Run 'real' $RealPackage @{ImageFogDensity=.15;UI=$true;SaveRecipe="$Output/real.json"}
    Run 'real-reopen' '' @{Recipe="$Output/real.json"}
    Run 'real-relit' $RealPackage @{ImageFogDensity=.15;ImageSmoke='shadow-move'}
    Run 'real-exposure' $RealPackage @{ImageFogDensity=.15;ImageSmoke='workspace-exposure'}
}
& $Python "$PSScriptRoot/reconstruction/verify_image_fog.py" $Output
if($LASTEXITCODE -ne 0){throw 'Fog numeric verification failed'}
