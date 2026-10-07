param([string]$BuildDirectory='build',[Parameter(Mandatory=$true)][string]$Package,
    [string]$SmallPackage,[string]$Prefix='fog-profile')
$ErrorActionPreference='Stop'
$common=@{BuildDirectory=$BuildDirectory;Package=$Package;WorkMode='image';ImageView='relighted';WindowSize='1920x1080';FixedSize=$true;Frames=420;Profile=$true}
# Keep runs sequential and exclude capture/readback, just like the Core timing protocol.
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -LogName "$Prefix-off" *> "generated/$Prefix-off-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -ImageSmoke profile-drag -LogName "$Prefix-off-light" *> "generated/$Prefix-off-light-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -ImageFogDensity .15 -LogName "$Prefix-idle" *> "generated/$Prefix-idle-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -ImageFogDensity .15 -ImageSmoke fog-drag -LogName "$Prefix-density" *> "generated/$Prefix-density-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Release -ImageFogDensity .15 -ImageSmoke profile-drag -LogName "$Prefix-light" *> "generated/$Prefix-light-run.log"
& "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Debug -ImageFogDensity .15 -ImageSmoke profile-drag -UI -LogName "$Prefix-debug" *> "generated/$Prefix-debug-run.log"
if($SmallPackage){
    $common.Package=$SmallPackage;$common.WindowSize='640x480'
    & "$PSScriptRoot/Run-Smoke.ps1" @common -Configuration Debug -ImageFogDensity .3 -ImageSmoke fog-drag -Warp -LogName "$Prefix-warp" *> "generated/$Prefix-warp-run.log"
}
Write-Output "Raw timing samples: generated/$Prefix-*.profile.json; no captures."
