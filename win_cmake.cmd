@echo off

SET SCRIPT_DIR=%~dp0
cd %SCRIPT_DIR%

echo "Removing build directory if exist..."
if exist build\ (rmdir /s/q build)

mkdir build

echo "Generating project for VS Code:"
cmake -G "Visual Studio 18 2026" -A x64 -S "." -B "build"

pause
