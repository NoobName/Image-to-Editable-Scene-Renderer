param([string]$BuildDirectory='generated/build-prompt28')
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $base=@{BuildDirectory=$BuildDirectory;Package='generated/prompt27-final-fixtures/plane';WorkMode='image';ImageView='relighted';Frames=100;Capture=$true;FixedSize=$true;WindowSize='258x194'}
    & ./tools/Run-Smoke.ps1 @base -ImageSmoke shadow-move -LogName prompt28-baseline
    foreach($mode in @('workspace-protect','workspace-clear','workspace-exposure','workspace-overlay','workspace-global')) {
        & ./tools/Run-Smoke.ps1 @base -ImageSmoke $mode -LogName "prompt28-$mode"
    }
    & ./tools/Run-Smoke.ps1 @base -ImageSmoke workspace-protect -Warp -LogName prompt28-protect-warp
    & ./tools/Run-Smoke.ps1 @base -ImageSmoke workspace-clear -Configuration Release -LogName prompt28-release
    foreach($view in @('depth','geometry-normal','ratio','protection')) {
        & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package $base.Package -WorkMode image -ImageView $view -ImageSmoke workspace-exposure -Frames 100 -Capture -LogName "prompt28-debug-$view"
    }
    foreach($mode in @('workspace-side','workspace-wipe','workspace-zoom','cycle','views','transaction')) {
        & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode image -ImageView relighted -ImageSmoke $mode -UI -Frames 160 -Capture -LogName "prompt28-ui-$mode"
    }
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode image -ImageView relighted -ImageSmoke workspace-side -UI -FixedSize -WindowSize 720x900 -Frames 100 -Capture -LogName prompt28-narrow
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode image -ImageView relighted -ImageSmoke workspace-wipe -UI -Warp -Frames 100 -Capture -LogName prompt28-ui-warp
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode scene -UI -Frames 140 -Capture -LogName prompt28-3d-ui
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode scene -Frames 140 -Capture -LogName prompt28-3d
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package assets/ScenePackage -WorkMode image -Frames 100 -Capture -LogName prompt28-legacy
    & ./tools/Run-ReconstructionSmoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -Image generated/scene24-final-indoor -ShadowOnly -WorkMode image -ImageView relighted -UI -Repeat 2 -LogName prompt28-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -Image generated/missing28 -ShadowOnly -WorkMode image -ImageView original -UI -ExpectError -LogName prompt28-failed
    & ./tools/Run-ReconstructionSmoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -Image generated/scene24-final-indoor -ShadowOnly -WorkMode image -ImageView original -UI -CancelFrame 5 -ExpectError -LogName prompt28-cancel
} finally {Pop-Location}
