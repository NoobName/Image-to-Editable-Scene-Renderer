param(
    [string]$BuildDirectory='build',
    [string]$Python='python',
    [ValidatePattern('^generated/[a-zA-Z0-9][a-zA-Z0-9_.-]*$')][string]$Output='generated/core-demo',
    [string]$Inputs,
    [string]$RealPackage
)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Choose a new output directory; previous evidence is never replaced.'}
New-Item -ItemType Directory -Path $Output | Out-Null
if(!$Inputs){
    $Inputs="$Output/inputs"
    & $Python "$PSScriptRoot/reconstruction/core_demo_inputs.py" --output $Inputs
    if($LASTEXITCODE -ne 0){throw 'Demo input preparation failed'}
}
$prefix=Split-Path $Output -Leaf
$cases=[System.Collections.Generic.List[object]]::new()
function Publish-Cases {
    # A report reader / antivirus may briefly hold the old file. Publish a complete UTF-8
    # snapshot and retry only the rename; never truncate an existing evidence manifest.
    $destination=[IO.Path]::GetFullPath("$Output/windows.json")
    $temporary=$destination+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
    try {
        [IO.File]::WriteAllText($temporary,($cases | ConvertTo-Json -Depth 8))
        for($attempt=0;;$attempt++) {
            try {[IO.File]::Move($temporary,$destination,$true);break}
            catch [IO.IOException] {if($attempt -ge 9){throw};Start-Sleep -Milliseconds 100}
        }
    } finally {if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}}
}
function Run([string]$Name,[string]$Package,[hashtable]$Extra){
    $arguments=@{BuildDirectory=$BuildDirectory;Frames=160;Capture=$true;WorkMode='image';ImageView='relighted';LogName="$prefix-$Name";ExportImage="$Output/$Name"}
    if($Package){$arguments.Package=$Package}
    foreach($key in $Extra.Keys){$arguments[$key]=$Extra[$key]}
    & "$PSScriptRoot/Run-Smoke.ps1" @arguments *> "$Output/$Name-run.log"
    $cases.Add(@{name=$Name;arguments=$arguments;status='pass';exit=0})
    Publish-Cases
}
Run 'native' "$Inputs/detail" @{SaveRecipe="$Output/native.json"}
Run 'zero' "$Inputs/detail" @{ImageSmoke='ratio-zero'}
Run 'cycle' "$Inputs/detail" @{ImageSmoke='cycle';UI=$true}
Run 'transaction' "$Inputs/detail" @{ImageSmoke='transaction'}
Run 'edit' "$Inputs/shadow/plane" @{ImageSmoke='shadow-move';SaveRecipe="$Output/edit.json";UI=$true}
Run 'reopen' '' @{Recipe="$Output/edit.json"}
Run 'ui' '' @{Recipe="$Output/edit.json";UI=$true;WindowSize='1920x1080';FixedSize=$true}
Run 'warp' '' @{Recipe="$Output/edit.json";Warp=$true}
Run 'release' '' @{Recipe="$Output/edit.json";Configuration='Release'}
Run 'reload' "$Inputs/detail" @{RecipeReload="$Output/edit.json";UI=$true}
Run 'cancel' "$Inputs/detail" @{RecipeReload="$Output/edit.json";RecipeCancel=$true}
Run 'missing' "$Inputs/detail" @{RecipeReload="$Output/absent.json"}
Run 'protected' "$Inputs/stability" @{ImageSmoke='stability-preset';ProtectionMask="$Inputs/stability.all-protect.png"}
Run 'shadow-zero' "$Inputs/shadow/plane" @{ImageSmoke='shadow-zero'}
Run 'shadow-confidence-zero' "$Inputs/shadow/no-confidence" @{ImageSmoke='shadow-move'}
Run 'emission' "$Inputs/shadow/emission" @{ImageSmoke='shadow-move'}
Run 'black' "$Inputs/shadow/black" @{ImageSmoke='shadow-move';UI=$true}
Run 'unsupported' "$Inputs/shadow/missing" @{ImageSmoke='shadow-move'}
Run 'specular' "$Inputs/specular/sphere" @{ImageSmoke='specular-move';UI=$true}
Run 'specular-off' "$Inputs/specular/sphere" @{ImageSmoke='specular-off'}
Run 'reference' "$Inputs/reference/source" @{ReferenceProposal="$Inputs/reference/reference-proposal";ApplyReference=$true;SaveRecipe="$Output/reference.json";UI=$true;WindowSize='1920x1080';FixedSize=$true}
Run 'reference-reopen' '' @{Recipe="$Output/reference.json"}
Run 'optimize' "$Inputs/reference/source" @{OptimizeReference="$Inputs/reference/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;UI=$true}
Run 'views' "$Inputs/shadow/plane" @{ImageSmoke='views';UI=$true}
Run 'narrow' '' @{Recipe="$Output/edit.json";UI=$true;WindowSize='600x1000';FixedSize=$true}
& "$PSScriptRoot/Run-Smoke.ps1" -BuildDirectory $BuildDirectory -Package assets/ScenePackage -Capture -LogName "$prefix-legacy" *> "$Output/legacy-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" -BuildDirectory $BuildDirectory -Package assets/ScenePackage -Warp -CameraSmoke -Capture -LogName "$prefix-legacy-warp" *> "$Output/legacy-warp-run.log"
if($RealPackage){
    Run 'real-original' $RealPackage @{ImageView='original';UI=$true;SaveRecipe="$Output/real-original.json"}
    Run 'real-edit' $RealPackage @{ImageSmoke='shadow-move';UI=$true;SaveRecipe="$Output/real-edit.json"}
    Run 'real-reopen' '' @{Recipe="$Output/real-edit.json"}
    Run 'real-cycle' '' @{Recipe="$Output/real-edit.json";ImageSmoke='cycle';UI=$true}
    & "$PSScriptRoot/Run-Smoke.ps1" -BuildDirectory $BuildDirectory -Package $RealPackage -CameraSmoke -UI -Capture -LogName "$prefix-real-3d" *> "$Output/real-3d-run.log"
}
& $Python "$PSScriptRoot/reconstruction/verify_core_demo.py" --output $Output --inputs $Inputs
if($LASTEXITCODE -ne 0){throw 'Core demo numeric verification failed'}
Write-Output "Core demo evidence: $Output; reopen $Output/reference.json with --recipe --work-mode image --ui"
