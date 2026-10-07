param([string]$VisualStudioPath, [switch]$Fresh,
    [ValidatePattern('^(build|generated/[a-zA-Z0-9][a-zA-Z0-9_.-]*)$')][string]$BuildDirectory='build')
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
if($BuildDirectory -ne 'build'){$configure += ' -B "'+$BuildDirectory+'"'}
if ($Fresh) { $configure += ' --fresh' }
# Match compiler output encoding to CMake dependency-prefix detection.
$command = 'chcp 65001 >nul && call "' + $devcmd + '" -arch=x64 -host_arch=x64 && ' + $configure +
    ' && cmake --build "'+$BuildDirectory+'" --config Debug && cmake --build "'+$BuildDirectory+'" --config Release' +
    ' && ctest --test-dir "'+$BuildDirectory+'" -C Debug --output-on-failure && ctest --test-dir "'+$BuildDirectory+'" -C Release --output-on-failure'
Push-Location $projectRoot
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw "Build or tests failed with exit code $LASTEXITCODE" }
} finally { Pop-Location }
