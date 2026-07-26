# DoDoE 渲染系统架构参考

## 概述

DoDoE 是一个基于 NVIDIA cutie RHI 的游戏引擎渲染系统，支持 Vulkan / DX12 / OpenGL 后端，采用延迟渲染管线。GPU-driven 能力正在建设中。

> **⚠️ 强制规则：任何对渲染系统的更改，必须同步更新本文档。**
> 包括但不限于：新增/删除 pass、修改 GpuScene API、改变数据流路径、新增 buffer 类型、修改 RenderGraph 契约、更新 shader 注册方式。

## 目录结构

```
engine/src/runtime/function/render/
├── render_system.h/.cpp          # 顶层，主循环
├── renderer.h/.cpp               # RenderCommandQueue：Game Thread 提交入口<br>├── render_command.h              # RenderCommand 类型定义
├── render_pipeline/
│   ├── render_pipeline.h/.cpp    # 工厂：按 RenderingPipelineType 创建 IRenderer
│   ├── renderer.h/.cpp           # IRenderer 接口 + RendererBase 共享基类
│   ├── deferred_renderer.h/.cpp  # DeferredRenderer：GBuffer + Shadow + DeferredLight
│   ├── only_2d_renderer.h/.cpp   # Only2DRenderer：Sprite + PostProcess2D
│   ├── render_pass_context.h     # RenderPassContext + PassType
│   ├── render_feature/           # IRenderFeature 插件
│   │   ├── render_feature.h      # 接口
│   │   ├── sprite_feature.h/.cpp
│   │   ├── imgui_feature.h/.cpp
│   │   └── render_builtin_features.h/.cpp  # GBuffer/Shadow/Skybox/Lighting/PostProcess
│   └── passes/                   # 各 pass 实现
│       ├── render_sprite_pass.cpp
│       ├── render_base_pass.cpp  # GBuffer
│       ├── render_shadow_pass.cpp
│       ├── render_deferred_light_pass.cpp
│       ├── render_post_process_pass.cpp
│       ├── render_imgui_pass.cpp
│       └── render_present_pass.cpp
├── render_graph/                 # RDG (Render Dependency Graph)
│   ├── render_graph_builder.h    # 对外 API
│   ├── render_graph.h/.cpp       # 内部 compile/execute
│   └── render_pass_blackboard_keys.h
├── render_scene/
│   ├── render_scene.h/.cpp       # CPU 权威场景数据
│   ├── render_object.h           # RenderObject 基类
│   ├── sprite_scene_info.h       # SpriteSceneInfo + SpriteInstance + QuadVertex
│   ├── primitive_scene_info.h    # 3D mesh 场景信息
│   └── light_scene_info.h        # 灯光信息
├── render_view/
│   ├── render_view.h/.cpp        # Per-view frustum + 扩展容器
│   ├── render_viewport.h/.cpp    # 视口管理
│   ├── sprite_view_extension.h   # visible_sprites 列表
│   └── mesh_view_extension.h     # 可见 primitive + draw commands
├── gpu_driven/
│   ├── gpu_scene.h/.cpp          # GPU 场景数据库
│   ├── gpu_scene_buffers.h       # GPU 数据结构定义
│   ├── gpu_dirty_flags.h         # 脏标记枚举
│   ├── gpu_object_id.h           # GpuObjectHandle / GpuObjectType / ResourceIndex
│   └── gpu_driven_renderer.h/.cpp # GpuCulling：compute culling + indirect args
├── framework/
│   ├── shader_library.h/.cpp     # 所有 shader 的加载与持有
│   ├── descriptor_table_manager.h/.cpp  # Bindless 描述符表
│   ├── texture_manager.h/.cpp    # 纹理加载与管理
│   ├── pipeline_state_cache.h/.cpp # PSO 缓存
│   ├── shared_render_service.h/.cpp # 聚合 gfx/shader/pso/texture
│   └── local_vertex_factory.h/.cpp # Per-platform input layout 创建
├── mesh_draw/                    # Mesh 绘制命令
│   ├── mesh_processor_base.h
│   ├── gbuffer_mesh_processor.h/.cpp
│   └── directional_shadow_mesh_processor.h/.cpp
└── render_settings.h             # RenderSettings 静态配置

engine/res/shaders/               # HLSL shader 源码
engine/res/shaders/bin/           # 编译产物 (.dxil / .spv)
engine/src/runtime/function/graphics/
├── gfx.h                         # 所有 Gfx* 类型别名（指向 cutie）
├── gfx_context.h/.cpp            # 设备创建、swapchain、GPU-driven 检测
└── draw_command_list.h/.cpp      # 延迟命令列表（Deferred Command Buffer）
```

## RenderCommandQueue

`RenderCommandQueue`（`render/renderer.h`）是渲染命令提交入口，提供静态 API 供 Game Thread 的 ECS System 调用：

- `AddPrimitive` / `RemovePrimitive` / `UpdatePrimitiveTransform`
- `AddSprite` / `RemoveSprite` / `UpdateSpriteTransform`
- `AddLight` / `RemoveLight` / `UpdateLightTransform`

内部根据 `ThreadingMode` 决定走 **直通路径**（SingleThread 直接操作 RenderScene）还是 **入队路径**（多线程下封装 `RenderCommand` push 到 `SpscQueue`，由 RenderThread 消费）。

## IRenderer 抽象

### 接口层次

```
IRenderer (纯接口)
  ├── render(view_family, scene, swapchain_index, cmd_list) = 0
  └── RendererBase (共享基础设施)
        ├── DeferredRenderer   — GBuffer + Shadow + DeferredLight + PostProcess
        └── Only2DRenderer     — Sprite + PostProcess2D (无 3D pass)
```

### 职责划分

| 层级 | 职责 |
|------|------|
| `IRenderer` | 纯接口，定义 `render()` 契约 |
| `RendererBase` | 线程池、共享服务、Feature 容器、`buildFrameDrawCommandList()` / `executeFrameGraph()` |
| `DeferredRenderer` | Mesh processors (GBuffer/Shadow)、PSO 构建、`setupMeshPassContexts()`、`buildMeshDrawCommands()` |
| `Only2DRenderer` | 轻量：只注册 2D 相关 Feature，跳过所有 mesh 流程 |

### RenderPipeline 角色变化

`RenderPipeline` 已从"自己做所有事"简化为**工厂**：
- `initialize()`：读取 `RenderSettings::GetRenderingPipelineType()`，创建对应的 `IRenderer`
- `render()`：委托给 `m_renderer->render()`
- 后续新增 ForwardRenderer 只需在工厂 switch 中添加一个分支

### 关键设计点

1. **Feature 组合由 Renderer 决定**。`DeferredRenderer::initialize()` 注册 `BaseSceneFeature + LightingFeature + PostProcessFeature + SpriteFeature + ImGuiFeature + PresentFeature`；`Only2DRenderer::initialize()` 只注册 `SpriteFeature + PostProcess2DFeature + ImGuiFeature + PresentFeature`
2. **Mesh 处理完全在 DeferredRenderer 内**。`Only2DRenderer` 没有 `m_mesh_processors` / `m_local_vertex_factory`
3. **GPU-driven culling 不是独立的 Renderer**，而是 DeferredRenderer 内部可选的 culling 策略（`GpuDrivenRenderer` 作为 culling 服务）
4. **RenderPassContext::isValid()** 只检查公共字段，不再依赖 `RenderingPipelineType`

## RHI 层 (cutie)

引擎不直接调用 D3D12/Vulkan/GL，而是通过 `cutie` 库（`engine/external/cutie-rhi/include/cutie/cutie.h`）的 C++ 接口。

### 类型别名规则

`gfx.h` 中所有 `Gfx*` 类型都是 `cutie::*` 的别名或引用计数包装：

- `GfxDeviceHandle` = `cutie::DeviceHandle` (RefCountPtr)
- `GfxBufferHandle` = `Ref<GfxBuffer>`（懒创建：`initializeRHI(device)` 模式）
- `GfxTextureHandle` = `Ref<GfxTexture>`
- `GfxGraphicsPipelineHandle` = `Ref<GfxGraphicsPipeline>`
- `GfxComputePipelineHandle` = `cutie::ComputePipelineHandle`
- `GfxShaderHandle` = `cutie::ShaderHandle`

### 引擎包装类 (Ref-counted)

`GfxBuffer`、`GfxTexture` 等引擎包装类使用延迟 RHI 初始化模式：

```cpp
auto buffer = create_ref<GfxBuffer>(desc);  // 创建 CPU 侧对象
buffer->initializeRHI(device);               // 延迟创建 GPU 资源
// 之后通过 buffer->getRHIHandle() 获取 cutie::IBuffer*
```

### 间接绘制支持

cutie RHI 层**已完整支持**，但 `DrawCommandList` 刚补齐接口：

| 方法 | 状态 |
|------|------|
| `drawIndirect(offset, count)` | ✅ `DrawCommandList` 已封装 |
| `drawIndexedIndirect(offset, count)` | ✅ |
| `dispatchIndirect(offset)` | ✅ |
| `GfxBufferDesc::isDrawIndirectArgs` | ✅ flag 存在 |
| `GfxResourceStates::IndirectArgument` | ✅ |
| `GfxGraphicsState::indirectParams` | ✅ 需在 setGraphicsState 前设置 |

### Bindless 描述符表

通过 `DescriptorTableManager` 管理，引擎维护一个 `GfxDescriptorTable`：
- 容量：1024
- 可见性：所有 shader stage
- Register space：仅 Texture_SRV（slot 0）
- 去重：按 `GfxBindingSetItem` hash，重复纹理复用索引
- 动态扩容：满时 2x 扩（保留旧内容）
- 每纹理加载时自动分配 `DescriptorIndex`

访问方式：`texture->getDescriptorIndex()` 返回 bindless 索引，shader 中用该索引从 descriptor table 读取纹理。

## 主循环数据流

```
Game Thread
  │ RenderCommand (AddPrimitive/AddSprite/AddLight/UpdateTransform)
  ▼
RenderSystem::renderFrame()
  ├── 消费 command queue → RenderScene::addPrimitive/addSprite/addLightSceneInfo
  ├── RenderScene::flushUpdates() → GpuScene 增量同步
  ├── RenderViewport::buildViewFamily() → CPU frustum culling → ViewExtension
  └── RenderPipeline::render()
        ├── 选择 IRenderer (DeferredRenderer / Only2DRenderer)
        ├── initViews() → CPU frustum culling → ViewExtension
        ├── setupMeshPassContexts() → instance_scene_data + light matrix (Deferred only)
        ├── buildMeshDrawCommands() → per-pass MeshDrawCommand 数组 (Deferred only)
        └── buildFrameDrawCommandList()
              ├── 遍历 IRenderFeature → registerPass(graph)
              ├── graph.compile() → 拓扑排序
              └── graph.execute() → 执行 pass
                    └── Present
```

## RenderGraph (RDG)

### 核心概念

- **RenderGraphBuilder**：pass 注册入口，提供 `createTransientTexture/Buffer`、`importTexture/Buffer`、`read`/`write`
- **RenderGraphPassBuilder**：单个 pass 的 setup lambda，通过 `pass_builder.read(handle)` / `pass_builder.write(handle)` 声明资源访问
- **RenderGraphImportRegistry**：Feature 在建图前发布可 import 的长期资源；Pass 通过类型 key 查询
- **RenderGraphBlackboard**：pass 间传递 RDG 数据；每个 key 用 `using Value = ...` 固定值类型
- **RenderGraphPassFlags**：`Raster`、`Compute`、`Copy`、`NeverCull`、`AsyncCompute`

### Pass 标准结构

```cpp
struct MyPassParameters {
    RenderGraphTextureHandle input{};
    RenderGraphBufferHandle output{};
};

struct SomeKey {
    using Value = RenderGraphTextureHandle;
};

graph.addPass<MyPassParameters>(
    "PassName",
    RenderGraphPassFlags::Raster,
    // Setup lambda — 声明资源依赖
    [&](RenderGraphPassBuilder& builder, MyPassParameters& params) {
        params.input = builder.read(*builder.blackboard().get<SomeKey>());
        params.output = builder.write(builder.createTransientBuffer(desc, "name"));
    },
    // Execute lambda — 实际操作
    [&](const MyPassParameters& params, const RenderGraphPassContext& ctx, DrawCommandList& cmd) {
        auto input = ctx.resolveTexture(params.input);
        auto output = ctx.resolveBuffer(params.output);
        // ... draw ...
    }
);
```

### 关键规则

1. **Setup 阶段只声明依赖，不创建 GPU 资源**。GPU 资源在 execute 阶段由 RDG 按需分配。
2. **importBuffer/importTexture** 用于外部持久资源（如 GpuScene buffer），不会被 RDG 生命周期管理。
3. **createTransientBuffer/Texture** 的生命周期由 RDG 管理，pass 之间通过 write→read 依赖自动排序。
4. **Blackboard** 是 pass 间通信的唯一通道。

## GpuScene（GPU 场景数据库）

### 定位
RenderScene = CPU 权威数据，GpuScene = GPU 权威数据，Pass 只消费 GpuScene。

### Buffer 清单

| Buffer | Element | 说明 |
|--------|---------|------|
| `object_meta` | `GpuObjectMeta` | 对象类型/flag/material/texture/bounds 索引 |
| `transforms` | `GpuTransform` | local_to_world + world_to_local |
| `bounds` | `GpuBounds` | center + extent + sphere_radius |
| `sprite_instance` | `SpriteGpuData` | Sprite 専用 instance 数据 |
| `primitive_instance` | `PrimitiveGpuData` | 3D mesh 専用 instance 数据 |
| `quad_vb` / `quad_ib` | `QuadVertex` / `UInt16` | 共享 quad mesh |

### 核心 API

```cpp
GpuObjectHandle registerObject(GpuObjectType type, GpuObjectMeta meta);
void unregisterObject(GpuObjectHandle handle);
void markDirty(GpuObjectHandle handle, GpuObjectDirtyFlags flags);
void updateTransform(handle, matrix);
void updateBounds(handle, center, extent);
void updateSpriteInstance(handle, SpriteGpuData);
void updatePrimitiveInstance(handle, PrimitiveGpuData);
void flushUpdates(DrawCommandList&);    // 增量上传 dirty ranges
GpuScenePassResources getPassResources() const;
```

### 增量上传机制

- 每个 buffer 维护一个 `DirtyRange {start, end}`（连续区间）
- `markDirty` / `update*` 方法 expand 对应 dirty range
- `flushUpdates` 只上传 dirty range 内的数据：`writeBuffer(buffer, data+offset, size, offset)`
- Buffer 扩容时重置 dirty range 为全量（新 buffer 需要全量初始化）

### 对象注册流程

```
RenderScene::rebuildPipelineSceneData()
  ├── pending_sprite_updates: Removed? → m_gpu_scene->unregisterObject(handle)
  ├── pending_sprite_updates: Added?   → m_gpu_scene->registerObject(GpuObjectType::Sprite, meta)
  │                                       → m_gpu_scene->updateSpriteInstance(handle, data)
  ├── pending_sprite_updates: Transform? → m_gpu_scene->updateTransform(handle, matrix)
  │                                        → m_gpu_scene->updateBounds(handle, center, extent)
  └── m_gpu_scene->flushUpdates(cmd_list)
```

## GpuDrivenRenderer（GPU Driven 渲染器）

### 职责
- 执行 compute culling → 生成 `VisibleObjects` / `VisibleCount` buffer
- 管理 `IndirectArgsBuffer`（每个 pass 的 indirect draw 参数）

### Culling Compute Shader

`gpu_culling_pass_cs.hlsl`：
- 输入：`ObjectMetaBuffer(t0)`、`TransformBuffer(t1)`、`BoundsBuffer(t2)`、`CullingParams(b0)`
- 输出：`VisibleObjects(u0)`（compact 的 object index 数组）、`VisibleCount(u1)`（atomic counter）
- 每个线程处理一个 object（64 线程/组）

### executeCulling 流程

1. 第一次调用：创建 binding layout + compute pipeline + output buffers
2. 每帧：从 view-projection 提取 6 个 frustum planes → 上传到 constant buffer
3. 清零 `VisibleCount`（writeBuffer zero）
4. `setComputeState` + bind GpuScene SRVs + UAVs
5. `dispatch(ceil(object_count/64), 1, 1)`
6. Barrier: visible buffers → IndirectArgument / ShaderResource

## 关键数据流路径

### Sprite 渲染路径

```
1. Game Thread → RenderCommand(AddSprite) → RenderScene::addSprite()
2. RenderScene::flushUpdates()
   → upsertSpriteSceneInfo()         # 更新 CPU SpriteSceneInfo
   → rebuildPipelineSceneData()
     → GpuScene::registerObject(Sprite, meta)
     → GpuScene::updateSpriteInstance(handle, data)
     → GpuScene::flushUpdates(cmd)   # 增量上传 sprite_instance buffer
3. RenderPipeline::render()
   → GpuDrivenRenderer::executeCulling()  # (可选) GPU culling
   → SpritePass: import GpuScene sprite_instance + quad buffers
     → drawIndexed(6, visible_count)      # 或 drawIndexedIndirect
```

### Mesh 渲染路径（暂未 GPU-driven）

```
1. Game Thread → RenderCommand(AddPrimitive) → RenderScene::addPrimitive()
2. RenderScene::flushUpdates()
   → upsertPrimitiveSceneInfo()     # 更新 CPU PrimitiveSceneInfo
3. RenderPipeline::render()
   → buildVisiblePrimitives()       # CPU frustum culling
   → setupMeshPassContexts()        # instance_scene_data + mesh pass relevance
   → buildMeshDrawCommands()        # per-pass MeshDrawCommand 数组
   → GBufferPass/ShadowPass         # drawIndexed with CPU commands
```

## 重要约定

1. **`// do@Redlive`** 是文件头注释，显示作者身份
2. **`// dodoe`** 是命名空间结束注释
3. **Managed<> 生命周期**：`Create()` 工厂方法，`Destroy()` 析构，禁止拷贝
4. **注释风格**：不使用中文或英文注释，只保留文件头 `// do@Redlive`，代码本身不写注释
5. **GfxBuffer 创建**：使用 builder 模式 `.setByteSize(N).setIsVertexBuffer(true).enableAutomaticStateTracking(state).setDebugName("name")`
6. **状态转换**：必须成对 `setBufferState(copyDest) → writeBuffer → setBufferState(target)`，中间 `commitBarriers()`

## 外部 RHI 依赖 (cutie)

### 支持的 Shader 类型
`Vertex`, `Hull`, `Domain`, `Geometry`, `Pixel`, `Amplification`, `Mesh`, `Compute`

### 支持的特性查询
- `Feature::HeapDirectlyIndexed` — bindless descriptor 支持（D3D12 heap / Vulkan descriptor indexing）
- `Feature::ComputeQueue` — 异步 compute 队列

### 间接 Buffer 能力
- `BufferDesc::isDrawIndirectArgs` — 标记 buffer 用于间接参数
- `ResourceStates::IndirectArgument` (0x10) — 间接参数 buffer 状态
- `GraphicsState::indirectParams` / `ComputeState::indirectParams` — 绑定间接参数 buffer
- `DrawIndirectArguments` = {vertexCount, instanceCount, startVertex, startInstance}
- `DrawIndexedIndirectArguments` = {indexCount, instanceCount, startIndex, baseVertex, startInstance}
- `DispatchIndirectArguments` = {groupsX, groupsY, groupsZ}

### Shader 文件扩展名
| 类型 | DX12 | Vulkan |
|------|------|--------|
| Vertex | `.vert.dxil` | `.vert.spv` |
| Fragment | `.frag.dxil` | `.frag.spv` |
| Compute | `.comp.dxil` | `.comp.spv` |
| Geometry | `.geom.dxil` | `.geom.spv` |

### Shader 加载流程
```cpp
auto source = ReadShaderFile("engine/res/shaders/bin/" + filename);
auto shader = GDrawCommandList.createShader(
    GfxShaderDesc().setShaderType(GfxShaderType::Compute).setEntryName("main"),
    source.data(), source.size());
```

## GpuScene 重构历史

### 旧架构问题
1. GpuScene 每帧全量删除+重建（`render_scene.cpp:457-486`）
2. Sprite 有专用 `SlotMap<SpriteGpuData>` 和 `registerSprite()`，与 primitive 分裂
3. Sprite pass 有双路径：GpuScene import vs transient buffer fallback + per-texture batching
4. 所有 culling 在 CPU 端做线性 for 循环
5. 无 indirect draw，所有 draw 走直接 `drawIndexed`

### 新架构 (重构后)
1. 统一 `registerObject(GpuObjectType, meta)` + `updateSpriteInstance/PrimitiveInstance`
2. 增量上传：`DirtyRange` + `flushDirtyRange()` 替代全量 `writeBuffer(0)`
3. Sprite pass 只有一条路径：import GpuScene buffer + bindless 绘制
4. `GpuDrivenRenderer` 提供 compute culling + indirect args
5. `DrawCommandList` 已补齐 `drawIndirect` / `drawIndexedIndirect` / `dispatchIndirect`
6. RenderScene 增量同步：按 `pending_*_updates` 粒度同步到 GpuScene，`m_cpu_to_gpu_map` 跨帧保持
