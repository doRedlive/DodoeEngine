@echo off
setlocal
cd /d "%~dp0.."
set "TARGET=%1"
if "%TARGET%"=="" set "TARGET=Cakery"

if /i "%TARGET%"=="Cakery" (
    cmake --preset msvc-editor-debug
    if %ERRORLEVEL% NEQ 0 exit /b 1
    cmake --build --preset msvc-editor-debug
) else if /i "%TARGET%"=="Sandbox" (
    cmake --preset msvc-sandbox-debug
    if %ERRORLEVEL% NEQ 0 exit /b 1
    cmake --build --preset msvc-sandbox-debug
) else (
    echo Usage: build.bat [Cakery^|Sandbox]
    exit /b 1
)
endlocal
