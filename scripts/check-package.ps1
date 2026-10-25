$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$name = 'The-Cuelist-0.1.0-Windows-x64'
$zip = Join-Path $repo ('dist\' + $name + '.zip')
$checkRoot = Join-Path $repo ('build\package-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $checkRoot | Out-Null
Expand-Archive -LiteralPath $zip -DestinationPath $checkRoot
$package = Join-Path $checkRoot $name
foreach ($file in @('The Cuelist.exe', 'Avvia.cmd', 'LEGGIMI.txt', 'README.md', 'JUCE-LICENSE.md', 'SHA256.txt')) {
    if (!(Test-Path -LiteralPath (Join-Path $package $file))) { throw "File mancante: $file" }
}
$original = (Get-FileHash -LiteralPath (Join-Path $repo 'build\TheCuelist_artefacts\Release\The Cuelist.exe')).Hash
$extracted = (Get-FileHash -LiteralPath (Join-Path $package 'The Cuelist.exe')).Hash
if ($original -ne $extracted) { throw 'Eseguibile estratto diverso dalla build verificata.' }
$sourceOnly = Join-Path $checkRoot 'source-only'
New-Item -ItemType Directory -Path $sourceOnly | Out-Null
$launcher = Join-Path $sourceOnly 'Avvia.cmd'
Copy-Item -LiteralPath (Join-Path $repo 'Avvia.cmd') -Destination $launcher
$commandLine = 'call "' + $launcher + '" <nul'
$output = & $env:ComSpec /d /c $commandLine
if ($LASTEXITCODE -ne 1) { throw 'Il launcher senza eseguibile non ha restituito il codice atteso.' }
if (($output -join "`n") -notmatch 'non il programma compilato') { throw 'Istruzioni per il pacchetto mancanti.' }
Write-Host 'PASS: ZIP estratto con tutti i file richiesti.'
Write-Host 'PASS: SHA256 eseguibile identico alla build verificata.'
Write-Host 'PASS: launcher senza eseguibile mostra istruzioni e non tenta la compilazione.'
Write-Host "Cartella verificata: $package"
