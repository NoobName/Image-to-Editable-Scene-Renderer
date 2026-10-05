param([string]$VisualStudioPath, [switch]$Fresh)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$VisualStudioPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $VisualStudioPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
}
if (!$VisualStudioPath) { throw 'Visual Studio was not discovered. Pass -VisualStudioPath <installation directory>.' }
$devcmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $devcmd)) { throw "Missing developer environment: $devcmd" }
$configure = 'cmake --preset windows'
if ($Fresh) { $configure += ' --fresh' }
# Match compiler output encoding to CMake dependency-prefix detection.
$command = 'chcp 65001 >nul && call "' + $devcmd + '" -arch=x64 -host_arch=x64 && ' + $configure +
    ' && cmake --build --preset debug && cmake --build --preset release' +
    ' && ctest --test-dir build -C Debug --output-on-failure && ctest --test-dir build -C Release --output-on-failure'
Push-Location $projectRoot
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw "Build or tests failed with exit code $LASTEXITCODE" }
} finally { Pop-Location }
