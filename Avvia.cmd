@echo off
setlocal
cd /d "%~dp0"
if exist "The Cuelist.exe" (
    start "" "The Cuelist.exe"
    exit /b 0
)
if exist "portable\The Cuelist.exe" (
    start "" "portable\The Cuelist.exe"
    exit /b 0
)
if exist "build\TheCuelist_artefacts\Release\The Cuelist.exe" (
    start "" "build\TheCuelist_artefacts\Release\The Cuelist.exe"
    exit /b 0
)
echo.
echo Questa cartella contiene i sorgenti, ma non il programma compilato.
echo Per usare The Cuelist NON serve installare Visual Studio.
echo.
echo Estrai il pacchetto The-Cuelist-0.2.0-Windows-x64.zip
echo e apri "The Cuelist.exe" nella cartella estratta.
echo In alternativa, copia l'eseguibile nella cartella di questo launcher.
echo.
pause
exit /b 1
