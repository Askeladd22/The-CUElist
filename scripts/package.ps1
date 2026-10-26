param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo
if (!$SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') }
$exe = Join-Path $repo 'build\TheCuelist_artefacts\Release\The Cuelist.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Release executable missing. Compile on the development PC before packaging.' }
$packageName = 'The-Cuelist-0.5.0-Windows-x64'
$dist = Join-Path $repo 'dist'
$stagingRoot = Join-Path $repo ('build\package-' + [guid]::NewGuid().ToString('N'))
$package = Join-Path $stagingRoot $packageName
New-Item -ItemType Directory -Path $package -Force | Out-Null
New-Item -ItemType Directory -Path $dist -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $package 'The Cuelist.exe')
Copy-Item -LiteralPath (Join-Path $repo 'Avvia.cmd') -Destination $package
Copy-Item -LiteralPath (Join-Path $repo 'README.md') -Destination $package
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination $package
Copy-Item -LiteralPath (Join-Path $repo 'NOTICE.md') -Destination $package
Copy-Item -LiteralPath (Join-Path $repo 'vendor\JUCE\LICENSE.md') -Destination (Join-Path $package 'JUCE-LICENSE.md')
@'
THE CUELIST 0.5.0 - WINDOWS x64

1. Extract ALL files from this ZIP into a folder on your computer.
2. Double-click "The Cuelist.exe" to launch the application.

You do not need to install Visual Studio, Build Tools, CMake, Git, or Python.
Avvia.cmd is an alternative launcher. It does not build or install anything.

Import a WAV or MP3 file. SPACE triggers the next cue; ESC/PANIC stops playback.
To move a show, copy both its .cuelist file and adjacent _media folder.
Read README.md for operating instructions and version limitations.
MIDI: import or create notes in the piano roll; select the output in column utilities.
OSC: configure the UDP destination in column utilities and edit timed events/ramps.

JUCE 8.0.12: https://github.com/juce-framework/JUCE/tree/8.0.12
The Cuelist: proprietary software, all rights reserved.
See LICENSE for terms and NOTICE.md for third-party licenses.
Sources are not included. Verify JUCE licensing before distributing to others.
'@ | Set-Content -LiteralPath (Join-Path $package 'README-WINDOWS.txt') -Encoding UTF8
$exeHash = (Get-FileHash -LiteralPath (Join-Path $package 'The Cuelist.exe') -Algorithm SHA256).Hash
"$exeHash  The Cuelist.exe" | Set-Content -LiteralPath (Join-Path $package 'SHA256.txt') -Encoding ASCII
$zip = Join-Path $dist ($packageName + '.zip')
Compress-Archive -LiteralPath $package -DestinationPath $zip -Force
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
"$hash  $packageName.zip" | Set-Content -LiteralPath ($zip + '.sha256') -Encoding ASCII
Write-Host "Package ready: $zip"
Write-Host "SHA256: $hash"
