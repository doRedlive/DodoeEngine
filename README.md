# dodoe
dodoe says hello !

---

## Prerequisites / 前置依赖

- **Git** — `winget install Git.Git`
- **CMake** ≥ 3.20 — `winget install Kitware.CMake`
- **Vulkan SDK** — `winget install KhronosGroup.VulkanSDK`
- **.NET SDK** ≥ 10.0 — `winget install Microsoft.DotNet.SDK.10`
- **Qt6** ≥ 6.5 — `pip install aqtinstall && aqt install-qt windows desktop 6.11.1 win64_msvc2022_64`
- **Visual Studio** 2026+ with "Desktop development with C++"

## Setup / 配置

```bash
git submodule update --init --recursive
scripts/setup.bat        # Windows
bash scripts/setup.sh    # Linux / macOS
```

## Build / 构建

| Preset | 产物 |
|---|---|
| `msvc-editor-debug` | `bin/editor-debug/Cakery.exe` |
| `msvc-sandbox-debug` | `bin/sandbox-debug/DodoeSandbox.exe` |

```bash
cmake --preset msvc-editor-debug
cmake --build --preset msvc-editor-debug
```
