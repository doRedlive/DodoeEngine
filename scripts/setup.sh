#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
HAS_MISS=0

echo "[1/6] git submodule..."
git submodule update --init --recursive >/dev/null 2>&1 || echo "       FAIL"

echo "[2/6] Vulkan SDK..."
VK_OK=0
[ -n "${VULKAN_SDK:-}" ] && [ -f "$VULKAN_SDK/include/vulkan/vulkan.h" ] && { echo "       $VULKAN_SDK"; VK_OK=1; }
if [ "$VK_OK" -eq 0 ]; then
    for d in "$HOME/VulkanSDK"/*/ /usr/local /usr; do
        [ -f "$d/include/vulkan/vulkan.h" ] && { echo "       $d"; VK_OK=1; break; }
    done
fi
[ "$VK_OK" -eq 0 ] && { echo "       MISS  https://vulkan.lunarg.com/"; HAS_MISS=1; }

echo "[3/6] .NET SDK..."
if command -v dotnet >/dev/null 2>&1; then
    dotnet --list-sdks 2>/dev/null | sed 's/^/       /'
else
    echo "       MISS  https://dotnet.microsoft.com/download"
    HAS_MISS=1
fi

echo "[4/6] Qt6..."
QT_OK=0
QMAKE="$(command -v qmake || command -v qmake6 || true)"
[ -n "$QMAKE" ] && PREFIX="$("$QMAKE" -query QT_INSTALL_PREFIX 2>/dev/null || true)"
[ -n "${PREFIX:-}" ] && { echo "       $PREFIX"; QT_OK=1; }
[ "$QT_OK" -eq 0 ] && { echo "       MISS  pip install aqtinstall"; echo "             https://www.qt.io/download"; HAS_MISS=1; }

echo "[5/6] LLVM libclang (DodoeParser)..."
LLVM_SRC="engine/src/metaparser/3rd_party/LLVM"

if [ "$(uname)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ]; then
    # macOS arm64 uses Xcode's built-in libclang
    XCODE_CLANG="$(xcrun --show-sdk-path 2>/dev/null || echo)/../../Toolchains/XcodeDefault.xctoolchain/usr/lib/libclang.dylib"
    echo "       OK (Xcode built-in)"
elif [ "$(uname)" = "Linux" ]; then
    LLVM_DST="$LLVM_SRC/bin/Linux"
    if [ -f "$LLVM_DST/libclang.so.12" ]; then
        echo "       OK"
    else
        echo "       Downloading LLVM 18.1.8 x64 (libclang only)..."
        mkdir -p "$LLVM_DST" "$LLVM_SRC/lib/Linux"
        TMPDIR="$(mktemp -d)"
        URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-linux-gnu-ubuntu-18.04.tar.xz"
        if curl -fSL --connect-timeout 30 "$URL" -o "$TMPDIR/llvm.tar.xz" 2>/dev/null; then
            tar -xf "$TMPDIR/llvm.tar.xz" -C "$TMPDIR"
            EXTRACTED="$(find "$TMPDIR" -maxdepth 1 -type d -name 'clang+llvm*' | head -1)"
            cp "$EXTRACTED/lib/libclang.so.12" "$LLVM_DST/"
            cp "$EXTRACTED/lib/libclang.so"     "$LLVM_DST/" 2>/dev/null || true
            rm -rf "$TMPDIR"
            echo "       OK"
        else
            rm -rf "$TMPDIR"
            echo "       FAIL  sudo apt install libclang-18-dev"
            echo "             or download from https://github.com/llvm/llvm-project/releases"
            HAS_MISS=1
        fi
    fi
else
    # macOS x64
    LLVM_DST="$LLVM_SRC/bin/macOS"
    if [ -f "$LLVM_DST/libclang.dylib" ]; then
        echo "       OK"
    else
        echo "       Downloading LLVM 18.1.8 arm64 (libclang only)..."
        mkdir -p "$LLVM_DST" "$LLVM_SRC/lib/macOS"
        TMPDIR="$(mktemp -d)"
        URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-arm64-apple-darwin22.0.tar.xz"
        if curl -fSL --connect-timeout 30 "$URL" -o "$TMPDIR/llvm.tar.xz" 2>/dev/null; then
            tar -xf "$TMPDIR/llvm.tar.xz" -C "$TMPDIR"
            EXTRACTED="$(find "$TMPDIR" -maxdepth 1 -type d -name 'clang+llvm*' | head -1)"
            cp "$EXTRACTED/lib/libclang.dylib" "$LLVM_DST/"
            rm -rf "$TMPDIR"
            echo "       OK"
        else
            rm -rf "$TMPDIR"
            echo "       FAIL  brew install llvm@18"
            echo "             or download from https://github.com/llvm/llvm-project/releases"
            HAS_MISS=1
        fi
    fi
fi

echo "[6/6] Python (DODOE_PYTHON_ENABLED)..."
PY_DST="engine/external/python"
if [ -f "$PY_DST/python3.dll" ] || [ -f "$PY_DST/libpython3.13.dylib" ] || [ -f "$PY_DST/libpython3.13.so" ]; then
    echo "       OK"
elif command -v python3 >/dev/null 2>&1; then
    echo "       OK (system python3)"
else
    echo "       MISS  install python3.13-dev or set DODOE_PYTHON_ENABLED=OFF"
    HAS_MISS=1
fi

echo
[ "$HAS_MISS" -eq 0 ] && echo "All deps found." || echo "Fix missing deps above."
echo "cmake --preset msvc-debug-editor"
echo "cmake --build --preset msvc-debug-editor"
