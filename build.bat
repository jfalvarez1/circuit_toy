@echo off
REM Circuit Playground Build Script for Windows
REM This script handles building and copying required DLLs

echo Building Circuit Playground...

REM Check if build directory exists, if not run meson setup
if not exist "build" (
    echo Setting up build directory...
    meson setup build
    if errorlevel 1 (
        echo Meson setup failed!
        exit /b 1
    )
)

REM Build the project
meson compile -C build
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

REM Copy SDL2 DLL to build directory if it exists in subprojects
if exist "build\subprojects\SDL2-2.32.8\SDL2-8.dll" (
    echo Copying SDL2-8.dll to build directory...
    copy /Y "build\subprojects\SDL2-2.32.8\SDL2-8.dll" "build\"
) else if exist "build\subprojects\SDL2-2.30.0\SDL2.dll" (
    echo Copying SDL2.dll to build directory...
    copy /Y "build\subprojects\SDL2-2.30.0\SDL2.dll" "build\"
) else (
    REM Try to find any SDL2 DLL in subprojects
    for /r "build\subprojects" %%f in (SDL2*.dll) do (
        echo Copying %%~nxf to build directory...
        copy /Y "%%f" "build\"
        goto :done_copy
    )
)
:done_copy

echo.
echo Build complete! Run: build\circuit-playground.exe
