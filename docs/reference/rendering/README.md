# Rendering 架构体系

本目录是 Dodoe 渲染子系统的完整架构参考,覆盖从游戏逻辑线程到 GPU 提交的完整链路。既包含底层图形抽象(RHI),也包含实际使用这些抽象的上层系统(World 系统、资源加载、渲染管线)。

## 分层总览

```text
┌─────────────────────────────────────────────────────────────────┐
│  游戏侧(Game/World 线程,TaskScheduler 并行)                     │
│    SpriteRendererSystem / MeshRendererSystem / LightSystem ...   │
│    组件(PPtr 引用) → ResourceManager 惰性加载 → RenderCommandQueue │
├─────────────────────────────────────────────────────────────────┤
│  渲染侧(RenderThread / 主线程,视 ThreadingMode)                  │
│    RenderSystem → RenderScene(脏标记)→ GpuScene(CPU 镜像上传)     │
│    RenderViewFamily(视锥剔除)→ RenderPipeline(Feature/Pass)      │
│    RenderGraph(资源/屏障/裁剪)→ DrawCommandList(命令录制)         │
├─────────────────────────────────────────────────────────────────┤
│  图形抽象层(RHI)                                                 │
│    GfxContext / GfxBackend / Gfx* proxy(gfx.h)                   │
│    → cutie-rhi(NVRHI fork):IDevice / ICommandList               │
├─────────────────────────────────────────────────────────────────┤
│  图形后端                                                         │
│    D3D12(默认)/ Vulkan 1.3 / OpenGL 4.5(自研后端)               │
└─────────────────────────────────────────────────────────────────┘
```

## 核心设计原则

1. **线程分离**:游戏逻辑多线程(TaskScheduler 按系统读写依赖分层级并行),GPU 提交单线程(RHI 不变式:同一时刻只有一个线程触碰设备)。两线程间只通过无锁命令队列和命令流通信。
2. **Proxy + 惰性实体化**:引擎层资源(`GfxTexture`/`GfxBuffer` 等)是轻量 Proxy,持有 `desc` 与 `m_rhi_ready` 标志;真正的 GPU 对象(`cutie::TextureHandle`)在 `initializeRHI()` 时才创建。这使"任意线程创建资源、渲染线程统一实体化"成为可能。
3. **命令录制优先**:所有渲染操作先录制到 `DrawCommandList` 命令流(线性分配器 + 侵入式链表,零虚调用开销),再由提交线程回放到 cutie `ICommandList`。
4. **后端无关**:上层(RenderGraph/Pass/Material)只面对 `Gfx*` 抽象;后端差异(上下文所有权、能力探测、交换链)全部封装在 `GfxBackend` 与 cutie 后端内。

## 文档导航

| 文档 | 内容 |
|---|---|
| [rhi.md](rhi.md) | RHI 层:cutie-rhi(NVRHI fork)接口体系、GfxContext/GfxBackend、gfx.h Proxy 资源、DrawCommandList 双模式命令录制 |
| [cutie-rhi.md](cutie-rhi.md) | cutie-rhi 库内部:基础设施(引用计数/格式/状态追踪)、IDevice 内存与上传机制、三后端命令列表实现对比、validation 层、OpenGL 后端详解 |
| [threading.md](threading.md) | 渲染多线程模型:ThreadingMode 三模式、RenderThread/DrawThread/TaskScheduler/ThreadPool、OpenGL 上下文所有权转移、GfxRenderScope 与资源创建时序 |
| [frame-flow.md](frame-flow.md) | 每帧流程:renderFrame 逐步时序、RenderFrameScheduler 帧槽与 in-flight、帧内存管理 |
| [render-pipeline.md](render-pipeline.md) | 渲染管线:Renderer/Feature/Pass 体系、RenderGraph(资源/屏障/裁剪/并行执行)、视图与剔除、GpuScene/GpuCulling |
| [resources.md](resources.md) | 资源体系:ResourceManager → TextureManager/Sprite/Mesh/Material 链路、PPtr 引用、渲染侧缓存(PSO/Framebuffer/BindingSet)、World 系统同步协议 |

## 与其他文档的关系

- [graphics.md](../graphics.md) / [render.md](../render.md):目录级速查表(文件与职责的索引)。
- [core.md](../core.md):`Ref<T>`/`Scope<T>` 智能指针、LinearAllocator、MPMC 队列等基础设施在本体系中被大量使用。
- [world.md](../world.md):World 系统如何被调度(本文档 threading.md 的"游戏侧"部分)。
