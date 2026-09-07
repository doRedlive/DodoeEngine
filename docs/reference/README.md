# Dodoe Reference

| 模块 | 目录与入口 |
|---|---|
| **引擎核心(Core / Memory / Resource / Tooling / Code Style)** | [core/README.md](core/README.md) |
| Graphics | [rendering/graphics.md](rendering/graphics.md) |
| Render | [rendering/render.md](rendering/render.md) |
| **Rendering 架构体系(RHI / 线程 / 管线 / RenderGraph / Shader / Material)** | [rendering/README.md](rendering/README.md) |
| **World / Simulation / UI-Input-Window** | [modules/README.md](modules/README.md) |
| **Scripting(引擎侧 / C# 参考)** | [scripting/README.md](scripting/README.md) |
| **Editor / Cakery Qt / Qt 基础** | [editor/README.md](editor/README.md) |

## 目录结构

```text
reference/
├── README.md          本索引
├── core/              引擎核心:core.md、memory.md、resource.md、tooling.md、code-style.md
├── rendering/         渲染架构:rhi、cutie-rhi、threading、frame-flow、render-pipeline、
│                      render-graph、lighting-ibl、resources、shader-system、material-system、
│                      graphics.md、render.md(模块速查表)
├── modules/           运行时功能模块:world.md、simulation.md、ui-input-window.md
├── scripting/         脚本:scripting.md、csharp-scripting.md
└── editor/            编辑器:editor.md、cakery-qt-architecture.md、qt-basics.md
```
