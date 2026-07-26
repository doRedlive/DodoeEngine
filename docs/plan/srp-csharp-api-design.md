# Dodoe SRP C# API 设计

## 0. 设计前提

1. **C# 不能遍历可见物体**。C# 只做声明式绘制——指定 phase + 过滤条件，不接触 `PrimitiveSceneInfo*`、`MeshBatch*`、`MeshDrawCommand`、`Gfx*Handle`。
2. **C# 不能持有帧内数据到下一帧**。`TextureHandle`、`BufferHandle`、`ManagedRenderView` 等仅在 Build/Execute 期间有效。
3. **C# 负责 "what and when"（组织什么 Pass、何时执行），C++ 负责 "how"（怎么画、怎么绑定、怎么调度）**。
4. **Renderer 独属于 C++**。`IRenderer / RendererBase` 是执行引擎（culling、graph compile/execute、PSO、command 提交），C# 不可见也不可替换。C# 只有 `RenderPipeline → RenderFeature[] → RenderPass` 三层。
5. 跨帧持有的资产句柄（`MeshHandle`、`MaterialHandle`、`RTHandle`）走 ID + revision 模式，不暴露裸指针。

---

## 1. 总体架构

```
C# 层                                  C++ 层
──────                                 ──────
RenderPipeline                         RenderPipelineBase (已有)
  ├─ RenderFeature[]                     │
  │    └─ RenderPass.Build()             │
  │         ├─ graph 资源声明            │
  │         └─ cmd.DrawRenderers() ──→   RendererBase (执行引擎，C# 不可见)
  │                                      │
  └─ 只决定 Feature 组合                 ├─ prepareViews()
                                         ├─ buildMeshDrawCommands()
                                         ├─ graph compile / execute
                                         └─ 提交 DrawCommandList
```

- C# `RenderPipeline` 的唯一职责：组合 `RenderFeature[]`。内建实现（`DeferredPipeline`）提供默认组合；用户可以派生并替换。
- C++ `RendererBase` 拥有一帧的完整生命周期：prepare → graph build（遍历 C# Feature 调用 `AddRenderPasses`）→ compile → execute。
- `DrawRenderers()` 是 C# Pass 接触 C++ 执行引擎的唯一 mesh 绘制入口——内部自动从 `MeshViewExtension` 取已准备的 draw data，C# 侧完全无感。

---

## 2. C# 类型定义

### 2.1 资产句柄（跨帧持有，Initialize / OnResize / Dispose 管理）

```csharp
// 网格资产引用
public struct MeshHandle : IEquatable<MeshHandle>
{
    internal int id;
    internal int revision;
    public bool IsValid { get; }
    public static MeshHandle Invalid { get; }
}

// 材质资产引用
public struct MaterialHandle : IEquatable<MaterialHandle>
{
    internal int id;
    internal int revision;
    public bool IsValid { get; }
    public static MaterialHandle Invalid { get; }
}

// 持久化 RT 引用（Feature 在 Initialize 创建，OnResize 更新尺寸）
public struct RTHandle
{
    internal int id;
    public void Resize(int width, int height);
    public bool IsValid { get; }
}
```

对应的 C++ 侧：
- `MeshHandle` → 资产系统 mesh ID + revision
- `MaterialHandle` → `MaterialSystem` 中 material instance ID + revision
- `RTHandle` → `RenderTargetHandle*`（通过 `RenderTargetSystem` 管理）

### 2.2 帧内只读数据（仅在 Build / Execute 期间有效，禁止保存）

```csharp
// ── Pass.Build 的入口 ──
public struct PassBuildContext
{
    public ManagedRenderView view;        // 当前 view 的只读信息
    public RenderGraphBlackboard blackboard; // GBuffer / Depth / Shadow 等产品存取
}

// ── 只读 View 信息 ──
public struct ManagedRenderView
{
    // 相机矩阵
    public Matrix4x4 ViewMatrix { get; }
    public Matrix4x4 ProjectionMatrix { get; }
    public Matrix4x4 ViewProjectionMatrix { get; }

    // 视口
    public int ViewportX { get; }
    public int ViewportY { get; }
    public int ViewportWidth { get; }
    public int ViewportHeight { get; }

    // 相机世界位置
    public Vector3 CameraPosition { get; }

    // 时间
    public float DeltaTime { get; }
    public float TimeSeconds { get; }

    // 显式不允许：
    // - 遍历 visible objects
    // - 访问 PrimitiveSceneInfo / MeshBatch / MeshViewExtension
    // - 访问 GfxContext / DrawCommandList
}
```

### 2.3 执行回调中的绘制命令

```csharp
// ── Raster Pass 的 execute 回调 ──
public struct RasterCommandContext
{
    // 主路径：批量绘制场景 mesh（声明式），内部走 C++ MeshViewExtension
    public void DrawRenderers(MeshDrawSettings settings);

    // 补充路径：少量单独 mesh（gizmo / debug / procedural）
    public void DrawMesh(MeshHandle mesh, int subMesh,
                         MaterialHandle material, Matrix4x4 transform,
                         MaterialPropertyBlock? properties = null);

    // 全屏后处理（等价于绘制一个全屏三角形）
    public void DrawProcedural(MaterialHandle material);

    // Clear（仅对 graph 声明的 attachment 有效）
    public void ClearColor(int attachmentIndex, Color color);
    public void ClearDepth(float depth, byte stencil = 0);
}

// ── Compute Pass 的 execute 回调 ──
public struct ComputeCommandContext
{
    public void SetTexture(int slot, TextureHandle texture);
    public void SetBuffer(int slot, BufferHandle buffer);
    public void Dispatch(int x, int y, int z);
}
```

### 2.4 MeshDrawSettings — 核心的声明式绘制过滤

```csharp
public struct MeshDrawSettings
{
    public string Phase;                    // "GBuffer" / "DirectionalShadow" / ...
    public RenderQueueRange Queue;          // Opaque / Transparent / 自定义
    public int LayerMask;                   // ~0 = 所有层，1 << N = 特定层
    public MaterialHandle? MaterialOverride; // 可选：强制替换材质
    public bool EnableInstancing;

    public static MeshDrawSettings For(string phase)
        => new() { Phase = phase, LayerMask = ~0 };

    public MeshDrawSettings WithQueue(int min, int max)
        { Queue = new(min, max); return this; }

    public MeshDrawSettings WithLayerMask(int mask)
        { LayerMask = mask; return this; }

    public MeshDrawSettings WithMaterial(MaterialHandle mtl)
        { MaterialOverride = mtl; return this; }
}

public struct RenderQueueRange
{
    public int Min, Max;
    public static RenderQueueRange Opaque      => new(0, 2500);
    public static RenderQueueRange Transparent => new(2501, 5000);
    public RenderQueueRange(int min, int max) { Min = min; Max = max; }
}
```

### 2.5 Render Graph Blackboard（Pass 间数据交接）

```csharp
// C++ RenderGraphBlackboard 的托管包装
public class RenderGraphBlackboard
{
    public void Set<TKey, TValue>(TValue value);
    public TValue? Get<TKey, TValue>();
}

// 预定义的 key，C# 和 C++ 共用：
public struct GBufferKey { }
public struct SceneDepthKey { }
public struct ShadowMapKey { }
public struct SceneColorKey { }
public struct FxaaColorKey { }
public struct SpriteColorKey { }
public struct ImGuiColorKey { }
```

### 2.6 初始化服务

```csharp
// Pipeline / Feature 初始化时通过这个 facade 获取长期资源
public class PipelineServices
{
    // RT 管理
    public RTHandle CreateRenderTarget(RenderTargetDesc desc);
    public RTHandle FindRenderTarget(string name);

    // 资产加载
    public MeshHandle     LoadMesh(string path);
    public MaterialHandle LoadMaterial(string path);

    // 显式不提供：
    // - GfxContext / DrawCommandList / RenderScene
    // - PSO / Binding / RHI handle
}
```

---

## 3. C# 抽象类

### 3.1 三层协议

```csharp
// ── Pipeline：SRP 运行时根对象，唯一职责是组织 Feature 列表 ──
public abstract class RenderPipeline : IDisposable
{
    // 初始化：创建 Feature、分配长期资源
    public abstract void Initialize(RenderPipelineDefinition definition,
                                    PipelineServices services);

    // 返回本 Pipeline 拥有的 Feature（C++ RendererBase 每 frame 遍历）
    public abstract RenderFeature[] GetFeatures();

    public virtual void OnResize(int width, int height) { }
    public virtual void Dispose() { }
}

// ── Feature：可配置的长期能力模块 ──
public abstract class RenderFeature : IDisposable
{
    public virtual void Initialize(PipelineServices services) { }
    public virtual void OnResize(int width, int height) { }

    // 每个 view 调用一次：向 graph 注入 Pass
    public abstract void AddRenderPasses(RenderGraph graph, PassBuildContext context);

    public virtual void Dispose() { }
}

// ── Pass：一个逻辑步骤，只声明图节点和 execute 回调 ──
public abstract class RenderPass
{
    public abstract void Build(RenderGraph graph, PassBuildContext context);
}
```

### 3.2 C++ 侧如何消费

```
C++ RendererBase::render() 每帧流程：

  1. prepareViews(scene, views)           // culling + 填充 MeshViewExtension
  2. setupMeshPassContexts(scene, views)  // instance data、light、time
  3. buildMeshDrawCommands(views, cmd)    // PSO + cached/dynamic draw commands

  4. for each view:
       create RenderGraph
       for each feature in pipeline->GetFeatures():  ← C# 列表 + C++ 内建列表
           feature->AddRenderPasses(graph, ctx)       ← 可能是 C# 回调
       graph.compile()
       graph.execute()                                ← C# SetRenderFunc 在这里回调
```

C++ RendererBase 自己的 `m_features` 和 C# Pipeline 的 `GetFeatures()` 合并成一个 Feature 列表来遍历。`PipelineServices` 使得 C# Feature 能在初始化时通过 facade 访问 `RenderTargetSystem` / 资产系统。

---

## 4. 完整示例

### 4.1 内建 Deferred Pipeline

```csharp
public sealed class DeferredPipeline : RenderPipeline
{
    private RenderFeature[] _features = null!;

    public override void Initialize(RenderPipelineDefinition definition,
                                    PipelineServices services)
    {
        _features = new RenderFeature[] {
            new BaseSceneFeature(),     // GBuffer + Shadow
            new LightingFeature(),      // 延迟光照
            new PostProcessFeature(),   // 后处理
            new PresentFeature()        // 呈现到 swapchain
        };
        foreach (var f in _features) f.Initialize(services);
    }

    public override RenderFeature[] GetFeatures() => _features;

    public override void OnResize(int width, int height)
    {
        foreach (var f in _features) f.OnResize(width, height);
    }

    public override void Dispose()
    {
        for (int i = _features.Length - 1; i >= 0; i--)
            _features[i].Dispose();
    }
}
```

内建 Pipeline 只做一件事：按顺序排列 Feature。C++ host 拿到 `GetFeatures()` 后，和其他内建 C++ Feature 一起遍历调用 `AddRenderPasses()`。

### 4.2 内建 GBuffer Pass

```csharp
// GBuffer Pass 是 BaseSceneFeature 内部的 pass（通常是 C++ 实现）。
// 如果以 C# 写，长这样：
public sealed class GBufferPass : RenderPass
{
    public override void Build(RenderGraph graph, PassBuildContext context)
    {
        // 1. 声明 transient 资源
        var albedo = graph.CreateTexture(
            TextureDesc.Relative(1.0f, 1.0f, GraphicsFormat.RGBA8), "GBufferA");
        var normal = graph.CreateTexture(
            TextureDesc.Relative(1.0f, 1.0f, GraphicsFormat.RGBA16), "GBufferB");
        var position = graph.CreateTexture(
            TextureDesc.Relative(1.0f, 1.0f, GraphicsFormat.RGBA32), "GBufferC");
        var material = graph.CreateTexture(
            TextureDesc.Relative(1.0f, 1.0f, GraphicsFormat.RGBA8), "GBufferD");
        var depth = graph.CreateTexture(
            TextureDesc.Depth(1.0f, 1.0f, GraphicsFormat.D32), "GBufferDepth");

        // 2. 写入产品（后续 Pass 消费）
        context.Products.Set<GBufferKey>(new SceneTextures {
            Albedo = albedo, Normal = normal, Position = position,
            Material = material, Depth = depth
        });

        // 3. 注册 raster pass + execute 回调
        graph.AddRasterPass<GBufferPassData>("GBuffer", out var pass)
             .SetColorAttachment(albedo,   0, LoadAction.Clear)
             .SetColorAttachment(normal,   1, LoadAction.Clear)
             .SetColorAttachment(position, 2, LoadAction.Clear)
             .SetColorAttachment(material, 3, LoadAction.Clear)
             .SetDepthAttachment(depth, LoadAction.Clear)
             .SetRenderFunc((GBufferPassData data, RasterCommandContext cmd) =>
             {
                 cmd.ClearColor(0, new Color(0.08f, 0.09f, 0.11f));
                 cmd.ClearColor(1, new Color(0, 0, 0));
                 cmd.ClearColor(2, new Color(0, 0, 0));
                 cmd.ClearColor(3, new Color(0, 1, 1));
                 cmd.ClearDepth(1.0f);

                 cmd.DrawRenderers(
                     MeshDrawSettings.For("GBuffer")
                         .WithQueue(RenderQueueRange.Opaque)
                         .WithLayerMask(~0));
             });
    }
}
```

`cmd.DrawRenderers()` 内部自动从 `MeshViewExtension` 取对应 phase 的 cached/dynamic draw commands——C# 不需要知道 prepare 的存在。

### 4.3 自定义后处理 Pass（纯 C#，无 mesh 依赖）

```csharp
public sealed class BloomDownsamplePass : RenderPass
{
    private RTHandle _bloomRT;

    public BloomDownsamplePass(RTHandle bloomRT) { _bloomRT = bloomRT; }

    public override void Build(RenderGraph graph, PassBuildContext context)
    {
        var sceneColor = context.Products.Get<SceneColorKey, TextureHandle>()
                         ?? throw new("SceneColor missing");
        var bloomTex = graph.ImportTexture(_bloomRT, "BloomDown");

        graph.AddComputePass<BloomData>("Bloom.Downsample", out var pass)
             .ReadTexture(sceneColor)
             .WriteTexture(bloomTex)
             .SetComputeFunc((BloomData data, ComputeCommandContext cmd) =>
             {
                 cmd.SetTexture(0, sceneColor);
                 cmd.SetTexture(1, bloomTex);
                 cmd.Dispatch(
                     (context.view.ViewportWidth  + 31) / 32,
                     (context.view.ViewportHeight + 31) / 32,
                     1);
             });

        context.Products.Set<BloomDownKey>(bloomTex);
    }
}
```

### 4.4 Debug 绘制单个 mesh

```csharp
// 在 pass 的 execute 回调中，用于 gizmo / debug / procedural：
cmd.DrawMesh(
    _wireCubeMesh, 0, _debugMaterial,
    Matrix4x4.TRS(position, rotation, scale),
    new MaterialPropertyBlock { Color = Color.Yellow }
);
// 走 dynamic command 路径，仅限少量物体。
// 大量物体 → 提交到 RenderScene → C++ Cull → DrawRenderers 批量路径。
```

### 4.5 用户自定义 SRP（加 Outline Feature 的 Deferred）

```csharp
public sealed class MyOutlinePipeline : RenderPipeline
{
    private RenderFeature[] _features = null!;

    public override void Initialize(RenderPipelineDefinition definition,
                                    PipelineServices services)
    {
        _features = new RenderFeature[] {
            new BaseSceneFeature(),       // 内建：GBuffer + Shadow
            new MyOutlineFeature(),       // 用户自定义：edge mask
            new LightingFeature(),        // 内建：延迟光照
            new PostProcessFeature(),     // 内建：后处理
            new PresentFeature()          // 内建：呈现
        };
        foreach (var f in _features) f.Initialize(services);
    }

    public override RenderFeature[] GetFeatures() => _features;

    // OnResize / Dispose 遍历 _features...
}

public sealed class MyOutlineFeature : RenderFeature
{
    private RTHandle _outlineRT = null!;

    public override void Initialize(PipelineServices services)
    {
        _outlineRT = services.CreateRenderTarget(new RenderTargetDesc {
            Name = "OutlineColor", ScalePolicy = ScalePolicy.Relative,
            ScaleX = 1.0f, ScaleY = 1.0f, Format = GraphicsFormat.RGBA8
        });
    }

    public override void OnResize(int w, int h) => _outlineRT.Resize(w, h);

    public override void AddRenderPasses(RenderGraph graph, PassBuildContext context)
    {
        var gbuffer = context.Products.Get<GBufferKey, SceneTextures>();
        if (gbuffer == null) return;

        var outlineTex = graph.ImportTexture(_outlineRT, "OutlineColor");

        graph.AddRasterPass<OutlinePassData>("Outline.Mask", out var pass)
             .ReadTexture(gbuffer.Depth)
             .WriteTexture(outlineTex)
             .SetRenderFunc((OutlinePassData data, RasterCommandContext cmd) =>
             {
                 cmd.ClearColor(0, Color.Transparent);
                 cmd.DrawRenderers(
                     MeshDrawSettings.For("GBuffer")
                         .WithQueue(RenderQueueRange.Opaque));
             });

        context.Products.Set<OutlineKey>(outlineTex);
    }

    public override void Dispose() => _outlineRT.DisposeIfValid();
}
```

用户在 `MyOutlinePipeline` 里只需决定 Feature 列表——在内建 `BaseSceneFeature + LightingFeature + PostProcessFeature + PresentFeature` 中间插入自己的 `MyOutlineFeature`。C++ RendererBase 的一帧流程（prepare → graph build → compile → execute）完全不受影响。

---

## 5. C++ 侧需要的改动

### 5.1 MeshPhase 注册表

```cpp
// 从硬编码 enum 迁移到注册表，C# 用字符串引用
struct MeshPhaseDesc {
    String name;                    // "GBuffer", "DirectionalShadow", ...
    MeshPassType pass_type;        // 对应已有的 enum
    MeshPassFilter filter;         // opaque / transparent
};

class MeshPhaseRegistry {
public:
    void registerBuiltinPhases();  // 注册 GBuffer, DirectionalShadow
    const MeshPhaseDesc* find(const String& name) const;
};
```

第一版只注册已有的 `GBuffer` 和 `DirectionalShadow`。将来 C# 可引用已注册 phase，但创建新 phase 仍需要 C++ native 注册。

### 5.2 DrawRenderers 内部实现

```cpp
// 收到 C# 调用后，C++ 侧的处理逻辑：
void RasterCommandContext::DrawRenderers(const MeshDrawSettings& settings) {
    auto* phase = MeshPhaseRegistry::find(settings.phase);
    auto* mesh_ext = m_view->getExtension<MeshViewExtension>();

    // 根据 phase 拿对应的 draw instances
    auto& cached  = mesh_ext->cached_draw_instances[phase->pass_type];
    auto& dynamic = mesh_ext->dynamic_draw_instances[phase->pass_type];

    // Dispatch cached + dynamic（现有代码路径）
    MeshDrawCommandDispatcher::DispatchCached(/* ... cached ... */);
    MeshDrawCommandDispatcher::DispatchCached(/* ... dynamic ... */);
}
```

内部直接复用 `MeshViewExtension` 上已有的 draw data——这些 data 由 C++ `RendererBase::buildMeshDrawCommands()` 在 graph build 之前填入，C# 完全无感。

### 5.3 托管包装

| C# 类型 | C++ 包装 | 暴露内容 |
|---|---|---|
| `MeshHandle` | 资产系统 mesh ID + revision | `id`, `revision`, `IsValid` |
| `MaterialHandle` | `MaterialSystem` material ID + revision | `id`, `revision`, `IsValid` |
| `RTHandle` | `RenderTargetHandle*` | `id`, `Resize()`, `IsValid` |
| `ManagedRenderView` | `RenderView*` | 矩阵、viewport、时间（只读） |
| `RenderGraphBlackboard` | 已有的 `RenderGraphBlackboard` | `Set<T>`, `Get<T>` |
| `MeshDrawSettings` | struct（传值） | phase, queue, layer, override |
| `RasterCommandContext` | 执行回调参数 | `DrawRenderers`, `DrawMesh`, `DrawProcedural`, `Clear*` |
| `ComputeCommandContext` | 执行回调参数 | `SetTexture`, `SetBuffer`, `Dispatch` |
| `PipelineServices` | `SharedRenderService*` 的 facade | `CreateRenderTarget`, `LoadMesh`, `LoadMaterial` |

---

## 6. 待讨论：MeshRenderService 的去留

### 6.1 原来的设想

把 `DeferredRenderer` 里 `initViews()` + `setupMeshPassContexts()` + `buildMeshDrawCommands()` 抽成一个独立 service，供所有 Pipeline 共享：

```
MeshRenderService::prepareView()
  → MeshViewExtension
       visible_primitives / instance_scene_data
       pass relevance / cached and dynamic draw commands
```

### 6.2 现在的情况

C# SRP 设计确定后，C# 永远不需要知道 prepare 的存在。C# Pass 只调 `DrawRenderers(settings)`，内部自动取 `MeshViewExtension` 上已准备的 draw data。

所以 MeshRenderService 变成一个**纯 C++ 内部重构的问题**——它不影响 C# API，也不影响 C# 和 C++ 之间的协议。

### 6.3 三种方案

**方案 A：保留 MeshRenderService，但只作为 C++ 内部复用单元**

```
RendererBase::render() 内部调用：
  1. MeshRenderService::prepare(scene, views, requiredPhases)
  2. buildFrameDrawCommandList()  // graph build + execute
```

适合未来有多套 C++ Pipeline（Forward / Forward+ / Mobile）需要复用同一套 prepare 逻辑。

**方案 B：prepare 逻辑收敛到 RenderView**

```
RenderView::prepareMeshPasses(phaseMask, scene, cmdList)
  → 填充自身 MeshViewExtension
```

view 持有自己的 draw data，不依赖外部 service。

**方案 C：不抽 service，prepare 留在 RendererBase**

保持现状。仅当确实出现第二个需要 mesh prepare 的管线时再抽取。

### 6.4 建议

当前阶段走 **方案 C**——把 prepare 逻辑整理为 `RendererBase` 的 protected 方法（`prepareViews()`、`setupMeshPassData()`、`buildMeshDrawCommands()`），`DeferredRenderer::render()` 调标准流程。等未来出现第二个管线再考虑是否抽取。

**需要明确结论。**

---

## 7. 实施顺序

| 阶段 | 内容 |
|---|---|
| 1 | C++ MeshPhase 注册表（字符串 → MeshPassType 映射） |
| 2 | C++ `DrawRenderers(RenderView&, MeshDrawSettings)` 实现 |
| 3 | C++ 托管包装：`ManagedRenderView`、`MeshHandle`、`MaterialHandle`、`RTHandle` |
| 4 | C# 绑定：`RenderPipeline`、`RenderFeature`、`RenderPass` 抽象类 |
| 5 | C# 绑定：`RenderGraph`（`AddRasterPass` / `AddComputePass`）+ `RasterCommandContext` / `ComputeCommandContext` |
| 6 | C# 绑定：`PipelineServices`（RT 管理、资产加载） |
| 7 | 迁移所有 C++ Pass 为 IRenderPass 类，接入 `DrawRenderers` |
| 8 | DeferredRenderer prepare 逻辑整理到 RendererBase（方案 A / B / C 取决于结论） |
| 9 | C# SRP 完整示例可用（Pipeline + 内建 Feature + 自定义 Feature） |
