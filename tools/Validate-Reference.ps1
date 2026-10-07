param([string]$BuildDirectory='generated/build-prompt30',[string]$Fixtures='generated/prompt30-fixtures',[string]$Output='generated/prompt30-output')
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Choose a new output directory'}
New-Item -ItemType Directory -Path $Output | Out-Null
$common=@{BuildDirectory=$BuildDirectory;Package="$Fixtures/source";WorkMode='image';ImageView='relighted';Frames=90;Capture=$true}
function Run([string]$name,[hashtable]$extra){
    $args=@{};foreach($key in $common.Keys){$args[$key]=$common[$key]};foreach($key in $extra.Keys){$args[$key]=$extra[$key]}
    $args.LogName="prompt30-$name";$args.ExportImage="$Output/$name"
    & "$PSScriptRoot/Run-Smoke.ps1" @args *> "$Output/$name.log"
}
Run 'baseline' @{}
Run 'inspect' @{ReferenceProposal="$Fixtures/reference-proposal"}
Run 'apply' @{ReferenceProposal="$Fixtures/reference-proposal";ApplyReference=$true;SaveRecipe="$Output/matched.json"}
Run 'process' @{ReferenceInput="$Fixtures/reference";ReferenceRelation='same-scene';ApplyReference=$true}
Run 'different' @{ReferenceProposal="$Fixtures/different-proposal";ApplyReference=$true}
Run 'reset' @{ReferenceProposal="$Fixtures/reference-proposal";ApplyReference=$true;ResetReference=$true}
Run 'plane' @{ReferenceProposal="$Fixtures/plane-proposal";ApplyReference=$true}
Run 'fallback' @{ReferenceProposal="$Fixtures/fallback-proposal";ApplyReference=$true}
Run 'warp' @{ReferenceProposal="$Fixtures/reference-proposal";ApplyReference=$true;Warp=$true}
Run 'release' @{ReferenceProposal="$Fixtures/reference-proposal";ApplyReference=$true;Configuration='Release'}
Run 'ui-repeat' @{ReferenceProposal="$Fixtures/reference-proposal";ApplyReference=$true;UI=$true;ReferenceRepeat=3;ImageSmoke='cycle';WindowSize='1600x900'}
Run 'narrow' @{ReferenceProposal="$Fixtures/reference-proposal";ApplyReference=$true;UI=$true;WindowSize='800x1000';FixedSize=$true}
Run 'cancel' @{ReferenceInput="$Fixtures/reference";ApplyReference=$true;ReferenceCancelFrame=5}
Run 'missing' @{ReferenceProposal="$Fixtures/missing";ApplyReference=$true}
$common.Remove('Package');Run 'reopen' @{Recipe="$Output/matched.json"}
Write-Output 'Reference finite-window matrix complete'
