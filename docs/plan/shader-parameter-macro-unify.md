# 宏系统 UE 化重构与 pass 统一方案

## 范围与结论

当前 `BEGIN_SHADER_PARAMETER_STRUCT` 是 UE 宏的简化移植：绑定号写死在宏里（`SHADER_PARAMETER(TextureSRV, 1, scene_color)`），成员要手写在 `END_SHADER_PARAMETER_STRUCT(func(a); func(b); ...)` 里。本方案向 UE 靠拢——按名字绑定（C++ 成员名 == shader 资源名）、自动收集成员（不再手写 func 列表），然后把所有手写 `createBindingSet` 的 pass 统一到宏风格上。

已确认同步改 shader 资源名为干净名字（`u_Albedo` → `Albedo`、`DeferredLightPassUBO` → `LightParams` 等），使 C++ 成员名与反射出的 GLSL 资源名一致。

非目标：sprite/ui 的 bindless 描述符表本体（无法用单资源宏表达）、imgui pass（debug-only、逐 draw 换纹理）保持手写。

## Phase 3 — Buffer SRV/UAV 与 Compute

宏参数体系已扩展到单资源的完整常用资源集合：Texture SRV/UAV、Typed Buffer SRV/UAV、Structured Buffer SRV/UAV、Raw Buffer SRV/UAV、Sampler、Constant Buffer、Volatile Constant Buffer 与 push constant。`ShaderBindingReflector` 仍以 shader 反射为准解析 set/slot/kind，并依据反射资源类型创建相应的 `GfxBindingSetItem`；Typed Buffer 可通过成员的 `format` 指定视图格式，所有 buffer 类型支持 `range`。

新增宏分为两层：

- `SHADER_PARAMETER(Type, member)` / `SHADER_PARAMETER_NAMED(Type, resource, member)`：RenderGraph 资源版本，`Type` 可使用新的 TextureUAV、TypedBufferSRV/UAV、StructuredBufferSRV/UAV、RawBufferSRV/UAV。
- `SHADER_PARAMETER_RAWTEX_UAV`、`SHADER_PARAMETER_TYPED_BUFFER`、`SHADER_PARAMETER_STRUCTURED_BUFFER`、`SHADER_PARAMETER_RAW_BUFFER`、`SHADER_PARAMETER_RAWVOLATILECB` 及各自 `_UAV` / `_NAMED` 版本：直接持有 `GfxTextureHandle` 或 `GfxBufferHandle` 的版本，适合非 RenderGraph 的 compute 和 mesh 路径。

GPU culling、bucket count、bucket fill、GizmoPass，以及 GBuffer/DirectionalShadow 的固定 Global/View/Primitive 资源组都已迁移为宏参数结构。Sprite/UI 的 bindless 路径也已迁移其固定的 Camera/Sampler 资源；descriptor table 本体仍由专用代码追加。逐 draw 的 ImGui 纹理/scissor、以及 MaterialSystem 的动态材质模板仍保留专用路径；它们不是“单个按名资源”的参数模型。

## 当前状态

绑定宏与 binder 位于 `runtime/function/render/shader/shader_parameter.h`：
- `ShaderParameter<Type, ValueT>` 仅保存资源名和值；set/slot/type 均由传入的 shader reflection 决定。
- `END_SHADER_PARAMETER_STRUCT()` 自动遍历聚合体成员，不再维护手写成员列表。
- `ShaderBindingReflector<T>` 按反射资源的 descriptor set 分组创建 layout/binding set，并按反射组合与可见性缓存 layout。

引擎已有完整 shader 反射（`runtime/function/render/shader/shader_reflection.h/.cpp`）：
- `ShaderReflectionData` 带 constant_buffers / textures / samplers，各有 name / set / slot（纹理还带 kind / dimension），由 SPIRV-Cross 反射 GLSL 资源名得到。
- `ShaderLibrary::getReflection(name)` 按 shader 名取反射，shader 名即 pass 使用的名字（`FullscreenVS` / `PresentPS` / `SkyboxPS` / `DeferredLightPS` / `SpriteVS` / `SpriteTraditionalPS` / `UIVS` / `UIPSArray` / `TestVS` / `TestPS`）。

按反射自动建 binding layout 的先例在 `runtime/function/render/material/material_system.cpp`（`resolveTemplate`）：合并 VS+PS 反射、按 (slot, type) 去重、`ShaderResourceKindToBindingItem` 映射 kind→item、经 `binding_layout_cache->getOrCreate` 缓存。

结论：名字绑定不需要新增反射基础设施；binder 复用 material_system 的逻辑，按 set 分组 + 按名字解析 slot。绑定解析发生在运行时（反射是运行时数据），所以成员名用运行时 `const char*` 成员即可，不需要 C++20 类类型 NTTP。

## 目标与非目标

目标：
- 宏语法对齐 UE：无 binding 号、无 func 成员列表。
- 按名字绑定：C++ 成员名 == GLSL 资源名，set/slot 由 shader 反射自动解析。
- 自动收集成员：聚合体遍历替代手写 `func(a); func(b); ...`。
- 统一全部手写 pass：test、deferred_light、sprite（traditional）、ui（array），以及既有宏用户 present / skybox / post_process / post_process_2d。
- 支持子缓冲 CB（deferred_light 的 staging 分配）与 cube 纹理维度。

非目标：
- sprite/ui 的 bindless 描述符表绑定（保持手写）。
- imgui pass（保持手写）。

## Phase 1 — 重写宏系统

核心文件：`runtime/function/render/shader/shader_parameter.h`（重写）+ 新增聚合体遍历小工具。

### 成员带名字

`ShaderParameter<Type, Set, ValueT>` 增加成员 `const char* kName{}`，宏用 `#name` 生成；保留 `value` 成员，pass 代码 `params.member.value = ...` 不变。ConstantBuffer 变体增加可选成员 `GfxBufferRange range{cutie::EntireBuffer}`，支持子缓冲 CB。`makeLayoutItem(slot)` / `addToBindingSet(desc, slot, resolved)` 改为实例方法、运行时传 slot。

### 宏语法

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(LightPassParams)
    SHADER_PARAMETER_RAWCB_NAMED(LightParams, light_cb)
    SHADER_PARAMETER_RAWTEX_NAMED(Albedo, albedo)
    SHADER_PARAMETER_NAMED(Sampler, Sampler, sampler)
END_SHADER_PARAMETER_STRUCT()
```

- `BEGIN_SHADER_PARAMETER_STRUCT(StructName)` 去掉 set 参数；`SHADER_PARAMETER_SET(...)` 删除——成员的 set/slot 绑定时从反射解析。
- 宏集合：`SHADER_PARAMETER(type, name)`、`SHADER_PARAMETER_NAMED(type, resource, member)`、`SHADER_PARAMETER_RAWCB`、`SHADER_PARAMETER_RAWTEX`、`SHADER_PARAMETER_PUSH_CONSTANTS(type, name)`；资源名与成员名不同的情况使用对应 `_NAMED` 版本。
- `END_SHADER_PARAMETER_STRUCT()` 无参。

### 自动收集成员

参数结构体是纯聚合（只有 ShaderParameter 成员 + 静态成员），用聚合体计数技巧实现 `forEachField(T&, F&&)`（成员数检测 → structured binding 解包），替换手写 func 列表。

风险：聚合体计数在 MSVC 下的正确性——先在单独小 TU 编译验证再铺开。

### 按名字解析的 binder

改 `ShaderBindingReflector`：

- `getOrCreateLayouts(vs_refl, ps_refl)`：按反射里的 set 分组建 `GfxBindingLayoutDesc`（`setRegisterSpaceIsDescriptorSet(true).setRegisterSpace(set)`），组内按 material_system 的方式 (slot, type) 去重、`ShaderResourceKindToBindingItem` 映射；经 `binding_layout_cache->getOrCreate` 缓存（key 由 shader 名派生）。
- `createBindingSets(cmd, layouts, params, vs_refl, ps_refl, resolveTex, resolveBuf)`：`forEachField` 遍历成员，用 `member.kName` 在合并反射里查 (set, slot, kind)，查不到报 `DO_ERROR`，填 per-set `GfxBindingSetDesc` 并 `createBindingSet`。
- 保留 `ShaderReflector::ValidateAgainstLayout` 校验。

## Phase 2 — 统一 pass

### shader 资源改名

按名字绑定的前提是 C++ 成员名 == 反射出的 GLSL 资源名。改完后 `res/CMakeLists.txt` 用 DXC 自动重编译 .spv/.dxil。纯文本改名，风险低。

| 文件 | 现名 → 新名 |
|---|---|
| `res/shaders/deferred_light_pass.frag` | `DeferredLightPassUBO` → `LightParams`；`u_Albedo/u_Normal/u_Position/u_ShadowMap/u_Material/u_SkyboxTexture` 去 `u_`；`u_Sampler` → `Sampler` |
| `res/shaders/sprite_pass.vert` | `SpriteCameraUBO` → `CameraParams` |
| `res/shaders/sprite_pass_traditional.frag` | `u_Texture` → `Texture`，`u_TextureSampler` → `Sampler` |
| `res/shaders/ui_pass.vert` | `UIVP` → `CameraParams` |
| `res/shaders/ui_pass_array.frag` | `u_Texture` → `Texture`，`u_TextureSampler` → `Sampler` |
| `res/shaders/combine_pass.frag`（PresentPS） | `CombinePushConstants` → `PresentViewportCB`；`u_SceneTexture/u_ImGuiTexture/u_TextureSampler` → `SceneTexture/ImGuiTexture/Sampler` |
| skybox / tone_mapping / color_grading / fxaa / test 各 shader | 实现时逐个读 GLSL，同样规范化块名 + 去 `u_` |

> 不改 sprite/ui 的 bindless shader（`sprite_pass.frag` / `ui_pass.frag` 的 `u_Textures` 描述符表绑定不参与名字绑定）。

### 转反射宏

- 既有宏用户：present / skybox / post_process / post_process_2d——新语法（无 binding 号 + 无 func 列表），绑定代码换成新 binder。
- 手写 pass 转宏：test、deferred_light、sprite（traditional 路径）、ui（array 路径）。每个 pass 的 shader 名对传给 binder（present = `{"FullscreenVS", "PresentPS"}`、sprite = `{"SpriteVS", "SpriteTraditionalPS"}` 等）。
  - deferred_light：CB 成员 `.range = GfxBufferRange(offset, size)`（staging 子缓冲）；cube 纹理 dimension 从反射拿到，SRV item 自动带维度。
  - sprite/ui traditional：View set 的 `RAWCB(CameraParams, ...)` + Material set 的 `RAWTEX/Sampler`，跨 set 由反射自动分组（不再需要 `SHADER_PARAMETER_SET`）。
- 保留手写：sprite/ui bindless 的 descriptor table 追加、imgui。
- pass 构造 / feature：`render_sprite_pass.h` / `render_ui_pass.h` 构造函数去掉手写 binding-layout 参数、删成员；`sprite_feature.cpp` / `ui_feature.cpp` 不再创建这些 layout，固定 View/Sampler layout 由反射在 pass 内获得。

## 验证

1. 对改动文件运行 `git diff --check`，确认无空白错误。
2. 编译与运行编辑器不在本次执行范围内（仓库指令禁止编译）。
3. 后续允许验证时，用 RenderDoc 抓帧核对 SpritePass / UIPass / GBuffer / Shadow / Compute 的 binding 无缺失、draw/dispatch 数正确；确认 binding 与 shader 反射一致。
