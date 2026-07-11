@echo off
setlocal

cd /d "%~dp0.."

set "BUILD_DIR=build"
set "QT_PATH=C:/Qt/6.11.1/msvc2022_64"
set "TARGET=%1"
if "%TARGET%"=="" set "TARGET=Cakery"

cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH="%QT_PATH%" -DDODOE_EDITOR=ON
if %ERRORLEVEL% NEQ 0 exit /b 1

endlocal
