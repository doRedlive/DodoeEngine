---
name: dodoe-render-pipeline-architecture
description: Dodoe Engine render pipeline architecture reference — RenderPipeline, RenderGraph, passes, features, blackboard data flow, view extensions, draw commands, and initialization flow. Use when modifying or extending the render pipeline.
metadata:
  type: reference
---

# Dodoe Render Pipeline Architecture

> Auto-generated from an exhaustive codebase exploration on 2026-07-04.
> Covers the render pipeline system under `engine/src/runtime/function/render/`.

---

## 1. Directory Structure

```
engine/src/runtime/function/render/
├── framework/                    # Core render framework
│   ├── local_vertex_factory.h/cpp   → input layouts (GBuffer, Shadow, Sprite, ImGui)
│   ├── pipeline_state_cache.h/cpp   → PSO resolve/create cache
│   ├── shared_render_service.h/cpp  → aggregates ShaderLibrary + PSO cache + TextureManager
│   ├── vertex_factory.h             → base vertex factory
│   └── ...
├── graphics/
│   ├── draw_command_list.h/cpp      → deferred command recording (linked-list)
│   └── gfx_context.h                → RHI backend wrapper (Vulkan/DX12/OpenGL)
├── mesh_draw/                    # Mesh drawing pipeline
│   ├── mesh_pass_type.h             → enum { GBuffer=0, DirectionalShadow=1 }
│   ├── mesh_processor_base.h        → IMeshPassProcessor interface
│   ├── gbuffer_mesh_processor.h/cpp
│   ├── directional_shadow_mesh_processor.h/cpp
│   ├── mesh_draw_types.h            → InstanceSceneData, MeshPassRelevance, etc.
│   ├── mesh_draw_command.h          → MeshDrawCommand struct
│   └── mesh_draw_command_dispatch.h
├── render_graph/                 # Render graph (dependency-based pass scheduling)
│   ├── render_graph.h               → compiled pass DAG
│   ├── render_graph_builder.h       → build API, blackboard, compile/execute
│   └── render_graph_resource.h      → transient resource handles
├── render_pipeline/              # Pipeline orchestration
│   ├── render_pipeline.h/cpp        → THE pipeline class
│   ├── render_pass_context.h        → bundled context for all passes
│   ├── render_pipeline_pass_utils.h
│   ├── passes/                      # All pass implementations
│   │   ├── render_pipeline_passes.h     → pass function declarations
│   │   ├── render_pass_blackboard_keys.h → inter-pass data keys
│   │   ├── render_base_pass.cpp         → GBufferPass
│   │   ├── render_shadow_pass.cpp       → DirectionalShadowPass
│   │   ├── render_skybox_pass.cpp       → SkyboxPass
│   │   ├── render_deferred_light_pass.cpp → DeferredLightPass
│   │   ├── render_post_process_pass.cpp → PostProcessPass (tone mapping)
│   │   ├── render_sprite_pass.cpp       → SpritePass
│   │   ├── render_imgui_pass.cpp        → ImGuiPass (DEBUG only)
│   │   └── render_present_pass.cpp      → PresentPass
│   └── render_feature/             # Feature plugins (register passes)
│       ├── render_feature.h            → IRenderFeature base + RenderFeatureContext
│       ├── render_builtin_features.h/cpp → BaseScene, Lighting, PostProcess, Present
│       ├── sprite_feature.h/cpp
│       ├── sprite_render_resource.h/cpp
│       ├── imgui_feature.h/cpp
│       └── imgui_render_resource.h/cpp
├── render_scene/                 # Scene data
│   ├── render_scene.h/cpp
│   ├── render_object.h               → RenderObjectType { StaticMesh, Sprite, Foliage }
│   ├── primitive_render_object.h
│   ├── sprite_render_object.h        → SpriteRenderObject
│   ├── sprite_scene_info.h           → SpriteInstance (64B), QuadVertex (28B), kQuadVertices/Indices
│   └── light_scene_info.h
├── render_view/                  # View definitions
│   ├── render_view.h/cpp             → RenderView with ViewExtensionContainer
│   ├── render_view_family.h/cpp      → array of RenderView + frame time
│   ├── view_extension.h              → IViewExtension base
│   ├── mesh_view_extension.h/cpp     → per-view mesh data
│   ├── sprite_view_extension.h/cpp   → per-view visible sprites
│   └── render_viewport.h/cpp         → window wrapper, letterbox calc
├── render_settings.h/cpp         # Pipeline type enum + config
├── render_system.h/cpp           # Top-level render system
├── render_command.h              # Game→Render thread commands
└── renderer.h                    # Static API: Renderer::AddSprite(), etc.
```

---

## 2. Pipeline Type Configuration

**File:** `engine/src/runtime/function/render/render_settings.h`

```cpp
enum class RenderingPipelineType {
    None = 0,
    Forward,
    ForwardPlus,
    Deferred,      // ← only one implemented
    DeferredPlus,
    Only2D,        // ← defined but NOT implemented (hard assert blocks it)
};
```

Configured via `ApplicationSpecification::render_settings.pipeline` at engine startup.
Default: `Deferred`. There is **no runtime switching mechanism** — pipeline type is read once at init.

**Critical line** in `render_pipeline.cpp:218`:
```cpp
DO_ASSERT(RenderSettings::GetRenderingPipelineType() == RenderingPipelineType::Deferred,
          "RenderPipeline currently only supports Deferred pipeline type");
```

---

## 3. Initialization Flow

### Engine Startup → Render Bootstrap

```
Application(spec)
  → SystemContext::preInit()           // logging, events, type meta
  → SystemContext::initializeModules():
      1. TimeSystem::Create
      2. WindowManager::Create         // GLFW window
      3. RenderSettings::Initialize    // stores api, pipeline, threading_mode
      4. UISystem::Create
      5. RenderSystem::Create          // boots entire render stack
      6. ... physics, scripts, world
```

### RenderSystem::initialize() (render_system.cpp:16-45)

```
1. RenderViewport::Create              → wraps window
2. GfxContext::Create                  → RHI device (Vulkan/DX12/OpenGL)
3. GDrawCommandList.setDevice          → global cmd list gets device
4. DescriptorTableManager::Create      → bindless tables
5. TextureManager::Create              → texture lifecycle
6. SharedRenderService::Create         → ShaderLibrary + PipelineStateCache
7. RenderScene::Create                 → scene container
8. RenderPipeline::Create({            → pipeline boot
       worker_count,
       gfx_context,
       shared_render_service
   })
```

### RenderPipeline::initialize() (render_pipeline.cpp:217-252)

```
1. ASSERT pipeline type == Deferred (hard assertion!)
2. Create ThreadPool
3. Stash gfx_context, shared_render_service
4. Create LocalVertexFactory
   → pre-creates GBuffer and Shadow input layouts
5. Create MeshProcessors:
   - GBufferMeshProcessor        (index MeshPassType::GBuffer)
   - DirectionalShadowMeshProcessor (index MeshPassType::DirectionalShadow)
6. Register Features (ordered list):
   [0] BaseSceneFeature     → GBufferPass + ShadowPass + SkyboxPass
   [1] LightingFeature      → DeferredLightPass
   [2] PostProcessFeature   → PostProcessPass (tone mapping)
   [3] SpriteFeature        → SpritePass
   [4] ImGuiFeature         → ImGuiPass (debug only)
   [5] PresentFeature       → PresentPass
```

---

## 4. Per-Frame Render Flow

```
RenderPipeline::render(view_family, scene, swapchain_index, out_commands)
  │
  ├─ 1. initViews(scene, view_family)
  │     → resetExtensions() on each view
  │     → view_family.buildVisiblePrimitives(scene)
  │         → frustum cull meshes → MeshViewExtension.visible_primitives
  │         → frustum cull sprites → SpriteViewExtension.visible_sprites
  │
  ├─ 2. setupMeshPassContexts(scene, view_family)
  │     → find first enabled directional light
  │     → compute directional_light_view_projection
  │     → for each view:
  │         → set frame_time_data
  │         → build instance_scene_data array from visible_primitives
  │         → set directional_shadow_view_projection
  │         → setupMeshPassRelevance() → per-primitive pass bitfield
  │
  ├─ 3. buildMeshDrawCommands(view_family, cmd_list)
  │     → resolve GBuffer PSO from cache
  │     → resolve Shadow PSO from cache
  │     → for each view:
  │         → GBufferMeshProcessor.buildCommands() → GBuffer draw command list
  │         → DirectionalShadowMeshProcessor.buildCommands() → Shadow draw command list
  │         → assign resolved PSOs to all commands
  │
  └─ 4. buildFrameDrawCommandList(view_family, scene, swapchain_index, out_commands)
        → buildPassContext(scene)  → RenderPassContext
        → for each view:
            → RenderGraphBuilder graph
            → for each feature: feature->registerPass(graph, context)
            → graph.compile()  → DAG + resource scheduling
        → for each view: graph.execute(pool, context, out_commands)
```

### Frame Tick Paths (ThreadingMode)

- **TripleThread**: Game→Render thread (`renderFrame()`)→Draw thread (GPU execution)
- **DualThread**: Game+Render on one thread, Draw on separate thread
- **SingleThread**: Everything on one thread via `executeFrameOnce()`

---

## 5. Feature System (IRenderFeature)

**File:** `engine/src/runtime/function/render/render_pipeline/render_feature/render_feature.h`

```cpp
struct RenderFeatureContext {
    const RenderView* view{nullptr};
    const RenderPassContext* pass_context{nullptr};
};

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;
    virtual void registerPass(RenderGraphBuilder& graph,
                              const RenderFeatureContext& context) const = 0;
};
```

Each feature has ONE method: `registerPass()` — it calls pass functions into the `RenderGraphBuilder`.
Some features own mutable `RenderResource` objects with lazy `getOrCreate*` semantics and `reset()` for invalidation.

### Feature → Pass Mapping

| Feature | File | Passes Registered |
|---|---|---|
| `BaseSceneFeature` | `render_builtin_features.cpp:11-17` | `RenderGBufferPass`, `RenderDirectionalShadowPass`, `RenderSkyboxPass` |
| `LightingFeature` | `render_builtin_features.cpp:19-22` | `RenderDeferredLightPass` |
| `PostProcessFeature` | `render_builtin_features.cpp:24-27` | `RenderPostProcessPass` (tone mapping) |
| `SpriteFeature` | `sprite_feature.cpp:10-13` | `RenderSpritePass` |
| `ImGuiFeature` | `imgui_feature.cpp:10-13` | `RenderImGuiPass` (`#ifdef DODOE_DEBUG`) |
| `PresentFeature` | `render_builtin_features.cpp:29-32` | `RenderPresentPass` |

---

## 6. RenderGraph Blackboard (Inter-Pass Data Flow)

**File:** `engine/src/runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h`

| Key | Value Type | Producer | Consumer(s) |
|---|---|---|---|
| `SceneTexturesKey` | `SceneTextures` (albedo, normal, position, material, depth, instance_scene_data) | GBufferPass | ShadowPass, SkyboxPass, DeferredLightPass |
| `ShadowMapKey` | `RenderGraphTextureHandle` | DirectionalShadowPass | DeferredLightPass |
| `SceneHdrKey` | `RenderGraphTextureHandle` | SkyboxPass | DeferredLightPass, PostProcessPass |
| `FxaaColorKey` | `RenderGraphTextureHandle` | PostProcessPass | SpritePass, PresentPass |
| `ImGuiColorKey` | `RenderGraphTextureHandle` | ImGuiPass | PresentPass |

### Full Deferred Pipeline Data Flow:

```
GBufferPass          DirectionalShadowPass     SkyboxPass        DeferredLightPass     PostProcessPass      SpritePass         ImGuiPass        PresentPass
─────────────────    ──────────────────────    ────────────      ─────────────────     ────────────────     ────────────       ────────────     ────────────
Writes:              Reads:  SceneTextures     Reads:  depth      Reads:  albedo/normal  Reads:  HDR          Reads:  FxaaColor  Writes: ImGuiColor Reads: FxaaColor,
  SceneTextures      Writes: ShadowMap         Writes: HDR color    position/material   Writes: FxaaColor    Writes: FxaaColor                      ImGuiColor
  (6 fields)                                       (SceneHdr)        shadow/depth/skybox                                             Writes: backbuffer
                                                                    Writes: HDR color
```

---

## 7. Pass Implementation Pattern

Every pass follows this exact pattern:

```cpp
// In render_pipeline_passes.h
namespace dodoe::RenderPipelinePass {
    void RenderXxxPass(RenderGraphBuilder& graph, /* ... args ... */);
}

// In render_xxx_pass.cpp
struct XxxPassParameters {
    RenderGraphTextureHandle input_texture;
    RenderGraphTextureHandle output_texture;
};

void RenderPipelinePass::RenderXxxPass(RenderGraphBuilder& graph, /* ... args ... */) {
    // SETUP PHASE: declare resources
    graph.addPass<XxxPassParameters>(
        "XxxPass",
        RenderGraphPassFlags::Graphics,
        [&](XxxPassParameters& params, RenderGraphPassSetup& setup) {
            // Declare transient textures/buffers
            // Read from / write to blackboard
        },
        [](const XxxPassParameters& params, const RenderGraphExecuteContext& context) {
            // EXECUTE PHASE: issue GPU commands
            // Resolve handles → real GPU objects
            // Create framebuffers
            // Draw via DrawCommandList
        }
    );
}
```

---

## 8. RenderPassContext

**File:** `engine/src/runtime/function/render/render_pipeline/render_pass_context.h`

Bundles everything a pass needs into a single `const` reference:

```cpp
struct RenderPassContext {
    GfxContext* gfx_context{nullptr};
    SharedRenderService* shared_render_service{nullptr};
    const RenderScene* scene{nullptr};
    LocalVertexFactory* local_vertex_factory{nullptr};
    const IMeshPassProcessor* mesh_processors[MeshPassType::Count]{};

    // Convenience accessors:
    auto* getShaderLibrary() const;
    auto* getPipelineStateCache() const;
    auto* getTextureManager() const;
    auto* getScene() const;
    template<MeshPassType> auto* getMeshProcessor() const;
    [[nodiscard]] Bool isValid() const;
};
```

Constructed once per frame by `RenderPipeline::buildPassContext()`.

---

## 9. RenderView & View Extensions

**Files:**
- `engine/src/runtime/function/render/render_view/render_view.h`
- `engine/src/runtime/function/render/render_view/render_view_family.h`
- `engine/src/runtime/function/render/render_view/view_extension.h`

### RenderViewFamily
- Holds `DynamicArray<RenderView>` + frame time/delta
- Currently single view per viewport

### RenderView
- ID, viewport rect, view/projection matrices
- `ViewExtensionContainer` → type-erased map of `IViewExtension` by `type_hash_code`
- `getOrCreateExtension<T>()` creates or retrieves per-view data

### MeshViewExtension (`mesh_view_extension.h`)
- `visible_primitives` → frustum-culled primitives
- `primitive_mesh_pass_relevance` → per-primitive MeshPassRelevance bitfield
- `primitive_first_instance_offsets` → index into instance array
- `instance_scene_data` → InstanceSceneData per instance (model matrix, color_tint, params)
- `mesh_pass_commands[2]` → per-pass DynamicArray<MeshDrawCommand>
- `mesh_pass_primitive_indices[2]` → per-pass primitive indices
- `gbuffer_shader_data` → GBufferMeshDrawShaderData
- `directional_shadow_view_projection` → light VP matrix
- `frame_time_data` → Vector4f (time, delta, 0, 0)

### SpriteViewExtension (`sprite_view_extension.h`)
- `visible_sprites` → frustum-culled `DynamicArray<const SpriteSceneInfo*>`

---

## 10. Mesh Draw System

### MeshPassType (`mesh_pass_type.h`)
```cpp
enum class MeshPassType : UInt8 {
    GBuffer = 0,
    DirectionalShadow,
    Count  // = 2
};
```

### MeshDrawCommand (`mesh_draw_command.h`)
```cpp
struct MeshDrawCommand {
    MeshPassType pass_type;
    GfxGraphicsPipelineHandle pipeline;
    StaticArray<GfxBindingSetHandle, kMaxBindingSets> binding_sets;
    DynamicArray<GfxVertexBinding> vertex_bindings;
    GfxIndexBinding index_binding;
    DrawArguments draw_args;       // vertex_count, instance_count, vertex_offset, first_index, first_instance
    UInt32 primitive_index;
    Size_t shader_data_index;
    UInt64 sort_key;
    GfxBufferHandle instance_scene_buffer;
    Size_t instance_scene_buffer_offset;
    UInt32 instance_scene_count;
};
```

### IMeshPassProcessor (`mesh_processor_base.h`)
```cpp
class IMeshPassProcessor {
public:
    virtual ~IMeshPassProcessor() = default;
    virtual void reset() = 0;
    virtual GfxBindingLayoutHandle getBindingLayout() const = 0;
    virtual GfxBufferHandle getConstantBuffer() const = 0;
};
```

### Visible Primitive Culling (`render_view.cpp:80-123`)
- Extracts 6 frustum planes from VP matrix
- Tests each primitive's AABB in world space against all planes
- Sprites use same frustum culling logic (treated as flat quads in XY plane)

---

## 11. DrawCommandList (Deferred Recording)

**File:** `engine/src/runtime/function/graphics/draw_command_list.h`

A deferred-command system: all draw calls, state changes, and resource creations are recorded as linked-list `Command` nodes. They are "executed" against a real `GfxCommandList` later.

- Global singleton: `GDrawCommandList` (defined in `draw_command_list.cpp:6`)
- Commands stored as linked list: `m_head → node → node → ... → m_tail`
- Execution traverses the list calling `m_execute()` on each node
- Supports `ImmediateFrameScope` for direct recording mode (SingleThread/DualThread)

### Key Methods:
- **State:** `setTextureState()`, `setBufferState()`, `commitBarriers()`, `setGraphicsState()`, `setComputeState()`
- **Draw:** `draw()`, `drawIndexed()`, `dispatch()`
- **Upload:** `writeBuffer()`, `writeTexture()`, `copyBuffer()`
- **Clear:** `clearTextureFloat()`, `clearTextureUInt()`, `clearDepthStencilTexture()`
- **Resource:** `createTexture()`, `createBuffer()`, `createFramebuffer()`, `createBindingSet()`, `createGraphicsPipeline()`, `createInputLayout()`, `createBindingLayout()`

---

## 12. 2D Rendering (Existing)

### Sprite System ✓

| Component | File |
|---|---|
| Render object | `render_scene/sprite_render_object.h` — texture, UV, color, sorting layer, flags |
| GPU instance | `render_scene/sprite_scene_info.h` — `SpriteInstance` (64B, 16-aligned) |
| Quad geometry | `render_scene/sprite_scene_info.h` — `kQuadVertices` (4×28B), `kQuadIndices` (6 indices) |
| View extension | `render_view/sprite_view_extension.h` — `visible_sprites` |
| Feature | `render_pipeline/render_feature/sprite_feature.h` |
| Render resource | `render_pipeline/render_feature/sprite_render_resource.h` |
| Pass | `render_pipeline/passes/render_sprite_pass.cpp` |
| Input layout | `framework/local_vertex_factory.cpp:50-64` — 4 attribs at 28B stride |
| Renderer API | `renderer.h` — `Renderer::AddSprite()`, `UpdateSpriteTransform()`, `RemoveSprite()` |

**Sprite render resource** (`sprite_render_resource.cpp`):
- Framebuffer: lazy-created, invalidated on format/size change
- Binding layout: sampler(b0) + constant buffer(t0)
- Pipeline: alpha blending, no depth test, optional descriptor table

**Sprite pass** (`render_sprite_pass.cpp`):
- Reads `FxaaColorKey` from blackboard (or creates fresh target)
- Builds orthographic VP matrix
- Uploads quad VB/IB
- Collects visible sprite instances
- Draws via `drawIndexed(6 indices × N instances)`

### ImGui System ✓ (Debug Only)

| Component | File |
|---|---|
| Feature | `render_pipeline/render_feature/imgui_feature.h` |
| Render resource | `render_pipeline/render_feature/imgui_render_resource.h` |
| Pass | `render_pipeline/passes/render_imgui_pass.cpp` (`#ifdef DODOE_DEBUG`) |
| Input layout | `framework/local_vertex_factory.cpp:67-85` — 3 attribs matching `ImDrawVert` |

### NOT yet implemented:
- **Tilemap** — zero references in entire render directory
- **2D Light** — zero references
- **Rect / Line / Shape** — no dedicated rendering

---

## 13. LocalVertexFactory

**File:** `engine/src/runtime/function/render/framework/local_vertex_factory.h`

Stores four pre-created input layout handles:

| Layout | Creation | Attributes |
|---|---|---|
| `m_gbuffer_input_layout` | Init (eager) | 10 attribs: position, normal, tangent, bitangent, uv0-1, color + 6 TEXCOORD instanced + instance_color_tint + instance_params |
| `m_shadow_input_layout` | Init (eager) | position + 4 TEXCOORD instanced |
| `m_sprite_input_layout` | Lazy | POSITION(RGB32_F), TEXCOORD(RG32_F), COLOR(RGBA8_UNORM), TEXINDEX(R32_UINT) |
| `m_imgui_input_layout` | Lazy | POSITION(RG32_F), TEXCOORD(RG32_F), COLOR(RGBA8_UNORM) |

---

## 14. ShaderLibrary (SharedRenderService)

**File:** `engine/src/runtime/function/render/framework/shared_render_service.h`

Shader library contains all compiled shaders:

| Shader | Usage |
|---|---|
| GBuffer VS/PS | Deferred GBuffer pass |
| Shadow VS/PS | Directional shadow pass |
| Fullscreen VS | Used by post-process passes |
| Skybox PS | Skybox pass (usually combined with fullscreen VS) |
| DeferredLight PS | Deferred lighting pass |
| ToneMapping PS | Post-process pass |
| Sprite VS/PS | Sprite rendering |
| ImGui VS/PS | ImGui debug overlay |
| Present PS | Final present to backbuffer |

---

## 15. RenderCommand Queue

**File:** `engine/src/runtime/function/render/render_command.h`

Thread-safe SPSC queue from Game thread → Render thread:

```cpp
RenderCommand {
    AddPrimitive, RemovePrimitive, UpdatePrimitiveTransform,
    AddLight, RemoveLight, UpdateLightTransform,
    AddSprite, RemoveSprite, UpdateSpriteTransform
}
```

Static API in `renderer.h`:
```cpp
Renderer::AddPrimitive(...)     → RenderSystem::enqueueRenderCommand(...)
Renderer::AddSprite(...)        → RenderSystem::enqueueRenderCommand(...)
Renderer::AddLight(...)         → RenderSystem::enqueueRenderCommand(...)
```

Drained at the top of `RenderSystem::renderFrame()`.

---

## 16. Render Resource Caching Pattern

All feature-owned resources use lazy create + invalidation:

```cpp
class XxxRenderResource {
public:
    void reset() { /* invalidate all handles */ }
    auto getOrCreateBindingLayout(GfxDevice device) → GfxBindingLayoutHandle;
    auto getOrCreatePipeline(GfxDevice device, GfxFramebufferInfo fb_info) → GfxGraphicsPipelineHandle;
    auto getOrCreateFramebuffer(GfxDevice device, GfxTextureHandle color, Vector2i size) → GfxFramebufferHandle;
private:
    GfxBindingLayoutHandle  m_binding_layout;   // invalid handle until created
    GfxGraphicsPipelineHandle m_pipeline;
    GfxFramebufferHandle    m_framebuffer;
    // cached format/size for invalidation
};
```

Invalidation triggers:
- Framebuffer: format or size changed
- Pipeline: framebuffer info changed
- Binding layout: never (static per feature)

---

## 17. Key Architecture Decisions

1. **Single concrete pipeline**: Despite the `RenderingPipelineType` enum having 6 values (including `Only2D`), exactly ONE `RenderPipeline` class always runs Deferred. No strategy/factory pattern.

2. **Feature-based pass registration**: `IRenderFeature` objects are the extension point — they register passes into `RenderGraphBuilder`. This decouples "what to render" from "how to render."

3. **Render Graph (RDG)**: All passes use the RenderGraph pattern — resources declared symbolically, graph resolves dependencies, culls unreferenced passes, manages transient resource lifetimes.

4. **Blackboard for cross-pass data**: Passes share data via typed keys, not explicit parameter passing between features.

5. **View Extensions for per-view state**: Each `RenderView` holds arbitrary data via `getOrCreateExtension<T>()`, avoiding monolithic view classes.

6. **Mesh processors decoupled from passes**: `IMeshPassProcessor` builds draw commands outside the render graph, passes dispatch pre-built commands.

7. **Lazy GPU resource creation**: Feature-owned resources use `getOrCreate` with invalidation on format/size changes.

8. **No JSON config file**: All rendering is programmatically configured via `ApplicationSpecification` → `RenderSettingsInitInfo`. No runtime config loading.
