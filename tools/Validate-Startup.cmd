@echo off
setlocal
rem Exercise the actual CMD quoting and GUI-process exit-code behavior.
pushd "%~dp0.." || exit /b 1
if not exist generated mkdir generated
for %%E in (gltf glb) do (
    start "" /wait ".\build\Debug\ImageSceneRenderer.exe" --model "assets/models/MaterialLab/MaterialLab.%%E" --frames 90 --smoke --ui --capture "generated/startup-%%E.bmp" --log "generated/startup-%%E.log"
    if errorlevel 1 goto failure
    findstr /c:"Validation summary: errors=0 warnings=0" "generated\startup-%%E.log" >nul || goto failure
    echo PASS: CMD model .%%E with double quotes
)
start "" /wait ".\build\Debug\ImageSceneRenderer.exe" --model 'assets/models/MaterialLab/MaterialLab.gltf' --smoke --frames 1 --log "generated/startup-invalid-quotes.log"
if not errorlevel 1 goto failure
findstr /c:"CMD treats single quotes" "generated\startup-invalid-quotes.log" >nul || goto failure
echo PASS: invalid CMD quotes report an actionable error without blocking smoke tests
popd
exit /b 0
:failure
echo FAIL: inspect generated/startup-*.log
popd
exit /b 1
