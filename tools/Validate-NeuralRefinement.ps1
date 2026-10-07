param([string]$BuildDirectory='generated/build-prompt33',[string]$Python='python',
    [string]$Recipe='generated/prompt32-demo-final/real-edit.json',
    [ValidatePattern('^generated/[a-zA-Z0-9][a-zA-Z0-9_.-]*$')][string]$Output='generated/refinement-windows')
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Choose a new evidence directory'}
New-Item -ItemType Directory -Path $Output | Out-Null
$prefix=Split-Path $Output -Leaf
$cases=@(
    @{Name='ready';Extra=@{UI=$true;WindowSize='1920x1080';FixedSize=$true}},
    @{Name='cancel';Extra=@{UI=$true;RefinementCancelFrame=20}},
    @{Name='stale';Extra=@{UI=$true;RefinementEditFrame=20}},
    @{Name='failure';Extra=@{RefinementPython='generated/missing-refinement-python.exe'}},
    @{Name='zero-warp';Extra=@{RefinementStrength=0;Warp=$true;UI=$true}}
)
foreach($case in $cases){
    $params=@{BuildDirectory=$BuildDirectory;Recipe=$Recipe;WorkMode='image';ImageView='relighted';Frames=160;
        Capture=$true;RefinementRun=$true;RefinementStrength=0.2;RefinementPython=$Python;
        LogName="$prefix-$($case.Name)";ExportImage="$Output/$($case.Name)"}
    foreach($key in $case.Extra.Keys){$params[$key]=$case.Extra[$key]}
    & "$PSScriptRoot/Run-Smoke.ps1" @params *> "$Output/$($case.Name)-run.log"
    $log=Get-Content -LiteralPath "generated/$prefix-$($case.Name).log" -Raw
    $expected=switch($case.Name){'cancel' {'Refinement stopped; physics retained: Reconstruction cancelled;'} 'stale' {'Cancelled/stale candidate'} 'failure' {'Refinement stopped; physics retained'} default {'Offline candidate ready'}}
    if(!$log.Contains($expected)){throw "Refinement $($case.Name) did not reach expected state: $expected"}
}
Write-Output "Refinement process/window evidence: $Output"
