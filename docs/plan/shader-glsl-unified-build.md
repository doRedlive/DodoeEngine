# Shader 体系统一到 GLSL（构建链自动化）

范围与结论
本方案把引擎 shader 体系收敛为**单一来源（GLSL）+ 一条自动化构建链**：GLSL 同时产出 `.spv`（Vulkan 字节码 + 运行期反射源）和 `.dxil`（DX12 字节码），compute 也并入同一逻辑，不再维护独立 HLSL 源。本次**只改构建链，不碰引擎 pass 布局**（DX12 因 register 约定变化暂无法建 PSO，布局迁移留作后续单独一轮）。

## 现状与问题

当前 shader 是**两套并行**：

- `engine/res/CMakeLists.txt` 用 `glslangValidator` 把 `shaders/*.vert|frag|geom` 编成 `.spv`（Vulkan/OpenGL 用）
- 同一 CMake 用 **24 个 legacy 图形 `.hlsl`**（`register(b0)` 那套）+ dxc 直编 `.dxil`（DX12 用），靠 `HLSL_VS/PS/CS_PAIRS` 三个硬编码列表维护
- `scripts/convert_shaders_dxil.sh` 手动做 GLSL→spv→HLSL→dxil（产物 register 与 CMake 的 HLSL 链不一致，是坑）

两套语言（GLSL 的 `binding=256/128/0` 约定 vs HLSL 的 `register(b0/s0/t0)`）必须手工同步；compute 单独走 HLSL。

**关键事实（已实测）**：spirv-cross 把 SPIR-V `(set, binding)` 1:1 映射到 HLSL `(space, register)`，**无 register 重映射能力**（`--remap` 只改变量名）。因此 GLSL→DXIL 后 D3D12 register 变为 GLSL 约定：`256=CB / 128=sampler / 0=texture / 384=UAV`。当前引擎 pass 布局用 `b0/s0/t0`，故本次改动后 DX12 暂不工作。

## 目标链路（唯一逻辑）

```
shaders/*.vert|frag|geom|comp   （GLSL，唯一作者入口）
   │ glslangValidator -V
   ▼
bin/<name>.<stage>.spv          （Vulkan 字节码 + 运行期反射源）
   │ spirv-cross --hlsl --stage X
   ▼
(dxil_src 临时 HLSL)
   │ dxc -T <stage>_6_0 -E main
   ▼
bin/<name>.<stage>.dxil         （DX12 字节码）
```

- vert→vs_6_0，frag→ps_6_0，geom→gs_6_0，comp→cs_6_0
- 全部放进 `engine/res/CMakeLists.txt`（构建自动、增量依赖），**不新增独立脚本**

## 改动清单

### A. 重写 `engine/res/CMakeLists.txt`
- 保留 glslang `.spv` 段，`file(GLOB ...)` 增加 `*.comp`
- 删除 `HLSL_VS/PS/CS_PAIRS` 三份列表及对应 dxc 直编 custom command
- 新增：每个 `.spv` → `spirv-cross --hlsl --shader-model 60 --stage <s> --output <build临时hlsl> <spv>` + `dxc -T <target> -E main <临时hlsl> -Fo <dxil>`，一条 `add_custom_command`（多 COMMAND）
- 中间 HLSL 放 `${CMAKE_CURRENT_BINARY_DIR}/dxil_src/`，不污染源树
- `find_program(SPIRV_CROSS spirv-cross)`；spirv-cross 与 dxc 任一缺失则跳过 dxil 并 WARNING（glslang 仍 REQUIRED）
- 保留 `shaders` / `shaders_dxil` target

### B. 移植 3 个 compute 到 GLSL（`.comp`）
`gpu_culling_pass.comp` / `bucket_count_pass.comp` / `bucket_fill_pass.comp`，统一 `layout(set=0, binding=...)` + `layout(std430,...)` + `layout(local_size_x=64) in;`，binding 遵循 256/0/384 约定，输入 SSBO 标 `readonly`（spirv-cross 才生成 `StructuredBuffer` t 寄存器而非 RWStructuredBuffer）：

| shader | CB | SRV | UAV |
|---|---|---|---|
| gpu_culling | 256 (CullingParams) | 0/1/2 (ObjectMeta/Transform/Bounds) | 384/385 (VisibleObjects/VisibleCount) |
| bucket_count | 257 (BucketParams) | 0/1 (VisibleObjects/ObjectMeta) | 384 (BucketCounts) |
| bucket_fill | — | 0/1/2 (VisibleObjects/ObjectMeta/Transform) | 384/385/386 (IndirectArgs/BucketOffsets/BucketCounts) |

要点：
- `InterlockedAdd` → GLSL `atomicAdd`（返回旧值）
- bucket_fill 的 `BucketCounts` 旧 HLSL 是 `u2` 可写 UAV，但引擎 C++ 布局错绑成 SRV t3（**既有 bug**）——GLSL 按可写 UAV(386) 移植，引擎侧留给后续布局迁移修正
- struct 用 std430 对齐 HLSL（`GpuObjectMeta`/`GpuBounds`/`GpuTransform`/`BucketCount`/`DrawIndexedIndirectArgs`）

### C. 删除旧文件
- `scripts/convert_shaders_dxil.sh`
- 全部 legacy `.hlsl`（24 个图形 + build_indirect_args_cs.hlsl + 3 个将被 .comp 取代的 compute）
- `blinn_phong.glsl`（无引用）
- 无对应 pass 的 shader：`pick_pass.vert/frag`、`point_light_shadow_pass.vert/frag/geom`、`test_pass_*.hlsl`
- 对应 stale bin：`pick_pass.*`、`point_light_shadow_pass.*`、`test_pass.*`

### D. `shader_manifest.json` 清理
删除死条目 `TestVS/TestPS`、`PickVS/PickPS`、`PointLightShadowVS/PS/GS`。保留全部活条目。compute 条目 platforms 暂保持 `["dx12"]`。

### E. 明确不做（本次）
- 不碰引擎 pass 绑定布局 / binding set / 反射槽位
- 不移动 bindless 纹理数组 `set=1`→`set=0`
- 不删 `render_test_pass.cpp/h`（未被注册，编译不受影响）

## 后续布局迁移提示（单独一轮）

恢复 DX12 渲染时需同步引擎布局到 GLSL 约定：
- 所有 pass 布局/绑定集：CB `0→256`、Sampler `0→128`、UAV `0..→384..`（SRV 槽不变）
- 涉及活 pass：GBuffer/Shadow 处理器、DeferredLight、Sprite/UI（bindless+array）、ImGui、Gizmo、GPU Culling 三个 compute 布局及对应 `GfxBindingSetItem` slot
- 把 bindless 纹理数组 `main_camera_pass/sprite_pass/ui_pass/main_camera_pass_nobindless` 从 `set=1` 移到 `set=0, binding=0`（否则 D3D12 落 space1，bindless 表在 space0 够不到）
- 修 bucket_fill：引擎布局 `StructuredBuffer_SRV(3)=BucketCounts` 改 `StructuredBuffer_UAV(386)`

## 验收

1. `cmake --preset` 重配置，`shaders`/`shaders_dxil` target 正常生成
2. 改一个 GLSL → 增量构建 → 对应 `.spv` 与 `.dxil` 都重新生成；`dxil_src/` 在 build 目录而非源树
3. spirv-cross 抽查 compute：readonly SSBO → `StructuredBuffer : register(t0..)`，writable → `RWStructuredBuffer : register(u384..)`
4. 跑引擎（Vulkan/OpenGL 后端）正常渲染，无 `failed to read shader file` 报错
5. DX12：预期 PSO 创建失败（本次接受，作为布局迁移起点依据）
