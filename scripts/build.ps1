param([switch]$Run, [switch]$TestOnly)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio Build Tools with Desktop development with C++.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'The MSVC compiler is not installed.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
if (!(Test-Path $cmake)) { throw 'Install CMake tools for Windows in Visual Studio Installer.' }
if (!(Test-Path 'vendor/JUCE/CMakeLists.txt')) {
    git clone --depth 1 --branch 8.0.12 https://github.com/juce-framework/JUCE.git vendor/JUCE
    if ($LASTEXITCODE) { throw 'JUCE download failed.' }
}
& $cmake -S . -B build
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
if ($TestOnly) { & $cmake --build build --config Release --target CuelistTests --parallel 4 }
else { & $cmake --build build --config Release --parallel 4 }
if ($LASTEXITCODE) { throw 'Compilation failed.' }
& $ctest --test-dir build -C Release --output-on-failure -V
if ($LASTEXITCODE) { throw 'Tests failed.' }
if ($Run) { Start-Process -FilePath (Join-Path $repo 'build\TheCuelist_artefacts\Release\The Cuelist.exe') }
