@echo off
setlocal
cd /d "%~dp0"
if exist "build\TheCuelist_artefacts\Release\The Cuelist.exe" (
    start "" "build\TheCuelist_artefacts\Release\The Cuelist.exe"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" -Run
    if errorlevel 1 pause
)
