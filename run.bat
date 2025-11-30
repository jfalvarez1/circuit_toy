@echo off
REM Circuit Playground Run Script
REM Builds if needed, then runs the application

REM Build first (will copy DLL if needed)
call build.bat
if errorlevel 1 exit /b 1

echo.
echo Starting Circuit Playground...
start "" "build\circuit-playground.exe"
