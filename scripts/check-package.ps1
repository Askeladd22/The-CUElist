param([switch]$SmokeLaunch)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$version = (Get-Content -LiteralPath (Join-Path $repo 'VERSION.txt') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+(-[a-zA-Z0-9.]+)?$') { throw 'Invalid VERSION.txt.' }
$name = "The-Cuelist-$version-Windows-x64"
$zip = Join-Path $repo ('dist\' + $name + '.zip')
$checkRoot = Join-Path $repo ('build\package-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $checkRoot | Out-Null
Expand-Archive -LiteralPath $zip -DestinationPath $checkRoot
$package = Join-Path $checkRoot $name
foreach ($file in @('The Cuelist.exe', 'Avvia.cmd', 'README-WINDOWS.txt', 'README.md', 'LICENSE', 'NOTICE.md', 'JUCE-LICENSE.md', 'SHA256.txt')) {
    if (!(Test-Path -LiteralPath (Join-Path $package $file))) { throw "Missing file: $file" }
}
foreach ($license in @('Exo2-OFL.txt','Hind-OFL.txt')) {
    $relative = 'fonts\' + $license
    $source = Join-Path $repo ('assets\fonts\' + $license)
    $packaged = Join-Path $package $relative
    if (!(Test-Path -LiteralPath $packaged)) { throw "Missing OFL notice: $relative" }
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $packaged).Hash) { throw "Outdated OFL notice: $relative" }
}
if (Test-Path -LiteralPath (Join-Path $package 'Sorgenti.zip')) { throw 'Unexpected source archive in the proprietary package.' }
$guide = 'docs\complete-user-guide.md'
if (!(Test-Path -LiteralPath (Join-Path $package $guide))) { throw "Missing offline User Guide: $guide" }
if ((Get-FileHash -LiteralPath (Join-Path $repo $guide)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $package $guide)).Hash) { throw 'Outdated bundled User Guide.' }
foreach ($file in @('README.md','LICENSE','NOTICE.md')) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $file)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $package $file)).Hash) { throw "Outdated document: $file" }
}
Write-Host 'PASS: license and documentation are current; no source archive included.'
$original = (Get-FileHash -LiteralPath (Join-Path $repo 'build\TheCuelist_artefacts\Release\The Cuelist.exe')).Hash
$extracted = (Get-FileHash -LiteralPath (Join-Path $package 'The Cuelist.exe')).Hash
if ($original -ne $extracted) { throw 'Extracted executable differs from the verified build.' }
$sourceOnly = Join-Path $checkRoot 'source-only'
New-Item -ItemType Directory -Path $sourceOnly | Out-Null
$launcher = Join-Path $sourceOnly 'Avvia.cmd'
Copy-Item -LiteralPath (Join-Path $repo 'Avvia.cmd') -Destination $launcher
$commandLine = 'call "' + $launcher + '" <nul'
$output = & $env:ComSpec /d /c $commandLine
if ($LASTEXITCODE -ne 1) { throw 'Source-only launcher returned an unexpected exit code.' }
if (($output -join "`n") -notmatch 'source code, but not the compiled application') { throw 'Package instructions are missing.' }
Write-Host 'PASS: ZIP extracted with all required files.'
Write-Host 'PASS: executable SHA256 matches the verified build.'
Write-Host 'PASS: source-only launcher displays instructions and does not compile.'
Write-Host "Verified folder: $package"
if ($SmokeLaunch) {
    $application = Start-Process -FilePath (Join-Path $package 'The Cuelist.exe') -ArgumentList '--offline' -WorkingDirectory $package -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 2
    $application.Refresh()
    if ($application.HasExited) { throw "Portable launch failed: early exit $($application.ExitCode)." }
    if (!$application.CloseMainWindow()) { throw 'Could not request closure of the test window.' }
    if (!$application.WaitForExit(5000)) { throw 'The test window did not close within five seconds.' }
    if ($application.ExitCode -ne 0) { throw "Portable shutdown error: $($application.ExitCode)." }
    Write-Host 'PASS: extracted executable started offline and exited normally.'
}
# The source-only launcher intentionally returns 1. Clear that expected native exit
# status, otherwise GitHub Actions' PowerShell wrapper marks a passing check as failed.
$global:LASTEXITCODE = 0
