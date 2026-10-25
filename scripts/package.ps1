param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo
if (!$SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') }
$exe = Join-Path $repo 'build\TheCuelist_artefacts\Release\The Cuelist.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Eseguibile Release assente. Compilare sul computer di sviluppo prima di creare il pacchetto.' }
$packageName = 'The-Cuelist-0.1.0-Windows-x64'
$dist = Join-Path $repo 'dist'
$stagingRoot = Join-Path $repo ('build\package-' + [guid]::NewGuid().ToString('N'))
$package = Join-Path $stagingRoot $packageName
New-Item -ItemType Directory -Path $package -Force | Out-Null
New-Item -ItemType Directory -Path $dist -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $package 'The Cuelist.exe')
Copy-Item -LiteralPath (Join-Path $repo 'Avvia.cmd') -Destination $package
Copy-Item -LiteralPath (Join-Path $repo 'README.md') -Destination $package
Copy-Item -LiteralPath (Join-Path $repo 'vendor\JUCE\LICENSE.md') -Destination (Join-Path $package 'JUCE-LICENSE.md')
@'
THE CUELIST 0.1.0 - WINDOWS x64

1. Estrai TUTTO questo ZIP in una cartella sul tuo computer.
2. Apri "The Cuelist.exe" con un doppio click.

Non occorre installare Visual Studio, Build Tools, CMake, Git o Python.
Avvia.cmd e un avvio alternativo; non compila e non installa nulla.

Importa un WAV in una cella Audio. GO avvia la riga. ESC/PANIC ferma l'audio.
Per tutte le istruzioni e i limiti della prima versione leggi README.md.
MIDI, OSC e timecode non sono ancora operativi in questa versione.

JUCE 8.0.12: https://github.com/juce-framework/JUCE/tree/8.0.12
Informazioni di licenza incluse in JUCE-LICENSE.md e README.md.
'@ | Set-Content -LiteralPath (Join-Path $package 'LEGGIMI.txt') -Encoding UTF8
$exeHash = (Get-FileHash -LiteralPath (Join-Path $package 'The Cuelist.exe') -Algorithm SHA256).Hash
"$exeHash  The Cuelist.exe" | Set-Content -LiteralPath (Join-Path $package 'SHA256.txt') -Encoding ASCII
$zip = Join-Path $dist ($packageName + '.zip')
Compress-Archive -LiteralPath $package -DestinationPath $zip -Force
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
"$hash  $packageName.zip" | Set-Content -LiteralPath ($zip + '.sha256') -Encoding ASCII
Write-Host "Pacchetto pronto: $zip"
Write-Host "SHA256: $hash"
