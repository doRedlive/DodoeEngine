#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
HAS_MISS=0

echo "[1/4] git submodule..."
git submodule update --init --recursive >/dev/null 2>&1 || echo "       FAIL"

echo "[2/4] Vulkan SDK..."
VK_OK=0
[ -n "${VULKAN_SDK:-}" ] && [ -f "$VULKAN_SDK/include/vulkan/vulkan.h" ] && { echo "       $VULKAN_SDK"; VK_OK=1; }
if [ "$VK_OK" -eq 0 ]; then
    for d in "$HOME/VulkanSDK"/*/ /usr/local /usr; do
        [ -f "$d/include/vulkan/vulkan.h" ] && { echo "       $d"; VK_OK=1; break; }
    done
fi
[ "$VK_OK" -eq 0 ] && { echo "       MISS  https://vulkan.lunarg.com/"; HAS_MISS=1; }

echo "[3/4] .NET SDK..."
if command -v dotnet >/dev/null 2>&1; then
    dotnet --list-sdks 2>/dev/null | sed 's/^/       /'
else
    echo "       MISS  https://dotnet.microsoft.com/download"
    HAS_MISS=1
fi

echo "[4/4] Qt6..."
QT_OK=0
QMAKE="$(command -v qmake || command -v qmake6 || true)"
[ -n "$QMAKE" ] && PREFIX="$("$QMAKE" -query QT_INSTALL_PREFIX 2>/dev/null || true)"
[ -n "${PREFIX:-}" ] && { echo "       $PREFIX"; QT_OK=1; }
[ "$QT_OK" -eq 0 ] && { echo "       MISS  pip install aqtinstall"; echo "             https://www.qt.io/download"; HAS_MISS=1; }

echo
[ "$HAS_MISS" -eq 0 ] && echo "All deps found." || echo "Fix missing deps above."
echo "cmake --preset msvc-debug-editor"
echo "cmake --build --preset msvc-debug-editor"
