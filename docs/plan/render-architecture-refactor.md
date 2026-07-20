# Dodoe 渲染架构重构方案

## 目标

1. **统一 SRP 架构**：所有渲染管线都实现同一套 `IRenderPipeline` 协议
2. **C++ / C# 同级实现**：Pipeline / Feature / Pass 既可以由 C++ 编写，也可以由 C# 编写
3. **资源分层清晰**：按资源类型和生命周期拆分，不使用单体 `RenderResourceSystem`
4. **Pass 类化**：Pass 具备独立类型，可复用、可扩展、可脚本绑定
5. **运行时可解析**：如果用户注册了自己的 SRP，则优先使用用户实现；否则回退到内建 C++ 实现

---

## 1. 现状问题

### 1.1 Pass 还是裸 lambda

- 当前 Pass 通过 `graph.addPass()` 直接注册 lambda
- Pass 没有独立类，无法被复用、继承、脚本化

### 1.2 Feature 资源管理不统一

| Feature | 是否有独立 Resource | 当前问题 |
|---------|:---:|---------|
| SpriteFeature | 是 | 资源分层方式和其他 Feature 不一致 |
| ImGuiFeature | 是 | 资源管理逻辑散在 Feature 内部 |
| LightingFeature | 是 | 只覆盖部分 buffer，边界不完整 |
| GizmoFeature | 是 | 只有局部资源，不具备统一模式 |
| BaseSceneFeature | 否 | GBuffer / Shadow / PrimitiveSceneBuffer 仍散在 Pass 内 |

### 1.3 资源生命周期不合理

- GBuffer Framebuffer 每帧创建
- Shadow Framebuffer 每帧创建
- `primitive_scene_buffer` 每帧 transient
- 材质、绑定、布局、PSO 归属关系不清晰

### 1.4 DeferredRenderer 职责过重

当前 `render()` 同时承担：

- View 初始化
- Mesh Draw 构建
- CPU / GPU Culling
- RenderGraph 编译与执行
- Feature 注册调度

---

## 2. 目标架构原则

### 2.1 唯一 Pipeline 协议

- 运行时只认 `IRenderPipeline`
- 不区分 native pipeline 和 managed pipeline 的调用路径
- C++ 和 C# 都只是 `IRenderPipeline` 的不同实现来源

### 2.2 Feature / Pass 分层

- `IRenderPipeline` 负责组织一组 Feature
- `IRenderFeature` 负责向 RenderGraph 注入一组 Pass
- `IRenderPass` 负责注册一个完整的 graph pass

### 2.3 资源按类型和生命周期拆分

不再设计一个包罗万象的总资源类，而是拆成：

- `RTHandleSystem`
- `RenderGraphTransientPool`
- `ShaderLibrary`
- `MaterialSystem`
- `PipelineStateCache`
- `BindingLayoutCache`
- `BindingSetCache`
- `InputLayoutCache`
- `FeatureResource`

### 2.4 解析优先级

- 先查用户注册的 C# Pipeline
- 如果找到，则运行用户实现
- 如果没找到，则回退到 C++ 内建实现

---

## 3. Unity SRP 对照

| Unity SRP | Dodoe 对应 |
|-----------|-----------|
| `RenderPipelineAsset` | `RenderPipelineDefinition` |
| `RenderPipeline` | `IRenderPipeline` |
| `ScriptableRendererFeature` | `IRenderFeature` |
| `ScriptableRenderPass` | `IRenderPass` |
| `RTHandleSystem` | `RTHandleSystem` |
| `RenderGraph` | `RenderGraph` / `RenderGraphBuilder` |
| 用户自定义 SRP | `ManagedPipelineRegistry` + `ManagedPipelineAdapter` |

说明：

- 这里只参考 Unity 的**职责划分和生命周期划分**
- 不照搬 Unity 的具体底层实现
- Dodoe 仍保留自己的 RHI、RenderGraph、PSO、Binding 体系

---

## 4. 四层架构

```text
Layer 4: Pipeline 层
   ├── RenderPipelineDefinition
   ├── IRenderPipeline
   ├── PipelineResolver
   ├── NativePipelineRegistry
   └── ManagedPipelineRegistry

Layer 3: Feature / Pass 层
   ├── IRenderFeature
   ├── IRenderPass
   ├── BaseSceneFeature
   ├── LightingFeature
   ├── PostProcessFeature
   ├── SpriteFeature
   ├── ImGuiFeature
   └── PresentFeature

Layer 2: 资源层
   ├── RTHandleSystem
   ├── RenderGraphTransientPool
   ├── ShaderLibrary / ShaderVariantCache
   ├── MaterialSystem
   ├── PipelineStateCache
   ├── BindingLayoutCache
   ├── BindingSetCache
   ├── InputLayoutCache / VertexFactoryRegistry
   └── FeatureResource

Layer 1: RHI 层
   └── GfxContext / GfxTexture / GfxBuffer / GfxFramebuffer / GfxPipeline / ...
```

---

## 5. 核心对象设计

### 5.1 RenderPipelineDefinition

`RenderPipelineDefinition` 只描述“我要使用什么 Pipeline”，不持有具体实现。

```cpp
namespace dodoe {

struct RenderPipelineDefinition {
    String pipeline_type;               // "Deferred" / "Forward" / "Mobile" / "MyCustomPipeline"
    Bool allow_managed_override{true};  // 同名时是否优先采用 Managed 实现
    DynamicArray<String> feature_names; // 可选：额外注入的 Feature
    CullingPath culling_path{CullingPath::CpuOnly};
};

} // dodoe
```

### 5.2 IRenderPipeline

`IRenderPipeline` 是运行时唯一认识的管线协议。

```cpp
namespace dodoe {

class IRenderPipeline {
public:
    virtual ~IRenderPipeline() = default;

    virtual Bool initialize(const RenderPipelineDefinition& definition,
                            GfxContext& gfx,
                            SharedRenderService& shared) = 0;

    virtual void onResize(UInt32 width, UInt32 height) = 0;

    virtual void render(RenderViewFamily& views,
                        RenderScene& scene,
                        UInt32 swapchain_index,
                        DrawCommandList& out_commands) = 0;

    virtual void shutdown() = 0;
};

} // dodoe
```

### 5.3 IRenderFeature

Feature 负责组织一组 Pass 和自身持久化资源。

```cpp
namespace dodoe {

struct RenderFeatureContext {
    const RenderView* view{nullptr};
    const RenderPassContext* pass_context{nullptr};
};

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;

    virtual void initialize(GfxContext& gfx, SharedRenderService& shared) {}
    virtual void onResize(UInt32 width, UInt32 height, GfxContext& gfx) {}

    virtual void registerPasses(RenderGraphBuilder& graph,
                                const RenderFeatureContext& context) = 0;

    virtual void shutdown() {}
};

} // dodoe
```

### 5.4 IRenderPass

Pass 是高层对象，职责是向 RenderGraph 注册一个完整的底层 graph pass。

```cpp
namespace dodoe {

class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    virtual void build(RenderGraphBuilder& graph,
                       const RenderPassContext& context,
                       const RenderView& view) = 0;
};

} // dodoe
```

约定：

- `IRenderPass` 不直接替代 `RenderGraphPass`
- `IRenderPass::build()` 内部仍然调用 `graph.addPass(...)`
- `IRenderPass` 是**架构层抽象**
- `RenderGraphPass` 是**图执行层抽象**

### 5.5 PipelineResolver

`PipelineResolver` 负责把 `RenderPipelineDefinition` 解析成最终运行的 `IRenderPipeline` 实例。

```cpp
namespace dodoe {

class NativePipelineRegistry {
public:
    void registerFactory(const String& name, PipelineFactoryFn fn);
    Scope<IRenderPipeline> create(const String& name) const;
};

class ManagedPipelineRegistry {
public:
    void registerFactory(const String& name, ManagedPipelineFactory fn);
    Scope<IRenderPipeline> create(const String& name) const;
};

class PipelineResolver {
public:
    Scope<IRenderPipeline> resolve(const RenderPipelineDefinition& definition,
                                   NativePipelineRegistry& native_registry,
                                   ManagedPipelineRegistry& managed_registry) const;
};

} // dodoe
```

解析规则：

1. `allow_managed_override == true` 时先查 `ManagedPipelineRegistry`
2. 找到则直接使用用户 Pipeline
3. 否则查 `NativePipelineRegistry`
4. 两边都找不到则报错

---

## 6. 资源层设计

### 6.1 总原则

- **不使用单体 `RenderResourceSystem`**
- 每类资源只放在属于自己的系统里
- Feature 私有资源和全局资源分开
- persistent 和 transient 分开
- size-dependent 和 scene-dependent 分开

### 6.2 资源子系统

#### RTHandleSystem

负责尺寸相关、跨帧持久化的渲染目标。

```cpp
class RTHandle {
public:
    void initialize(const RTHandleDesc& desc, GfxContext& gfx);
    void resolve(UInt32 reference_width, UInt32 reference_height, GfxContext& gfx);
    void reset();

    GfxTextureHandle getColorTexture(UInt32 index) const;
    GfxTextureHandle getDepthTexture() const;
    GfxFramebufferHandle getFramebuffer() const;
};
```

职责：

- GBuffer RT
- ShadowMap RT
- SceneColor / SceneDepth
- imported backbuffer 包装

#### RenderGraphTransientPool

负责所有 RenderGraph 临时资源。

职责：

- transient texture
- transient buffer
- 生命周期仅限单帧 graph 执行

#### ShaderLibrary / ShaderVariantCache

职责：

- shader module 加载
- shader variant 管理
- keyword / permutation
- 热重载入口

#### MaterialSystem

职责：

- `Material`
- `MaterialInstance`
- shader 绑定
- material parameter
- render state preset

#### PipelineStateCache

职责：

- graphics pipeline cache
- compute pipeline cache
- 由 shader + render state + framebuffer info 组合求 key

#### BindingLayoutCache

职责：

- `GfxBindingLayoutHandle`
- 由 shader 反射或固定声明生成

#### BindingSetCache

职责：

- `GfxBindingSetHandle`
- 由 layout + 资源组合求 key
- 按材质、Pass 或 draw data 复用

#### InputLayoutCache / VertexFactoryRegistry

职责：

- `GfxInputLayoutHandle`
- 顶点流布局
- VertexFactory 与 MeshLayout 对应

#### FeatureResource

负责某个 Feature 自己持有的跨帧资源。

例如：

- `BaseSceneResource`
- `DeferredLightResource`
- `SpriteRenderResource`
- `ImGuiRenderResource`

### 6.3 资源归属表

| 资源 | 归属 | 生命周期 |
|------|------|---------|
| `ShaderModule` / `ShaderVariant` | `ShaderLibrary` / `ShaderVariantCache` | 程序级 |
| `Material` / `MaterialInstance` | `MaterialSystem` | 资产级 / 场景级 |
| `GfxGraphicsPipelineHandle` | `PipelineStateCache` | 程序级 / 材质级 |
| `GfxBindingLayoutHandle` | `BindingLayoutCache` | 程序级 |
| `GfxBindingSetHandle` | `BindingSetCache` | 材质级 / draw 级 |
| `GfxInputLayoutHandle` | `InputLayoutCache` | 程序级 |
| 持久化 RT / FB | `RTHandleSystem` / `FeatureResource` | resize 级 / Feature 级 |
| transient texture / buffer | `RenderGraphTransientPool` | 单帧 |
| Feature 私有 buffer | `FeatureResource` | Feature 级 |

---

## 7. Feature 与 Pass 设计

### 7.1 BaseSceneFeature

`BaseSceneFeature` 管：

- `GBufferPass`
- `ShadowPass`
- `SkyboxPass`

并持有：

- `BaseSceneResource`
  - GBuffer RTHandle
  - ShadowMap RTHandle
  - PrimitiveSceneBuffer

```cpp
class BaseSceneResource {
    RTHandle m_gbuffer;
    RTHandle m_shadow_map;
    GfxBufferHandle m_primitive_scene_buffer;
    UInt32 m_primitive_scene_capacity{0};

public:
    void initialize(GfxContext& gfx);
    void onResize(UInt32 width, UInt32 height, GfxContext& gfx);
    void ensurePrimitiveSceneBufferCapacity(UInt32 instance_count, GfxContext& gfx);
    void reset();

    RTHandle& getGBuffer() { return m_gbuffer; }
    RTHandle& getShadowMap() { return m_shadow_map; }
    GfxBufferHandle getPrimitiveSceneBuffer() const { return m_primitive_scene_buffer; }
};
```

### 7.2 PrimitiveSceneBuffer 扩容策略

`primitive_scene_buffer` 不再每帧 transient，而是 Feature 持久化持有。

```cpp
void BaseSceneResource::ensurePrimitiveSceneBufferCapacity(UInt32 instance_count, GfxContext& gfx) {
    const UInt32 required = std::max(instance_count, 1u);
    if (required <= m_primitive_scene_capacity && m_primitive_scene_buffer) {
        return;
    }

    const UInt32 new_capacity = std::max(required, std::max(m_primitive_scene_capacity * 2, 64u));
    m_primitive_scene_capacity = new_capacity;

    m_primitive_scene_buffer = gfx.createBuffer(
        GfxBufferDesc()
            .setByteSize(new_capacity * sizeof(InstanceSceneData))
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
            .setDebugName("BaseScene PrimitiveSceneBuffer"));
}
```

约定：

- `onResize()` 只处理尺寸相关资源
- `ensurePrimitiveSceneBufferCapacity()` 只处理实例数量相关资源
- Pass 在 `build()` 前确保容量

### 7.3 GBufferPass 示例

```cpp
class GBufferPass : public IRenderPass {
    BaseSceneResource& m_resource;
    const GBufferMeshProcessor& m_mesh_processor;

public:
    GBufferPass(BaseSceneResource& resource, const GBufferMeshProcessor& processor)
        : m_resource(resource), m_mesh_processor(processor) {}

    void build(RenderGraphBuilder& graph,
               const RenderPassContext& context,
               const RenderView& view) override {
        const auto* mesh_ext = view.getExtension<MeshViewExtension>();
        const UInt32 visible_instance_count = mesh_ext
            ? static_cast<UInt32>(mesh_ext->instance_scene_data.size())
            : 0;

        m_resource.ensurePrimitiveSceneBufferCapacity(visible_instance_count, *context.gfx_context);

        graph.addPass<GBufferPassParameters>(
            "GBufferPass",
            RenderGraphPassFlags::Raster,
            [this](RenderGraphPassBuilder& b, GBufferPassParameters& p) {
                p.albedo = b.write(b.importTexture(m_resource.getGBuffer().getColorTexture(0), "GBufferA"));
                p.normal = b.write(b.importTexture(m_resource.getGBuffer().getColorTexture(1), "GBufferB"));
                p.depth  = b.write(b.importTexture(m_resource.getGBuffer().getDepthTexture(), "GBufferDepth"));
                p.scene_buffer = b.write(b.importBuffer(m_resource.getPrimitiveSceneBuffer(), "PrimitiveSceneBuffer"));
            },
            [this](const GBufferPassParameters& p, const RenderGraphPassContext& ctx, DrawCommandList& cmd) {
                auto framebuffer = m_resource.getGBuffer().getFramebuffer();
                // execute body...
            });
    }
};
```

---

## 8. Pipeline 组织方式

### 8.1 C++ 内建 Pipeline

```cpp
class DeferredPipeline : public IRenderPipeline {
    MeshRenderer m_mesh_renderer;
    DynamicArray<Scope<IRenderFeature>> m_features;
    RenderPassContext m_pass_context;

public:
    Bool initialize(const RenderPipelineDefinition& definition,
                    GfxContext& gfx,
                    SharedRenderService& shared) override;

    void onResize(UInt32 width, UInt32 height) override;

    void render(RenderViewFamily& views,
                RenderScene& scene,
                UInt32 swapchain_index,
                DrawCommandList& out_commands) override;

    void shutdown() override;
};
```

### 8.2 C# 用户 Pipeline

```csharp
public class MyDeferredPipeline : IRenderPipeline
{
    private SSAOFeature _ssao;

    public override bool Initialize(RenderPipelineDefinition definition,
                                    GfxContextHandle gfx,
                                    SharedRenderServiceHandle shared)
    {
        _ssao = new SSAOFeature();
        _ssao.Initialize(gfx, shared);
        return true;
    }

    public override void OnResize(uint width, uint height) {}
    public override void Render(RenderViewFamily views, RenderScene scene, uint swapchainIndex, DrawCommandList outCommands) {}
    public override void Shutdown() {}
}
```

### 8.3 注册与解析

```cpp
native_registry.registerFactory("Deferred", []() {
    return create_scope<DeferredPipeline>();
});
```

```csharp
ManagedPipelineRegistry.Register("Deferred", () => new MyDeferredPipeline());
```

```cpp
RenderPipelineDefinition definition;
definition.pipeline_type = "Deferred";

auto pipeline = resolver.resolve(definition, native_registry, managed_registry);
```

结果：

- 用户注册了 `Deferred` 的 managed 版本时，运行用户实现
- 用户没注册时，运行内建 C++ `DeferredPipeline`

### 8.4 Managed Adapter

```cpp
class ManagedPipelineAdapter : public IRenderPipeline {
public:
    Bool initialize(const RenderPipelineDefinition& definition,
                    GfxContext& gfx,
                    SharedRenderService& shared) override;
    void onResize(UInt32 width, UInt32 height) override;
    void render(RenderViewFamily& views,
                RenderScene& scene,
                UInt32 swapchain_index,
                DrawCommandList& out_commands) override;
    void shutdown() override;
};

class ManagedFeatureAdapter : public IRenderFeature {
public:
    void initialize(GfxContext& gfx, SharedRenderService& shared) override;
    void onResize(UInt32 width, UInt32 height, GfxContext& gfx) override;
    void registerPasses(RenderGraphBuilder& graph, const RenderFeatureContext& context) override;
    void shutdown() override;
};

class ManagedPassAdapter : public IRenderPass {
public:
    void build(RenderGraphBuilder& graph,
               const RenderPassContext& context,
               const RenderView& view) override;
};
```

---

## 9. 实施步骤

| 阶段 | 内容 |
|:---:|------|
| 1 | 抽出 `IRenderPipeline` / `IRenderFeature` / `IRenderPass` 三个统一协议 |
| 2 | 新建 `RenderPipelineDefinition`、`NativePipelineRegistry`、`ManagedPipelineRegistry`、`PipelineResolver` |
| 3 | 把 `DeferredRenderer`、`Only2DRenderer` 收敛为 native `IRenderPipeline` 实现 |
| 4 | 把 Pass 从裸 lambda 改为独立 `IRenderPass` 类 |
| 5 | 为 `BaseSceneFeature` 建立独立 `BaseSceneResource` |
| 6 | 建立 `RTHandleSystem`，替换每帧创建 framebuffer / RT 的逻辑 |
| 7 | 建立 `ShaderLibrary`、`MaterialSystem`、`PipelineStateCache`、`BindingLayoutCache`、`BindingSetCache`、`InputLayoutCache` |
| 8 | 建立 `RenderGraphTransientPool`，统一 transient resource |
| 9 | 打通 `ManagedPipelineAdapter`、`ManagedFeatureAdapter`、`ManagedPassAdapter` |

---

## 10. 最终使用方式

### 10.1 C++ 路径

```cpp
RenderPipelineDefinition definition;
definition.pipeline_type = "Deferred";

auto pipeline = resolver.resolve(definition, native_registry, managed_registry);
pipeline->initialize(definition, *gfx_context, *shared_render_service);
pipeline->render(view_family, scene, swapchain_index, out_commands);
```

### 10.2 C# 路径

```csharp
ManagedPipelineRegistry.Register("Deferred", () => new MyDeferredPipeline());
```

运行时效果：

- 有用户 Pipeline：使用用户 Pipeline
- 没有用户 Pipeline：使用内建 Pipeline
- Pipeline 内部再组织自己的 Feature 和 Pass
