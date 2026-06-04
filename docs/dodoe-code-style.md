---
name: dodoe-code-style
description: Dodoe Engine C++ code style, naming conventions, and type system. Use when writing or modifying any code in the Dodoe engine to ensure consistency.
metadata:
  type: project
---

# Dodoe Engine Code Style

## File Structure

```cpp
// do@Redlive

#pragma once

#include "dopch.h"

// ... other includes ...

namespace dodoe {
    // 4-space indent inside namespace
} // dodoe
```

- Always `#pragma once` — no traditional `#ifndef` header guards.
- First line of every `.h` and `.cpp` is `// do@Redlive`.
- CMake uses `GLOB_RECURSE` — new `.cpp` files are auto-included.

## Type System

Use project aliases, NOT standard library types directly:

| Project Type | C++ Equivalent |
|---|---|
| `UInt32` / `ui32` | `uint32_t` |
| `Int32` | `int32_t` |
| `Bool` | `bool` |
| `Size_t` | `size_t` |
| `String` | `std::string` |
| `DynamicArray<T>` | `std::vector<T>` |
| `UnorderedMap<K,V>` | `std::unordered_map<K,V>` |
| `Scope<T>` | `std::unique_ptr<T>` |
| `Ref<T>` | `std::shared_ptr<T>` |

Smart pointer creation:
```cpp
auto ptr = create_scope<MyClass>(args...);  // std::make_unique
auto ptr = create_ref<MyClass>(args...);    // std::make_shared
```

## Naming Conventions

**NO `F` prefix.** Do NOT use UE naming conventions (no `FMeshBatch`, `FDrawCommand`, etc.).

| Category | Convention | Example |
|---|---|---|
| Classes | PascalCase | `MeshPassProcessor`, `MainCameraPass` |
| Static class functions | PascalCase | `Self()`, `Initialize()`, `Create()`, `Destroy()` |
| Instance methods | camelCase | `isValid()`, `getPipelineState()`, `buildDrawCommands()` |
| Member variables | `m_` prefix | `m_rhi`, `m_pipeline_state`, `m_cmd_list` |
| Static class members | `s_` prefix | `s_input_layout_cache` |
| Global variables | `s_` or `g_` prefix | `s_GlobalVal`, `g_RenderResource` |
| Constants | `k` prefix + PascalCase | `kMaxQuadCount`, `kMaxPointLightCount`, `kShadowMapSize` |
| Local constexpr | `k` prefix or UPPER_CASE | `kVolatileConstantBufferVersions` |
| Free functions | PascalCase | `ReadShaderFile()`, `BuildDirectionalLightDirection()` |
| Namespaces | snake_case (project: `dodoe`) | `namespace dodoe` |
| Interface namespaces | lowercase alias | `namespace rhi = nvrhi` |

**Static class functions** use PascalCase because they act like named constructors / factory methods:
```cpp
auto& app = Application::Self();
auto system = RenderSystem::Create(info);
RenderSystem::Destroy(system);
Bool success = Manager::Initialize(config);
```

**Global variables** use `s_` (static/file-scope) or `g_` (truly global) prefix:
```cpp
static DescriptorTableManager* s_DescriptorTable = nullptr;  // file-scope static
extern RenderResource* g_RenderResource;                       // truly global
```

## Class Layout

**Declaration order: private → protected → public.**

Variables declared first (private vars, then protected vars, then public vars).
Then methods in the same order: public methods, protected methods, private methods.

```cpp
class Test {
    // 1. Private variables first
    Int m_value{0};
    String m_name{};

protected:
    // 2. Protected variables
    Bool m_initialized{false};

public:
    // 3. Public variables (rare)
    inline static const String kClassName = "Test";

    // 4. Public methods
    Test() = default;
    ~Test() = default;
    void doSomething();
    Int getValue() const;

protected:
    // 5. Protected methods
    virtual void onInitialize();

private:
    // 6. Private methods
    void internalHelper();
};
```

**Specific class type patterns:**

For `RenderPass` subclasses, public interface comes first (override methods):
```cpp
class MyPass : public RenderPass {
    inline static const String kResourceName = "MyResource";

    MeshPassProcessor m_mesh_processor;
    rhi::CommandListHandle m_cmd_list{};
    rhi::TextureHandle m_target{};
    rhi::FramebufferHandle m_framebuffer{};

public:
    explicit MyPass(RhiContext* rhi) { m_rhi = rhi; }
    ~MyPass() override = default;
    void setup() override;
    void execute(size_t index) override;
    void cleanup() override;
    void onViewportResize(const Vector2i& viewport_extent) override;

private:
    void createFramebuffer();
};
```

For data/trait classes (`MeshPipelineState`), private members with public getters:
```cpp
class MeshPipelineState {
    friend class MeshPassProcessor;

    MeshPipelineStateDesc m_desc;
    rhi::ShaderHandle m_vertex_shader;
    rhi::InputLayoutHandle m_input_layout;
    rhi::GraphicsPipelineHandle m_pipeline;

public:
    [[nodiscard]] const auto& getPipeline() const { return m_pipeline; }
    [[nodiscard]] const auto& getDesc() const { return m_desc; }
};
```

## Key Patterns

**Const correctness:**
```cpp
[[nodiscard]] Bool isValid() const;
[[nodiscard]] const MeshPipelineState* getPipelineState() const;
[[nodiscard]] RenderGraph& graph();
[[nodiscard]] const RenderGraph& graph() const;
```

**`explicit` on single-arg constructors:**
```cpp
explicit MainCameraPass(RhiContext* rhi);
explicit SkyboxPass(RhiContext* rhi) { m_rhi = rhi; }
```

**`= default` for default special members:**
```cpp
~MeshPassProcessor() = default;
MeshPassProcessor(const MeshPassProcessor&) = delete;
```

**`inline static` for class-level constants:**
```cpp
inline static const String kSceneAlbedoName = "MainCameraAlbedo";
inline static constexpr Size_t kMaxVertexBufferSlots = 16;
```

**`[[nodiscard]]` on:**
- All `isValid()` / `isEmpty()` checks
- All getter methods
- Any function where ignoring the return value is a bug

**Assertions:**
```cpp
DO_ASSERT(condition, "Message");
DO_ASSERT(false, "Message");  // unreachable
DO_ERROR("Message");
```

**Pass-level constant structs** — defined as nested structs in the pass header:
```cpp
struct MainCameraPassConstants {
    Matrix4f view_projection{1.0f};
    Vector4i draw_data{0};
    Vector4f material_data{0.0f, 1.0f, 1.0f, 0.0f};
};
```

## Render Pass Pattern

```cpp
class MyPass : public RenderPass {
    // Resource names as inline static const
    // GPU handles
    // Framebuffer handles
public:
    explicit MyPass(RhiContext* rhi) { m_rhi = rhi; }
    ~MyPass() override = default;
    void setup() override;
    void execute(size_t index) override;
    void cleanup() override;
    void onViewportResize(const Vector2i& viewport_extent) override;
    void onWindowResize(const Vector2i& window_extent) override;
private:
    void createFramebuffer();
    // other create* methods
};
```

## What NOT To Do

1. **No `F` prefix** — not `FMeshBatch`, `FMeshDrawCommand`, `FMeshPassProcessor`. The project is NOT Unreal Engine.
2. **No UE class copies** — design your own architecture, don't copy-paste UE source.
3. **No `std::` types in interfaces** — use `DynamicArray<T>` not `std::vector<T>`, `Scope<T>` not `std::unique_ptr<T>`.
4. **No global singletons for draw pipelines** — each pass owns its processor.
5. **No `IsValid()` with capital I** — it's `isValid()` per project convention.
6. **No traditional header guards** — always `#pragma once`.
7. **No public variables before private** — class declaration order is private → protected → public.
8. **No static functions in camelCase** — static class functions are PascalCase like `Self()`, `Create()`.
