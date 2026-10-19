@echo off
rem Build and run the scripted story test.
setlocal
cd /d "%~dp0\.."
if not exist build mkdir build
g++ -std=c++20 -O2 tests\game_path_test.cpp src\game.cpp -o build\game_tests.exe || exit /b 1
build\game_tests.exe
