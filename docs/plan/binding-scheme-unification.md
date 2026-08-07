# 绑定方案统一重构：现状与计划

> 日期：2026-08-05
> 分支：`do-dev`
> 状态：**半迁移中，当前工作区 DX12 不可运行**（详见"当前现状"）

## 1. 背景与问题

引擎存在**两套绑定编号体系**，导致 material 系统（唯一从反射建 binding layout 的消费者）崩溃：

| 层 | CB 绑定号 | sampler | 纹理 |
|---|---|---|---|
| 引擎显式 layout desc | 0（自然 b0） | 0 | 0+ |
| HLSL / .dxil（DX12 渲染） | 0（b0） | 0 | 0 |
| GLSL / .spv（反射 + Vulkan） | **256** | **128** | 0 |
| 反射数据（读 .spv） | **256** | **128** | 0 |

material 系统拿反射的 256 去建 layout → 与 DX12 的 b0 错位 → `duplicate bindings: b256` → 管线创建失败 → Device Removed（`0x887A0001`）。Only2D 正常是因为它的 pass 全部用显式 b0 layout，从不消费反射。

**根因**：HLSL→SPIR-V 编译时把 cutie 的 `VulkanBindingOffsets`（CB=256 / sampler=128）烤进了 `.spv`，而引擎 layout 和 `.dxil` 都用自然号 b0。两套编号只在"反射 ↔ 引擎"这一处接缝暴露。

**目标**：写**一套**绑定号，DX12 / Vulkan / OpenGL 全部可用，不再有"反射转换"补丁。

## 2. 目标方案：自然绑定号 + register space = descriptor set

绑定号保持自然（CB@0、sampler@0、纹理@0+），用 **register space** 区分资源类型：

| 资源 | registerSpace（DX12 根签名） | Vulkan descriptor set | 说明 |
|---|---|---|---|
| ConstantBuffer / PushConstants | 0 | set 0 | |
| Sampler | 1 | set 1 | |
| SRV（纹理 / StructuredBuffer） | 2 | set 2 | |
| bindless 描述符表 | 3 | set 3 | 独占空间 |
| UAV | 4 | set 4 | |

- **DX12**：根签名天然支持多 register space，`register(b0, space0)` / `(s0, space1)` / `(t0, space2)` 互不冲突。
- **Vulkan**：`registerSpaceIsDescriptorSet = true`（cutie 已支持，`vulkan-resource-bindings.cpp:1060-1064`）→ register space = descriptor set 索引；`VulkanBindingOffsets` 置零（绑定号已是最终值）。
- **OpenGL**：register space 被忽略，按 slot 绑到各自的 GL 命名空间（UBO / sampler unit / texture unit 是分开的）→ binding 0 各不冲突，且是低号、在 GL 上限内（`GL_MAX_UNIFORM_BUFFER_BINDINGS` 通常 36-96，Vulkan 方案的 256/128 会超限，所以**必须用自然号**）。

## 3. 当前现状（2026-08-05 已完成部分）

### 3.1 已完成的改动（工作区）

**Shader 层（Phase A）**
- 19 个 GLSL shader 改为自然绑定 + 按类型分 set：CB `set0/binding0`、sampler `set1/binding0`、普通纹理 `set2/binding0..N`、bindless `u_Textures[]` `set3/binding0`。（文件：`engine/res/shaders/*.vert|*.frag|*.geom`）
- 用 `glslangValidator -V` 重编 19 个 `.spv`（源码 `engine/res/shaders/bin/` 已同步到部署目录 `bin/sandbox-debug/engine/res/shaders/bin/`）。
- 渲染用 HLSL 加 register space：sampler → `space1`、普通纹理 → `space2`、bindless → `space3`、CB 保持 space0。涉及 sprite/ui/main_camera/deferred_light/skybox/tone_mapping/color_grading/fxaa/combine(imgui/gizmo 的 cbuffer 无变化)。**注意：compute shader（gpu_culling/bucket_*/build_indirect_args）尚未改。**

**引擎层（Phase B，部分）**
- `descriptor_table_manager.cpp`：bindless 表 `registerSpaces` 从 `Texture_SRV(0)` → `Texture_SRV(3)`（DX12 上表落到 space3，匹配 HLSL）。
- `gbuffer_mesh_processor.{h,cpp}`：layout 拆成 CB(space0) + sampler(space1)，binding set 对应拆成 2 个，draw command 追加 sampler set。
- `directional_shadow_mesh_processor.cpp`：layout 加 `setRegisterSpaceIsDescriptorSet(true)`（仅 CB，无拆分）。
- `base_scene_feature.cpp`：`MakeGBufferPipelineDesc` 增加 sampler layout 参数；非 bindless 纹理 layout 设 space2。
- `sprite_feature.{h,cpp}` + `render_sprite_pass.{h,cpp}`：layout 拆成 CB(space0)/sampler(space1)/texture(space2) 3 个；bindless 与 array 两条路径的 binding set、pipeline addBindingLayout、binding_sets 向量全部对应拆分。

**其他独立修复（此前已做）**
- `render_phase.h`：`UI` 移到 `PostProcess` 之后（UI 在色调映射后最后叠加）。
- `render_sprite_pass.cpp`：Sprite 优先合成进 `SceneHdrKey`（HDR 场景缓冲）+ 用 `SceneTexturesKey->depth` 做深度遮挡（`LessOrEqual`、只测不写）。
- `material_system.cpp`：反射建 layout 时去重（VS/PS 共享绑定）。
- `render_target_handle.cpp`：color/depth 纹理改用 `enableAutomaticStateTracking`（设置 `keepInitialState`），修复 D3D12 "Unknown prior state"。
- 工作区原有（非本次会话）：`application.*` config 加载、`sandbox_layer` 导入 backpack、`EditorContext.cpp`、`ComponentSet.cs`、`imgui.ini` 等。

### 3.2 未完成 / 半迁移状态（关键）

> **当前工作区 DX12 不可运行。** HLSL 已声明 register space，但大量引擎 layout 还没拆分，space 不匹配会导致运行时绑定失败。

Phase B 尚未做的拆分：
- **UI**（`ui_feature.{h,cpp}` + `render_ui_pass.{h,cpp}`）— 与 sprite 结构完全相同，照 sprite 模式拆。
- **ImGui**（`imgui_feature.{h,cpp}` + `render_imgui_pass.cpp`）— PushConstants+Texture+Sampler 拆 push/texture/sampler。
- **Gizmo**（`gizmo_feature.{h,cpp}` + `render_gizmo_pass.cpp`）— PushConstants+CB，加 flag 即可。
- **DeferredLightPass**（`render_deferred_light_pass.cpp`）— CB+Sampler+6 纹理拆 3。
- **TestPass**（`render_test_pass.cpp`）— Texture+Sampler 拆 2。
- **MaterialSystem**（`material_system.cpp:271-275` 显式 layout）— 纹理 layout 设 space2。
- **GpuDrivenRenderer**（`gpu_driven_renderer.cpp` 3 处）— CB/SRV/UAV 按 space0/2/4 拆。
- **ShaderBindingReflector**（`shader_parameter.h`）— skybox/postprocess/postprocess2d/present 共用，makeLayoutItem 按类型分 space，createBindingSet 拆分。
- **compute shader HLSL**（`gpu_culling/bucket_*/build_indirect_args`）— 加 register space。

尚未做的阶段：
- **Phase C**：cutie Vulkan 后端 `VulkanBindingOffsets` 置零（引擎所有 `GfxBindingLayoutDesc()` 显式 `.setBindingOffsets(VulkanBindingOffsets{})`，或改 cutie 默认）。
- **Phase D**：`ShaderReflector::ReflectSPIRV` 补读 `spv::DecorationDescriptorSet`，`Shader*Reflection` 加 `set` 字段；material 反射建 layout 按类型分 space。
- **Phase E**：重编全部 `.spv`/`.dxil`，编译，验证三后端。

### 3.3 已定位但尚未处理的隐患

- `binding_layout_generator.cpp` 的 `MergeReflection` 按 (slot, kind) 去重，但 `ShaderResourceKindToBindingItem` 忽略 `array_size` 参数；反射也不读 set。
- `blinn_phong.glsl`（OpenGL 专用材质 shader）用顺序绑定 set0/b0-b4，与新的按类型 set 方案不一致，未改。
- OpenGL 后端 bindless 与 compute binding 在 cutie 中未实现（`opengl-backend.h:554`、`opengl-commandlist.cpp:492`）。

## 4. 重构方案（完整路线）

### Phase A — Shader 改自然绑定 + 分 space/set
- [x] GLSL：19 个文件按类型分 set，绑定号取自然。
- [x] 重编 `.spv`（glslangValidator）。
- [x] 渲染 HLSL 加 register space。
- [ ] compute HLSL 加 register space（gpu_culling/bucket/build_indirect）。
- [ ] 重编 `.dxil`（dxc，`engine/res/CMakeLists.txt:73,88,103`）。

### Phase B — 引擎 layout 按类型拆多 register space
- [x] GBufferMeshProcessor / ShadowMeshProcessor / base_scene_feature（GBuffer 路径）
- [x] SpriteFeature + SpritePass
- [ ] UIFeature + UIPass（照 sprite 模式）
- [ ] ImGuiFeature+Pass、GizmoFeature+Pass
- [ ] DeferredLightPass、TestPass
- [ ] MaterialSystem 显式 layout、GpuDrivenRenderer
- [ ] ShaderBindingReflector（shader_parameter.h）
- [ ] bindless 表（已做 space3）+ 各 pass 的 bindless addBindingLayout 核对

### Phase C — cutie Vulkan 后端
- [ ] `VulkanBindingOffsets` 置零（引擎所有 layout 显式 `.setBindingOffsets(VulkanBindingOffsets{})` 或改 cutie 默认），`vulkan-resource-bindings.cpp:49-101` 不再加偏移。
- [ ] 确认 `registerSpaceIsDescriptorSet` 在 `createPipelineLayout` 生效。

### Phase D — 反射
- [ ] `ReflectSPIRV` 补读 `spv::DecorationDescriptorSet`。
- [ ] `Shader*Reflection` 结构加 `set` 字段。
- [ ] material 系统按 set 建 layout（CB set0→space0、sampler set1→space1、纹理 set2→space2），去重逻辑改用 `BindingLayoutGenerator::MergeReflection` 按 (set, slot, kind) 合并。
- [ ] 反射现在读自然号，与引擎约定一致，无转换。

### Phase E — 验证
1. 重编，跑 DX12 Deferred（`"pipeline":3`）+ Only2D（`"pipeline":5`）：无 b256、无 SpriteColor 断言、无 Unknown prior state；backpack 3D 场景 + sprite 遮挡 + UI 覆盖正常。
2. 跑 Vulkan（`"api":2`）与 OpenGL（`"api":1`）：绑定一致，无 descriptor set 冲突。
3. RenderDoc 捕获：检查根签名各 space 的绑定、Vulkan 各 set 的绑定号与 shader 声明一致。

## 5. 关键风险 / 注意

- **OpenGL**：bindless 与 compute binding 在 cutie 里未实现，"全部能用"对 OpenGL 的 bindless 路径需要单独补齐。
- `c_MaxBindingLayouts = 8`（`cutie.h:76`）：每 pass 最多 4 个 layout（CB/sampler/纹理/bindless），在限制内。
- `registerSpaceIsDescriptorSet` 要求同一管线所有 layout 该标志一致（validation-device.cpp:1174-1178）。
- Phase A 改动后 `.spv`/`.dxil` 需全量重编；`scripts/convert_shaders_dxil.sh` 的 .spv→.dxil 路径与 `engine/res/CMakeLists.txt` 的 HLSL→.dxil 直接编译并存，注意保持一致。
- **半迁移状态不要提交**：当前 Phase B 只完成一部分，DX12 不可运行，需要按 Phase B 剩余项补齐后才能验证。

## 6. 参考文件

- 反射绑定来源：`engine/src/runtime/function/render/shader/shader_reflection.cpp`
- 反射消费者：`engine/src/runtime/function/render/material/material_system.cpp`
- Vulkan 偏移应用：`engine/external/cutie-rhi/src/vulkan/vulkan-resource-bindings.cpp:49-101`
- registerSpace→descriptor set：`engine/external/cutie-rhi/src/vulkan/vulkan-resource-bindings.cpp:1001-1108`
- 各后端 layout 构造：`engine/src/runtime/function/render/render_pipeline/**`、`mesh_draw/**`
