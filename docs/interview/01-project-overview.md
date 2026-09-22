# 01 项目总览

对应简历条目：项目概述 / 技术栈。本文是所有讲解的起点，被问"介绍一下你的引擎"时从这里开始。

## 电梯陈述（30 秒）

> 这是我独立开发的一个跨后端现代化 3D 游戏引擎，从零搭底层架构。核心是四件事：一套跨平台 RHI 硬件抽象（D3D12/Vulkan/OpenGL 三后端 + Bindless），一套基于拓扑排序的 RenderGraph 帧图（自动依赖分析、Pass 裁剪、屏障推导、分层并行录制），一套参考 UE 的 Proxy/SceneInfo + PSO 缓存驱动的网格绘制管线，以及一套基于 CoreCLR 的 C# 脚本系统，支持热重载。全程用 C++20 和 Vulkan/D3D12 实现，Debug 下接 Tracy 做性能分析。

## 2 分钟版

在 30 秒版基础上，每个模块补一句"为什么这么设计"：

- **RHI 为什么基于 NVRHI fork 再自研 GL 后端**：D3D12/Vulkan 后端一致性与 validation 层已经很成熟，把精力集中在真正的差异化价值上——自研 OpenGL 后端 + 上层架构；自研 GL 后端同时证明了完全吃透抽象层。
- **为什么做 RenderGraph**：手写渲染顺序时资源状态切换、屏障、临时 RT 生命周期极易出错，且难以并行。
- **为什么参考 UE 的 MeshDrawingPipeline**：逐帧重建渲染状态（pipeline/binding/root signature）CPU 开销巨大，需要命令缓存与合批。
- **为什么 C# 用 CoreCLR 手工宿主**：想要类似 Unity 的脚本体验（托管、热重载、GC），又不引入 mono 的老运行时。

## 技术栈

| 类别 | 选型 |
|---|---|
| 语言 | C++20（引擎）、C# net10.0（脚本 GreenCake） |
| 图形 API | D3D12（默认）/ Vulkan 1.3 / OpenGL 4.5 Core（自研后端） |
| RHI | cutie-rhi（NVRHI fork，`engine/external/cutie-rhi/`） |
| 脚本运行时 | CoreCLR，hostfxr 手工宿主 |
| 调试/分析 | ImGui + imnodes、Tracy（Debug）、RenderDoc / Nsight |
| 其他 | CMake、Assimp、stb、GLFW、Qt6（Cakery 编辑器） |

## 分层架构

```text
游戏侧   World ECS 系统（TaskScheduler 按读写依赖分层并行）
         → ResourceManager 惰性加载 → RenderCommandQueue
渲染侧   RenderSystem → RenderScene(脏标记 diff) → GpuScene
         → RenderViewFamily(视锥剔除) → RenderGraph(资源/屏障/裁剪)
         → DrawCommandList 命令录制 → 提交
抽象层   GfxContext / GfxBackend / Gfx* Proxy（gfx.h）
         → cutie-rhi（NVRHI fork）
后端     D3D12(默认) / Vulkan 1.3 / OpenGL 4.5(自研后端)
```

分层文档见 `docs/reference/rendering/README.md`，与上图一致。

## 三条贯穿全局的设计主线

被问"你的引擎有什么核心设计"时，说这三条显得有体系：

1. **Proxy + 惰性实体化**：`GfxTexture` 等只是持 desc 的轻量代理，真正 GPU 对象在 `initializeGpu()` 时才创建，从而支持"任意线程创建资源、渲染线程统一实体化"。
2. **命令录制优先**：所有渲染操作先录进 `DrawCommandList` 命令流（线性分配器 + 侵入式链表，零虚调用），再由提交线程回放到 cutie `ICommandList`。
3. **后端无关**：上层（RenderGraph/Pass/Material）只面对 `Gfx*` 抽象；后端差异全部封装在 `GfxBackend` 与 cutie 内，配合能力探测自动降级。

## 模块地图（runtime 目录速查）

| 子目录 | 职责 |
|---|---|
| `function/render/render_graph/` | RenderGraph 声明式帧图（见 02） |
| `function/render/mesh_draw/` | MeshBatch / MeshDrawCommand / DrawCache（见 03） |
| `function/render/pipeline_state/` | PSO 缓存 key 与内存缓存 |
| `function/render/render_pipeline/` | Renderer/Feature/Pass 组织 |
| `function/render/gpu_driven/` | GpuScene 镜像 + compute 剔除 + indirect |
| `function/graphics/` | GfxContext / GfxBackend / Proxy / DrawCommandList（见 06） |
| `function/script/` | CoreCLR 宿主 / glue / 热重载（见 07） |
| `core/thread/` `core/async/` | RenderThread / ThreadPool / TaskScheduler（见 05） |
| `core/meta/` | 反射运行时（见 07 的反射一节） |
| `metaparser/` | libclang 代码生成器 |
| `service/reference/` | BaselineRenderer 固定管线基准渲染器 |
| `service/debug/` | RenderGraphPanel 可视化 / DebugImGui |
| `editor/` | Cakery Qt 编辑器 |

## 工程组织

- 双产物：`Cakery.exe`（Qt 编辑器）与 `DodoeSandbox.exe`（运行时沙盒），CMake preset 分开。
- 预编译管线：`DodoePreCompile` target 跑 metaparser 生成反射/序列化/脚本绑定，`splice_generated.py` 把生成片段注入固定标记位。
- 脚本程序集：引擎核心 `GreenCake.dll`（`engine/src/scriptcore/`）+ 用户游戏集（`dotnet build` 动态编译）。

## 高频追问

- **"项目里最难的部分是什么？"** → 建议答自研 OpenGL 后端（要同时伺候"现代 API 语义翻译"与"GL 隐式状态机现实"，见 06）或双线程同步（正确性 + 性能 + 可调试性三角，见 05）。
- **"和直接用 Unity/UE 比你的收获？"** → 明白引擎层每一步的代价：一次 draw call 前发生了什么、一次资源创建跨线程走什么路径、一次热重载内部多少道工序。
- **"哪些是抄 UE 的、哪些是自己的？"** → 范式是 UE 的（Proxy/SceneInfo/MeshPass/RenderGraph 思想），实现是自己的；差异点在 GL 后端、C# 热重载链路、帧图可视化面板。
