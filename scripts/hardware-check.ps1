param([switch]$OscLoopback)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ Build Tools are required.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC compiler not found.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (!(Test-Path -LiteralPath 'build\CMakeCache.txt')) {
    & $cmake -S . -B build
    if ($LASTEXITCODE) { throw 'CMake configure failed.' }
}
& $cmake --build build --config Release --target CuelistHardwareProbe --parallel 4
if ($LASTEXITCODE) { throw 'Hardware probe compilation failed.' }
$probe = Join-Path $repo 'build\Release\CuelistHardwareProbe.exe'
if (!(Test-Path -LiteralPath $probe)) { throw 'Hardware probe executable is missing.' }
$reports = Join-Path $repo 'build\hardware-reports'
New-Item -ItemType Directory -Force -Path $reports | Out-Null
$out = Join-Path $reports ('hardware-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.txt')
$argsList = @()
if ($OscLoopback) { $argsList += '--osc-loopback' }
& $probe @argsList 2>&1 | Tee-Object -FilePath $out
if ($LASTEXITCODE) { throw "Hardware probe failed. See $out" }
Write-Host "Local inventory report saved: $out"
Write-Host 'Test playback, reconnection and physical MIDI/OSC targets manually during rehearsal.'
