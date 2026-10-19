@echo off
rem Build The Hollow Spire with MinGW-w64 (g++ and windres on PATH).
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul || (echo [error] g++ was not found on PATH. Install MinGW-w64. & exit /b 1)
where windres >nul 2>nul || (echo [error] windres was not found on PATH. Install MinGW-w64 binutils. & exit /b 1)

if not exist build mkdir build
if not exist bin mkdir bin

echo [1/2] compiling resources...
windres src\app.rc -O coff -o build\app.res || exit /b 1

echo [2/2] compiling the game...
g++ -std=c++20 -O2 -municode -mwindows ^
    src\main.cpp src\game.cpp build\app.res ^
    -o bin\HollowSpire.exe ^
    -static -static-libgcc -static-libstdc++ ^
    -lgdi32 -luser32 -lshell32 -lmsimg32 -ldwmapi -lole32 || exit /b 1

echo.
echo Built bin\HollowSpire.exe
