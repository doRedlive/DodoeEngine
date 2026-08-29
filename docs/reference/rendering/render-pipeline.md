# 渲染管线与 RenderGraph

本文覆盖 `engine/src/runtime/function/render/` 的管线层:Renderer/Feature/Pass 组织、RenderGraph 的资源与屏障管理、视图剔除与 GPU driven 路径。

## 1. 管线组织:Pipeline → Renderer → Feature → Pass

```text
RenderPipeline(render_pipeline.cpp)
  └─ BaseRenderer 子类(按 RenderSettings::GetRenderingPipelineType())
       ├─ DeferredRenderer(默认):GBuffer 延迟路径
       ├─ ForwardRenderer
       ├─ Only2DRenderer:Sprite/UI/PostProcess2D/Present(2D 游戏)
       └─ OnlyGUIRenderer:仅 UI/ImGui/Present
            每个注册若干 IRenderFeature,feature->collectPasses(PassCollector)
            bakePasses():按 RenderPhase 升序排序 + blackboard 校验
```

`RendererCreateInfo`:worker_count(0 = hardware_concurrency,传给 renderer 自有 ThreadPool)、gfx_context、shared_render_service。

**RenderPhase 顺序**(render_phase.h,UInt8 排序键):

```text
Opaque → Shadow → Skybox → Lighting → Decals → Transparent
→ Sprite → PostProcess → UI → EditorGizmo → DebugUI → Present
```

**DeferredRenderer 注册的 Feature 与 Pass**:

| Feature | Pass | 职责 |
|---|---|---|
| OpaqueSceneFeature | GBufferPass(延迟)/ OpaquePass(前向)+ TransparentPass | 组织 LitMeshProcessor、MeshDrawCache,产出 GBuffer/HDR |
| ShadowSceneFeature | ShadowPass | 阴影图 |
| SkyboxFeature | SkyboxPass | 天空盒 |
| LightingFeature | DeferredLightPass | 读 GBuffer + ShadowMap 合成 HDR 光照 |
| PostProcessFeature | PostProcessPass | ToneMapping → SceneColor |
| SpriteFeature | SpritePass | 2D 精灵批渲染 |
| UIFeature | UIPass | 运行时 UI 实例 |
| GizmoFeature(仅编辑器) | GizmoPass | 编辑器 gizmo |
| ImGuiFeature(仅调试) | ImGuiPass | 调试 UI |
| PresentFeature | PresentPass | 拷贝到 ImportedBackBuffer 呈现 |

## 2. 渲染器每帧流程(DeferredRenderer::render,deferred_renderer.cpp:78)

```text
initViews:clearViewExtensions + view_family.buildVisiblePrimitives(scene)
OpaqueSceneFeature::setupMeshPassContexts(scene, view_family)
culling_path 分支:
    GpuOnly / CpuThenGpuVerify → executeGpuCulling + buildGpuDrivenDrawCommands
    CpuOnly                    → buildMeshDrawCommands(CPU 剔除 + processor 编命令)
ShadowSceneFeature::buildShadowDrawCommands
buildOrderedPasses(...):
    每个 view:RenderGraphBuilder 构建(pass->build 声明资源读写)
              graph.compile() → graph.execute(*m_thread_pool, ...)
```

## 3. RenderGraph(render_graph/)

### 3.1 资源模型

`RenderGraphResourceRecord` 支持 4 种来源(render_graph_resource.h:18):

| 来源 | 解析方式 |
|---|---|
| `Transient` | 从 `RenderGraphTransientPool` acquire(池化复用,不足则新建) |
| `ImportedTexture/Buffer` | 外部直接传入的 `GfxTextureHandle/GfxBufferHandle` |
| `ImportedBackBuffer` | `gfx_context.getSwapchainTextures()[swapchain_image_index]` |

**TransientPool**(render_graph_transient_pool.cpp):`acquireTexture/acquireBuffer` 线性扫描未占用槽,desc 全字段匹配即复用;`releaseAll()` 每帧帧末把 in_use 清零(池生命周期间接由 frame scheduler 管理)。跨帧持有瞬态资源是错误用法。

### 3.2 compile()(render_graph.cpp:374)

1. `buildDependencyGraph`:按 pass 读写访问建边(写→读、写→写),记录 first/last writer/reader;
2. `validateAccesses`:未初始化读、"写而未读未导出"、UAV 冲突断言;
3. `cullUnreachablePasses`:从 exported + backbuffer 资源反向可达标记,不可达 pass 裁剪(`NeverCull` 标志豁免);
4. `deriveBarriers`:按 pass 顺序追踪 `GfxResourceStates`,生成每个 pass 的 `pre_barriers`;
5. `topologicalSort`:Kahn 分层,同层 pass 无依赖可并行;有环断言。

### 3.3 execute()(render_graph.cpp:397)

```cpp
const bool direct_mode = out_commands.isImmediate()
                      || context.gfx_context->getOpenGLBackend() != nullptr;   // :408
```

- **direct_mode(GL 恒真)**:逐层、逐 pass 在调用线程内联执行——barriers → `setupPassAttachments` → `beginMarker` → `pass->execute(pass_context, out_commands)`。
- **并行分支(D3D12/Vulkan)**:每层为每个 pass 建独立 `DrawCommandList`,`pool.enqueue` 投递;worker lambda **先进入 `GfxRenderScope`**(资源创建走立即路径),执行 barriers/attachments/pass,`WaitGroup` 等整层完成;最后各 pass 命令流 `out_commands.append(std::move(...))` O(1) 拼接,保持层级顺序。

`setupPassAttachments`(render_graph.cpp:410):

- color slot 含 ImportedBackBuffer → 直接用 `getSwapchainFramebuffer`(断言:不可与其他附件组合、LoadOp 不可为 Clear);
- 否则解析 color/depth 纹理 → `setTextureState`(RT/DepthWrite)→ `commitBarriers` → 按 LoadOp 清屏 → `cmd.createFramebuffer(fb_desc)` → 写入 `pass_context`(连同 `FramebufferInfo`,供 PSO 匹配)。

### 3.4 Pass 上下文

`RenderGraphPassContext`(render_graph_pass.h:61):`getView/getScene/getShaderLibrary/getPipelineStateCache/getTextureManager`、`resolveTexture/resolveBuffer`、`getFramebuffer()`、`getRenderTargetSignature()`(= FramebufferInfo,PSO 缓存 key 的一部分)。

Pass 定义采用 lambda 风格:`graph.addPass<TParameters>(name, flags, setup, execute)`——setup 阶段声明资源读写(决定依赖与屏障),execute 阶段录制命令。IRenderPass 子类(如 SpritePass)则在 `build()` 中向 blackboard 写入产物 key(如 `SceneColorKey`),供下游 pass 消费。

## 4. 视图与剔除(render_view/)

- **RenderView**:view/proj/view_proj 矩阵 + viewport rect + ViewExtension 容器。
  - `buildVisiblePrimitives`:从 view_proj 抽取 6 个视锥平面,对每个 `PrimitiveSceneInfo` 做相交测试,结果写入 `MeshViewExtension.visible_primitives`(含 editor-only 过滤)。
  - `buildVisibleSprites`:对 `SpriteSceneInfo` 做球/AABB 相交 → `SpriteViewExtension.visible_sprites`。
- **RenderViewFamily**:多 view 容器 + 帧时间;`buildVisiblePrimitives/buildVisibleSprites` 遍历全部 view。
- **RenderViewTarget / RenderViewport**:每个视口 target 维护 logical/window/pixel 三层尺寸与 letterbox 计算;`buildViewFamily(scene, time, delta, view, proj)` 把"相机 + 视口"物化为 RenderViewFamily。

## 5. CPU 剔除与 Draw 命令(mesh_draw/)

- `IMeshPassProcessor` 基类(mesh_processor_base.cpp):视锥剔除工具 + `BuildDrawCommand`(填 MeshDrawCommand:passType、Primitive 常量 binding set、VB/IB 绑定、draw args)。
- `SubmitMeshDrawCommands`:对每个 `MeshDrawInstance` 写 primitive 常量缓冲(`writeBuffer`)→ 组 `GfxGraphicsState`(framebuffer/pipeline/viewport)→ `ShaderParameterBinder` 绑 Global/View/Pass/Primitive 各 set → `setGraphicsState` + `drawIndexed`。
- 具体处理器:`LitMeshProcessor`(Global/View/Primitive/Sampler 四套 layout,输出到 MeshDrawCommandCache 或动态命令)、`ShadowMeshProcessor`(仅 Global/View)。
- `MeshPassType` 是 PSO 缓存 key 的组成部分。

## 6. GPU Driven 路径(gpu_driven/)

激活条件(bindless + compute queue,由 `RenderSettings::ResolveFeatures` 决定):

- **GpuScene**(gpu_scene.cpp):全场景对象在 GPU 侧的镜像。八个持久缓冲:`object_meta / transforms / bounds / sprite_instance / primitive_instance / light_instance / quad_vb / quad_ib`。脏跟踪用 `DirtyRange`(区间 + 64 位 bitmap),`flushUpdates` 按脏位稀疏上传。`getPassResources()` 输出 `GpuScenePassResources` 供 pass import。
- **GpuCulling**(gpu_driven_renderer.cpp):3 条 compute pipeline。
  - `executeCulling`:CPU 组装视锥参数(6 平面 + object_count)→ 清零可见计数 → compute dispatch(`(count+63)/64`)输出可见对象索引列表 → visible_count 回读;
  - `executeBucketBuild`:按 bucket 统计/填充,最终写 `indirect_args_buffer`(并置 `IndirectArgument` 状态)。
- **buildGpuDrivenDrawCommands**(deferred_renderer.cpp:133):绑定 pipeline + 顶点流(slot 1 为实例缓冲)→ `drawIndexedIndirect(0, object_count)` 一次绘制全部。

CPU/GPU 两条路径由 `CullingPath` 设置切换(`CpuOnly / GpuOnly / CpuThenGpuVerify`)。

## 7. SpritePass 详解(render_sprite_pass.cpp)

build 阶段:优先写 blackboard 的 `SceneHdrKey`(有深度时附加 depth write + depth_occlusion),否则消费 `SceneColorKey`,再否则自建 transient "SpriteColor";汇总可见 sprite 实例(含批量),`stable_sort` 按 `(sorting_key, atlas_index)`;创建 transient instance buffer / VP 常量缓冲;import `quad_vb/quad_ib`。

execute 阶段按 `IsBindlessActive()` 分两条路径:

| 路径 | 绑定 | 绘制 |
|---|---|---|
| bindless | 全局 DescriptorTable + cb/sampler 两个 layout | 纹理经 `atlas_index`(= bindless descriptor index)在 shader 内索引,**一次 drawIndexed(6, N)** |
| traditional | per-atlas texture binding set(经 `resolveTextureBySlot`) | 实例已按 atlas 排序,按连续段分批 `drawIndexed(6, count, startInstance)` |

混合状态:SrcAlpha / OneMinusSrcAlpha;depth test 可选、不写深。
