param([string]$BuildDirectory='generated/build-prompt31',[string]$Fixtures='generated/prompt31-fixtures',[string]$Output='generated/prompt31-output')
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Choose a new output directory'}
New-Item -ItemType Directory -Path $Output | Out-Null
$common=@{BuildDirectory=$BuildDirectory;Package="$Fixtures/source";WorkMode='image';ImageView='relighted';Frames=90;Capture=$true}
function Run([string]$name,[hashtable]$extra){
    $args=@{};foreach($key in $common.Keys){$args[$key]=$common[$key]};foreach($key in $extra.Keys){$args[$key]=$extra[$key]}
    $args.LogName="prompt31-$name";$args.ExportImage="$Output/$name"
    & "$PSScriptRoot/Run-Smoke.ps1" @args *> "$Output/$name.log"
    $text=Get-Content "generated/prompt31-$name.log" -Raw
    $match=[regex]::Match($text,'Reference job directory: ([^\r\n]+)')
    if($match.Success){$job=$match.Groups[1].Value;Set-Content -Encoding utf8 "$Output/$name/job.txt" $job
        if(Test-Path -LiteralPath "$job/result/optimization.json"){Copy-Item -LiteralPath "$job/result/optimization.json" -Destination "$Output/$name/optimization.json"}
        if(Test-Path -LiteralPath "$job/result/gpu-verification.json"){Copy-Item -LiteralPath "$job/result/gpu-verification.json" -Destination "$Output/$name/gpu-verification.json"}
    }
}
Run 'baseline' @{SaveRecipe="$Output/baseline.json"}
Run 'optimized' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;SaveRecipe="$Output/optimized.json"}
Run 'inspect' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true}
Run 'different' @{OptimizeReference="$Fixtures/different-proposal";ApplyReference=$true}
Run 'reset' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;ResetReference=$true}
Run 'unregistered' @{OptimizeReference="$Fixtures/reference-proposal";ApplyReference=$true}
Run 'protected' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;ProtectionMask="$Fixtures/protect-all.png"}
Run 'plane-reference' @{OptimizeReference="$Fixtures/plane-proposal";ApplyReference=$true}
Run 'cancel' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;ReferenceCancelFrame=5}
Run 'edited' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;OptimizationEditFrame=5}
Run 'reload' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;RecipeReload="$Output/baseline.json"}
Run 'warp' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;Warp=$true}
Run 'release' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;Configuration='Release'}
Run 'ui' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;UI=$true;FixedSize=$true;WindowSize='1600x1400'}
Run 'cycle' @{OptimizeReference="$Fixtures/reference-proposal";OptimizationRegistered=$true;ApplyReference=$true;UI=$true;ImageSmoke='cycle'}
Run 'style-start' @{ReferenceProposal="$Fixtures/different-proposal";ApplyReference=$true;SaveRecipe="$Output/style-start.json"}
$common.Remove('Package')
Run 'reopen' @{Recipe="$Output/optimized.json"}
Run 'no-improvement' @{Recipe="$Output/style-start.json";OptimizeReference="$Fixtures/different-proposal";ApplyReference=$true}
Write-Output 'Optimization finite-window matrix complete'
