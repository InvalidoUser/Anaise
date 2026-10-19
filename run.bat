@echo off
rem Launch the built game.
setlocal
cd /d "%~dp0"
if not exist bin\HollowSpire.exe (
    echo [error] bin\HollowSpire.exe is missing. Run build.bat first.
    exit /b 1
)
start "" bin\HollowSpire.exe %*
