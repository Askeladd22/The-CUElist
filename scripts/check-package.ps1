param([switch]$SmokeLaunch)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$name = 'The-Cuelist-0.5.0-Windows-x64'
$zip = Join-Path $repo ('dist\' + $name + '.zip')
$checkRoot = Join-Path $repo ('build\package-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $checkRoot | Out-Null
Expand-Archive -LiteralPath $zip -DestinationPath $checkRoot
$package = Join-Path $checkRoot $name
foreach ($file in @('The Cuelist.exe', 'Avvia.cmd', 'LEGGIMI.txt', 'README.md', 'LICENSE', 'NOTICE.md', 'JUCE-LICENSE.md', 'SHA256.txt')) {
    if (!(Test-Path -LiteralPath (Join-Path $package $file))) { throw "File mancante: $file" }
}
if (Test-Path -LiteralPath (Join-Path $package 'Sorgenti.zip')) { throw 'Archivio sorgenti inatteso nel pacchetto proprietario.' }
foreach ($file in @('README.md','LICENSE','NOTICE.md')) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $file)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $package $file)).Hash) { throw "Documento non aggiornato: $file" }
}
Write-Host 'PASS: licenza e documentazione aggiornate; archivio sorgenti assente.'
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
if ($SmokeLaunch) {
    $application = Start-Process -FilePath (Join-Path $package 'The Cuelist.exe') -ArgumentList '--offline' -WorkingDirectory $package -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 2
    $application.Refresh()
    if ($application.HasExited) { throw "Avvio portable fallito: uscita anticipata $($application.ExitCode)." }
    if (!$application.CloseMainWindow()) { throw 'Impossibile chiedere la chiusura della finestra di verifica.' }
    if (!$application.WaitForExit(5000)) { throw 'La finestra di verifica non si e chiusa entro 5 secondi.' }
    if ($application.ExitCode -ne 0) { throw "Chiusura portable con errore: $($application.ExitCode)." }
    Write-Host 'PASS: eseguibile estratto avviato offline e chiuso normalmente.'
}
