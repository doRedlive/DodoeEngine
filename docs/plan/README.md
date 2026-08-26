# Plan Reference Map

| Plan | Reference |
|---|---|
| [SRP C# API 设计](srp-csharp-api-design.md) | [Render](../reference/render.md) -> [Graphics](../reference/graphics.md) -> [Scripting](../reference/scripting.md) -> [Code Style](../reference/code-style.md) |
| [Shader 参数宏统一](shader-parameter-macro-unify.md) | [Render](../reference/render.md) -> [Graphics](../reference/graphics.md) -> [Code Style](../reference/code-style.md) |
| [Prefab 系统](dodoe-prefab-system.md) | [World](../reference/world.md) -> [Resource](../reference/resource.md) -> [Core](../reference/core.md) -> [Editor](../reference/editor.md) -> [Code Style](../reference/code-style.md) |
| [ImGui 多视口](imgui-multi-viewport-support.md) | [UI、Input、Window](../reference/ui-input-window.md) -> [Render](../reference/render.md) -> [Graphics](../reference/graphics.md) -> [Code Style](../reference/code-style.md) |
| [Cakery 双产品架构](cakey-editor-only-.md) | [Editor](../reference/editor.md) -> [World](../reference/world.md) -> [Render](../reference/render.md) -> [UI、Input、Window](../reference/ui-input-window.md) -> [Code Style](../reference/code-style.md) |
| [Cakery 双产品进度](cakery-editor-only-progress.md) | [Editor](../reference/editor.md) -> [World](../reference/world.md) -> [Render](../reference/render.md) -> [Code Style](../reference/code-style.md) |
| [Cakery Runtime 交接](cakery-runtime-handoff.md) | [Editor](../reference/editor.md) -> [World](../reference/world.md) -> [Render](../reference/render.md) -> [UI、Input、Window](../reference/ui-input-window.md) -> [Code Style](../reference/code-style.md) |
| [网络模块](dodoe-network-module.md) | [Core](../reference/core.md) -> [World](../reference/world.md) -> [Scripting](../reference/scripting.md) -> [Code Style](../reference/code-style.md) |

## Reference 模块

| 模块 | 内容 |
|---|---|
| [Core](../reference/core.md) | Application、SystemContext、事件、线程、反射、序列化、Project |
| [Graphics](../reference/graphics.md) | GfxContext、DrawCommandList、DrawExecutor、D3D12/Vulkan/OpenGL backend |
| [Render](../reference/render.md) | RenderGraph、Pipeline、Feature、Pass、View、Scene、Shader、Material、GPU-driven |
| [World](../reference/world.md) | World、Scene、Entity、Components、Systems、场景序列化 |
| [Resource](../reference/resource.md) | Asset、AssetManager、Importer、FileID、SceneRes/EntityRes/ComponentRes |
| [Scripting](../reference/scripting.md) | ScriptSystem、ScriptEngine、ScriptRuntime、ScriptGlue、ScriptHub、C# API |
| [UI、Input、Window](../reference/ui-input-window.md) | Runtime UI、ImGui、InputManager、WindowManager |
| [Physics、Animation、Time](../reference/simulation.md) | Physics world、animation、TimeSystem |
| [Editor](../reference/editor.md) | EditorSession、Document、History、Backend、Cakery UI |
| [Parser、Generated、Sandbox](../reference/tooling.md) | Meta parser、生成代码、Sandbox |
| [Code Style](../reference/code-style.md) | C++ 类型、命名、类布局、render pass 模式 |
