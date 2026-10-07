param([string]$BuildDirectory='generated/build-prompt29',[string]$Output='generated/prompt29-output')
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory; validation never deletes previous evidence.'}
New-Item -ItemType Directory -Path $Output | Out-Null
$shared=@{BuildDirectory=$BuildDirectory;Frames=90;Capture=$true;WorkMode='image';ImageView='relighted'}
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Package generated/prompt21-detail -LogName prompt29-native -SaveRecipe "$Output/native.json" -ExportImage "$Output/native"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Package generated/prompt27-final-fixtures/plane -ImageSmoke workspace-protect -LogName prompt29-edit -SaveRecipe "$Output/edit.json" -ExportImage "$Output/edit"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/edit.json" -LogName prompt29-reopen -ExportImage "$Output/reopen"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/edit.json" -UI -LogName prompt29-ui -ExportImage "$Output/ui"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/edit.json" -Warp -LogName prompt29-warp -ExportImage "$Output/warp"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/edit.json" -Configuration Release -LogName prompt29-release -ExportImage "$Output/release"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Package generated/prompt21-detail -RecipeReload "$Output/edit.json" -UI -LogName prompt29-reload -ExportImage "$Output/reload"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Package generated/prompt21-detail -RecipeReload "$Output/edit.json" -RecipeCancel -LogName prompt29-cancel -ExportImage "$Output/cancel"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Package generated/prompt21-detail -RecipeReload "$Output/missing.json" -LogName prompt29-missing -ExportImage "$Output/missing"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Package generated/prompt27-final-fixtures/plane -ImageSmoke lighting-source -LogName prompt29-calibrate -SaveRecipe "$Output/calibrated.json" -ExportImage "$Output/calibrated"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/calibrated.json" -LogName prompt29-calibrate-reopen -ExportImage "$Output/calibrated-reopen"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/edit.json" -ImageSmoke cycle -UI -WindowSize 720x900 -LogName prompt29-cycle -ExportImage "$Output/cycle"
& "$PSScriptRoot/Run-Smoke.ps1" -BuildDirectory $BuildDirectory -Package assets/ScenePackage -Frames 90 -Capture -LogName prompt29-legacy
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Package generated/scene26-final-indoor -ImageSmoke workspace-side -UI -LogName prompt29-real -SaveRecipe "$Output/real.json" -ExportImage "$Output/real"
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/real.json" -LogName prompt29-real-reopen -ExportImage "$Output/real-reopen"
# Relocate a complete project tree, preserving the package bytes and relative reference.
$moved=Join-Path $Output '中文搬移'
New-Item -ItemType Directory -Path $moved | Out-Null
Copy-Item -LiteralPath generated/prompt27-final-fixtures/plane -Destination "$moved/package" -Recurse
$recipe=Get-Content -LiteralPath "$Output/edit.json" -Raw -Encoding utf8 | ConvertFrom-Json
$recipe.sourcePackage='package'
$recipe | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath "$moved/配方.json" -Encoding utf8
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$moved/配方.json" -LogName prompt29-moved -ExportImage "$Output/moved"
$bad=Get-Content -LiteralPath "$Output/edit.json" -Raw -Encoding utf8 | ConvertFrom-Json
$bad.identity.anchorSha256=('0'*64)
$bad | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath "$Output/bad-hash.json" -Encoding utf8
& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/edit.json" -RecipeReload "$Output/bad-hash.json" -LogName prompt29-bad-hash -ExportImage "$Output/bad-hash-retained"
$before=(Get-FileHash -LiteralPath "$Output/edit.json").Hash
$failed=$false
try{& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/edit.json" -SaveRecipe "$Output/edit.json" -LogName prompt29-save-existing}catch{$failed=$true}
if(!$failed -or $before -ne (Get-FileHash -LiteralPath "$Output/edit.json").Hash){throw 'Save-new failure did not preserve recipe'}
$before=(Get-FileHash -LiteralPath "$Output/edit/result.png").Hash
$failed=$false
try{& "$PSScriptRoot/Run-Smoke.ps1" @shared -Recipe "$Output/edit.json" -ExportImage "$Output/edit" -LogName prompt29-export-existing}catch{$failed=$true}
if(!$failed -or $before -ne (Get-FileHash -LiteralPath "$Output/edit/result.png").Hash){throw 'Export failure did not preserve result'}
