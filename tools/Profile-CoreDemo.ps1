param([string]$BuildDirectory='build',[Parameter(Mandatory=$true)][string]$Package,
    [string]$LowerAnalysisPackage,[string]$Prefix='core-profile')
$ErrorActionPreference='Stop'
$common=@{BuildDirectory=$BuildDirectory;Package=$Package;WorkMode='image';ImageView='relighted';WindowSize='1920x1080';FixedSize=$true;Frames=420;Profile=$true}
# Sequential runs: overlapping processes would measure contention instead of each workload.
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -LogName "$Prefix-idle" *> "generated/$Prefix-idle-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -ImageSmoke profile-drag -LogName "$Prefix-drag" *> "generated/$Prefix-drag-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Debug -ImageSmoke profile-drag -LogName "$Prefix-debug" *> "generated/$Prefix-debug-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -ImageSmoke profile-drag -UI -LogName "$Prefix-ui" *> "generated/$Prefix-ui-run.log"
if($LowerAnalysisPackage){
    $common.Package=$LowerAnalysisPackage
    & "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -ImageSmoke profile-drag -LogName "$Prefix-lower" *> "generated/$Prefix-lower-run.log"
}
Write-Output "Raw samples: generated/$Prefix-*.profile.json (no image captures/exports)"
