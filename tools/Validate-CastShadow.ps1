param([ValidatePattern('^(build|generated/[a-zA-Z0-9][a-zA-Z0-9_.-]*)$')][string]$BuildDirectory='build')
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try{
    $base=@{BuildDirectory=$BuildDirectory;Package='generated/prompt27-final-fixtures/plane';WorkMode='image';Frames=160;Capture=$true}
    foreach($view in @('cast-old-map','cast-new-map','cast-old-visibility','cast-new-visibility','cast-old-estimate','cast-confidence','cast-change','cast-difference','cast-final','cast-baseline')){
        & ./tools/Run-Smoke.ps1 @base -ImageView $view -ImageSmoke shadow-move -FixedSize -WindowSize 258x194 -LogName "prompt27-$view"
    }
    foreach($mode in @('shadow-off','shadow-zero','shadow-confidence-zero','shadow-reset','shadow-direct-off','shadow-pcf','shadow-scene-edit')){
        & ./tools/Run-Smoke.ps1 @base -ImageView cast-final -ImageSmoke $mode -LogName "prompt27-$mode"
    }
    & ./tools/Run-Smoke.ps1 @base -ImageView cast-final -LogName prompt27-identity
    & ./tools/Run-Smoke.ps1 @base -ImageView cast-final -ImageSmoke shadow-move -Warp -LogName prompt27-warp
    & ./tools/Run-Smoke.ps1 @base -ImageView cast-final -ImageSmoke shadow-move -Configuration Release -LogName prompt27-release
    foreach($kind in @('missing','emission','black','no-confidence','asymmetric')){
        & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package "generated/prompt27-final-fixtures/$kind" -WorkMode image -ImageView cast-final -ImageSmoke shadow-move -Frames 160 -Capture -LogName "prompt27-$kind"
    }
    foreach($mode in @('views','cycle','transaction','lighting-source')){
        & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode image -ImageView cast-final -ImageSmoke $mode -Frames 160 -Capture -LogName "prompt27-$mode"
    }
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode image -ImageView cast-final -ImageSmoke shadow-move -Frames 160 -Capture -UI -LogName prompt27-real-ui
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode image -ImageView cast-change -ImageSmoke shadow-move -Frames 160 -Capture -FixedSize -WindowSize 420x980 -LogName prompt27-real-tall
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode image -ImageView cast-final -ImageSmoke shadow-move -Frames 160 -Capture -LogName prompt27-real
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene24-final-indoor -WorkMode image -ImageView cast-final -ImageSmoke specular-move -Frames 160 -Capture -LogName prompt27-unavailable
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode scene -Frames 140 -Capture -LogName prompt27-3d
    & ./tools/Run-Smoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -WorkMode scene -Frames 140 -Capture -Warp -LogName prompt27-3d-warp
    & ./tools/Run-ReconstructionSmoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -Image generated/scene24-final-indoor -ShadowOnly -WorkMode image -ImageView cast-final -Repeat 2 -LogName prompt27-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -Image generated/missing27 -ShadowOnly -WorkMode image -ImageView original -ExpectError -LogName prompt27-failed
    & ./tools/Run-ReconstructionSmoke.ps1 -BuildDirectory $BuildDirectory -Package generated/scene26-final-indoor -Image generated/scene24-final-indoor -ShadowOnly -WorkMode image -ImageView original -CancelFrame 5 -ExpectError -LogName prompt27-cancel
}finally{Pop-Location}
