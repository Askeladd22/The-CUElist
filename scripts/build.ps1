param([switch]$Run, [switch]$TestOnly)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) { throw 'Installare Visual Studio Build Tools con Sviluppo desktop con C++.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Il compilatore MSVC non e installato.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
if (!(Test-Path $cmake)) { throw 'Installare il componente Strumenti CMake per Windows in Visual Studio Installer.' }
if (!(Test-Path 'vendor/JUCE/CMakeLists.txt')) {
    git clone --depth 1 --branch 8.0.12 https://github.com/juce-framework/JUCE.git vendor/JUCE
    if ($LASTEXITCODE) { throw 'Download JUCE fallito.' }
}
& $cmake -S . -B build
if ($LASTEXITCODE) { throw 'Configurazione CMake fallita.' }
if ($TestOnly) { & $cmake --build build --config Release --target CuelistTests --parallel 4 }
else { & $cmake --build build --config Release --parallel 4 }
if ($LASTEXITCODE) { throw 'Compilazione fallita.' }
& $ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Test falliti.' }
if ($Run) { Start-Process -FilePath (Join-Path $repo 'build\TheCuelist_artefacts\Release\The Cuelist.exe') }
