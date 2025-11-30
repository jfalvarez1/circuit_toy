@echo off
REM Circuit Playground Clean Rebuild Script for Windows
REM This script removes the build folder and rebuilds from scratch

echo Cleaning build directory...
if exist "build" (
    rmdir /s /q build
)

echo.
call build.bat

@echo off
REM Circuit Playground Run Script
REM Builds if needed, then runs the application

REM Build first (will copy DLL if needed)
call build.bat
if errorlevel 1 exit /b 1

echo.
echo Starting Circuit Playground...
start "" "build\circuit-playground.exe"
