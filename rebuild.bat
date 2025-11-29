@echo off
REM Circuit Playground Clean Rebuild Script for Windows
REM This script removes the build folder and rebuilds from scratch

echo Cleaning build directory...
if exist "build" (
    rmdir /s /q build
)

echo.
call build.bat
