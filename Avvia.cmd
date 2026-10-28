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
echo This folder contains the source code, but not the compiled application.
echo You do NOT need to install Visual Studio to run The Cuelist.
echo.
echo Extract the matching The-Cuelist-*-Windows-x64.zip release archive
echo and open "The Cuelist.exe" from the extracted folder.
echo Alternatively, copy the executable into the same folder as this launcher.
echo.
pause
exit /b 1
