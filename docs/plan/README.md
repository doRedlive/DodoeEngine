# Plan Reference Map

| Plan | Reference |
|---|---|
| [SRP C# API 设计](srp-csharp-api-design.md) | [Render](../reference/rendering/render.md) -> [Graphics](../reference/rendering/graphics.md) -> [Scripting](../reference/scripting/scripting.md) -> [Code Style](../reference/core/code-style.md) |
| [Shader 参数宏统一](shader-parameter-macro-unify.md) | [Render](../reference/rendering/render.md) -> [Graphics](../reference/rendering/graphics.md) -> [Code Style](../reference/core/code-style.md) |
| [Prefab 系统](dodoe-prefab-system.md) | [World](../reference/modules/world.md) -> [Resource](../reference/core/resource.md) -> [Core](../reference/core/core.md) -> [Editor](../reference/editor/editor.md) -> [Code Style](../reference/core/code-style.md) |
| [ImGui 多视口](imgui-multi-viewport-support.md) | [UI、Input、Window](../reference/modules/ui-input-window.md) -> [Render](../reference/rendering/render.md) -> [Graphics](../reference/rendering/graphics.md) -> [Code Style](../reference/core/code-style.md) |
| [Cakery 双产品架构](cakey-editor-only-.md) | [Editor](../reference/editor/editor.md) -> [World](../reference/modules/world.md) -> [Render](../reference/rendering/render.md) -> [UI、Input、Window](../reference/modules/ui-input-window.md) -> [Code Style](../reference/core/code-style.md) |
| [Cakery 双产品进度](cakery-editor-only-progress.md) | [Editor](../reference/editor/editor.md) -> [World](../reference/modules/world.md) -> [Render](../reference/rendering/render.md) -> [Code Style](../reference/core/code-style.md) |
| [Cakery Runtime 交接](cakery-runtime-handoff.md) | [Editor](../reference/editor/editor.md) -> [World](../reference/modules/world.md) -> [Render](../reference/rendering/render.md) -> [UI、Input、Window](../reference/modules/ui-input-window.md) -> [Code Style](../reference/core/code-style.md) |
| [网络模块](dodoe-network-module.md) | [Core](../reference/core/core.md) -> [World](../reference/modules/world.md) -> [Scripting](../reference/scripting/scripting.md) -> [Code Style](../reference/core/code-style.md) |

## Reference 模块

| 模块 | 内容 |
|---|---|
| [Core](../reference/core/core.md) | Application、SystemContext、事件、线程、反射、序列化、Project |
| [Graphics](../reference/rendering/graphics.md) | GfxContext、DrawCommandList、DrawExecutor、D3D12/Vulkan/OpenGL backend |
| [Render](../reference/rendering/render.md) | RenderGraph、Pipeline、Feature、Pass、View、Scene、Shader、Material、GPU-driven |
| [World](../reference/modules/world.md) | World、Scene、Entity、Components、Systems、场景序列化 |
| [Resource](../reference/core/resource.md) | Asset、AssetManager、Importer、FileID、SceneRes/EntityRes/ComponentRes |
| [Scripting](../reference/scripting/scripting.md) | ScriptSystem、ScriptEngine、ScriptRuntime、ScriptGlue、ScriptHub、C# API |
| [UI、Input、Window](../reference/modules/ui-input-window.md) | Runtime UI、ImGui、InputManager、WindowManager |
| [Physics、Animation、Time](../reference/modules/simulation.md) | Physics world、animation、TimeSystem |
| [Editor](../reference/editor/editor.md) | EditorSession、Document、History、Backend、Cakery UI |
| [Parser、Generated、Sandbox](../reference/core/tooling.md) | Meta parser、生成代码、Sandbox |
| [Code Style](../reference/core/code-style.md) | C++ 类型、命名、类布局、render pass 模式 |
