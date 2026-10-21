@echo off
rem Launch the built app.
setlocal
cd /d "%~dp0"
if not exist bin\Anaise.exe (
    echo [error] bin\Anaise.exe is missing. Run build.bat first.
    exit /b 1
)
start "" bin\Anaise.exe %*
