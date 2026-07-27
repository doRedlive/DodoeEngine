@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0.."

set HAS_MISS=0

echo [1/6] git submodule...
git submodule update --init --recursive >nul 2>&1 || echo [FAIL]

echo [2/6] Vulkan SDK...
set VK_OK=0
if defined VULKAN_SDK if exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" set VK_OK=1
if !VK_OK!==0 for /d %%d in ("C:\VulkanSDK\*") do if exist "%%d\Include\vulkan\vulkan.h" set VK_OK=1
if !VK_OK!==1 (echo        OK) else (
    echo        MISS  winget install KhronosGroup.VulkanSDK
    set HAS_MISS=1
)

echo [3/6] .NET SDK...
where dotnet >nul 2>&1
if !ERRORLEVEL!==0 (dotnet --list-sdks 2>nul) else (
    echo        MISS  winget install Microsoft.DotNet.SDK.10
    set HAS_MISS=1
)

echo [4/6] Qt6...
set QT_OK=0
where qmake >nul 2>&1
if !ERRORLEVEL!==0 (for /f %%i in ('qmake -query QT_INSTALL_PREFIX 2^>nul') do set QT_OK=1)
if !QT_OK!==0 for /d %%d in ("C:\Qt\*.*.*\msvc*") do if exist "%%d\bin\qmake.exe" set QT_OK=1
if !QT_OK!==1 (echo        OK) else (
    echo        MISS  pip install aqtinstall ^&^& aqt install-qt windows desktop 6.11.1 win64_msvc2022_64
    set HAS_MISS=1
)

echo [5/6] Python Embedded...
set PY_OK=0
if exist "engine\external\python\python313.dll" set PY_OK=1
if !PY_OK!==1 (
    echo        OK
) else (
    echo        Downloading Python 3.13 embeddable...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$url='https://www.python.org/ftp/python/3.13.11/python-3.13.11-embed-amd64.zip';" ^
        "$tmp=\"$env:TEMP\python-embed.zip\";" ^
        "Invoke-WebRequest -Uri $url -OutFile $tmp -UseBasicParsing;" ^
        "Expand-Archive -Path $tmp -DestinationPath 'engine\external\python' -Force;" ^
        "Remove-Item $tmp"
    if exist "engine\external\python\python313.dll" (
        echo        OK
    ) else (
        echo        FAIL  manual: https://www.python.org/downloads/windows/
        set HAS_MISS=1
    )
)

echo [6/6] LLVM libclang ^(DodoeParser^)...
set LLVM_OK=0
if exist "engine\src\metaparser\3rd_party\LLVM\bin\x64\libclang.dll" set LLVM_OK=1

if !LLVM_OK!==0 (
    for %%d in (
        "C:\Program Files\LLVM"
        "%LOCALAPPDATA%\Programs\LLVM"
    ) do if exist "%%~d\bin\libclang.dll" (
        echo        Copying from %%~d
        mkdir "engine\src\metaparser\3rd_party\LLVM\bin\x64" >nul 2>&1
        mkdir "engine\src\metaparser\3rd_party\LLVM\lib\x64" >nul 2>&1
        copy /y "%%~d\bin\libclang.dll" "engine\src\metaparser\3rd_party\LLVM\bin\x64\" >nul
        if exist "%%~d\lib\libclang.lib" copy /y "%%~d\lib\libclang.lib" "engine\src\metaparser\3rd_party\LLVM\lib\x64\" >nul
        set LLVM_OK=1
    )
)

if !LLVM_OK!==1 (
    echo        OK
) else (
    echo        MISS  winget install LLVM.LLVM --version 18.1.8
    echo              then re-run this script to auto-copy libclang
    set HAS_MISS=1
)

echo.
if !HAS_MISS!==1 (echo Fix missing deps above.) else (echo All deps found.)
echo cmake --preset msvc-debug-editor
echo cmake --build --preset msvc-debug-editor
endlocal
