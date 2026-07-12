@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0.."

set HAS_MISS=0

echo [1/4] git submodule...
git submodule update --init --recursive >nul 2>&1 || echo [FAIL]

echo [2/4] Vulkan SDK...
set VK_OK=0
if defined VULKAN_SDK if exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" set VK_OK=1
if !VK_OK!==0 for /d %%d in ("C:\VulkanSDK\*") do if exist "%%d\Include\vulkan\vulkan.h" set VK_OK=1
if !VK_OK!==1 (echo        OK) else (
    echo        MISS  winget install KhronosGroup.VulkanSDK
    set HAS_MISS=1
)

echo [3/4] .NET SDK...
where dotnet >nul 2>&1
if !ERRORLEVEL!==0 (dotnet --list-sdks 2>nul) else (
    echo        MISS  winget install Microsoft.DotNet.SDK.10
    set HAS_MISS=1
)

echo [4/4] Qt6...
set QT_OK=0
where qmake >nul 2>&1
if !ERRORLEVEL!==0 (for /f %%i in ('qmake -query QT_INSTALL_PREFIX 2^>nul') do set QT_OK=1)
if !QT_OK!==0 for /d %%d in ("C:\Qt\*.*.*\msvc*") do if exist "%%d\bin\qmake.exe" set QT_OK=1
if !QT_OK!==1 (echo        OK) else (
    echo        MISS  pip install aqtinstall ^&^& aqt install-qt windows desktop 6.11.1 win64_msvc2022_64
    set HAS_MISS=1
)

echo.
if !HAS_MISS!==1 (echo Fix missing deps above.) else (echo All deps found.)
echo cmake --preset msvc-debug-editor
echo cmake --build --preset msvc-debug-editor
endlocal
