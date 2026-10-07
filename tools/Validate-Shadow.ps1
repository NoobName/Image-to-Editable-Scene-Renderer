$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try{
    foreach($view in @('shadow-candidate','shadow-visibility','shadow-geometry','shadow-confidence','shadow-unknown','shadow-manual-confirm','shadow-manual-protect','shadow-effective','shadow-overlay')){
        & ./tools/Run-Smoke.ps1 -Package generated/prompt26-final-fixtures/occluder -WorkMode image -ImageView $view -FixedSize -WindowSize 129x97 -Frames 140 -Capture -LogName "prompt26-$view"
    }
    foreach($mode in @('views','cycle','input','transaction','lighting-source')){
        & ./tools/Run-Smoke.ps1 -Package generated/scene26-final-indoor -WorkMode image -ImageView shadow-overlay -ImageSmoke $mode -Frames 140 -Capture -LogName "prompt26-$mode"
    }
    & ./tools/Run-Smoke.ps1 -Package generated/scene26-final-indoor -WorkMode image -ImageView shadow-overlay -UI -Frames 140 -Capture -LogName prompt26-ui
    & ./tools/Run-Smoke.ps1 -Package generated/prompt26-final-fixtures/manual -WorkMode image -ImageView shadow-effective -Warp -Frames 140 -Capture -LogName prompt26-warp
    & ./tools/Run-Smoke.ps1 -Package generated/prompt26-final-fixtures/missing -WorkMode image -ImageView shadow-unknown -Frames 140 -Capture -LogName prompt26-missing
    & ./tools/Run-Smoke.ps1 -Package generated/prompt26-final-fixtures/occluder -WorkMode image -ImageView shadow-overlay -Configuration Release -Frames 140 -Capture -LogName prompt26-release
    & ./tools/Run-Smoke.ps1 -Package generated/scene24-final-indoor -WorkMode image -ImageView shadow-overlay -Frames 140 -Capture -LogName prompt26-unavailable
    foreach($phase in @('before','after')){
        $package=if($phase -eq 'before'){'generated/scene24-final-indoor'}else{'generated/scene26-final-indoor'}
        & ./tools/Run-Smoke.ps1 -Package $package -WorkMode image -ImageView relighted -ImageSmoke specular-move -Frames 140 -Capture -LogName "prompt26-$phase"
        & ./tools/Run-Smoke.ps1 -Package $package -WorkMode image -ImageView original -FixedSize -WindowSize 512x341 -Frames 140 -Capture -LogName "prompt26-source-$phase"
        & ./tools/Run-Smoke.ps1 -Package $package -WorkMode scene -Frames 140 -Capture -LogName "prompt26-final-$phase"
    }
    & ./tools/Run-Smoke.ps1 -Package generated/scene26-final-indoor -WorkMode image -ImageView shadow-overlay -FixedSize -WindowSize 420x980 -Frames 140 -Capture -LogName prompt26-tall
    & ./tools/Run-ReconstructionSmoke.ps1 -Package generated/scene26-final-indoor -Image generated/scene24-final-indoor -ShadowOnly -WorkMode image -ImageView shadow-overlay -Repeat 2 -LogName prompt26-reload
    & ./tools/Run-ReconstructionSmoke.ps1 -Package generated/scene26-final-indoor -Image generated/missing26 -ShadowOnly -WorkMode image -ImageView original -ExpectError -LogName prompt26-failed
    & ./tools/Run-ReconstructionSmoke.ps1 -Package generated/scene26-final-indoor -Image generated/scene24-final-indoor -ShadowOnly -WorkMode image -ImageView original -CancelFrame 5 -ExpectError -LogName prompt26-cancel
}finally{Pop-Location}
