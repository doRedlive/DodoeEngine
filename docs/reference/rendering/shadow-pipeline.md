# 阴影管线与 GPU 绘制原理

本文梳理引擎阴影渲染的完整数据流，以及支撑它的 GPU 绘制基础机制（drawcall 与 shader 的关系、顶点缓冲槽映射、instancing）。

## 1. 总览：阴影 = 两次独立绘制

阴影渲染没有任何"一步到位"的魔法，它由两个互不相干的 drawcall 阶段组成，中间靠一张深度图衔接：

```
第一次绘制（ShadowPass）                     第二次绘制（LightPass）
从光源视角把场景画一遍，只写深度               从相机视角正常打光，
                                            把第一张图当普通纹理查询
        │                                            ▲
        └────────── shadow map（D32 atlas）──────────┘
```

- 写入方：`ShadowPass` 用 shadow shader 把场景渲染到 shadow map
- 读取方：lighting shader（forward/deferred）采样 shadow map，判断每个像素是否被遮挡
- 核心契约：两次绘制使用**同一份 cascade 矩阵**，写入坐标和查询坐标才能对上

## 2. 两套实现

引擎里有两条并行的渲染路径，阴影逻辑各有一份：

| | Baseline 路径 | RenderGraph 路径 |
|---|---|---|
| Pass 类 | `BaselineShadowPass` (`src/runtime/service/reference/passes/baseline_shadow_pass.cpp`) | `ShadowPass` (`src/runtime/function/render/render_pipeline/passes/render_shadow_pass.cpp`) |
| 编排方式 | 直接持有 framebuffer/pipeline，`render()` 返回 `BaselineShadowResult` | RenderGraph 声明式，产出经 blackboard `ShadowMapKey` 传递 |
| 实例数据 | 直接使用 `mesh_ext->instance_scene_data` | 专用 `caster_instance_buffer`（见 §3.2） |
| 数据准备 | `ShadowSystem::BuildFrameData` | `ShadowSystem::SetupView` → `ShadowViewExtension` |

两者共享同一套 shader 资源：`directional_light_shadow_pass.vert/.frag`（生成深度图）和 `shadow_csm.glsl`（采样深度图）。

## 3. RenderGraph 路径详解

### 3.1 数据准备：ShadowSystem

`ShadowSystem::BuildFrameData`（`shadow_system.cpp`）每帧计算：

1. 从场景中找到首个投射阴影的平行光方向，构建 light view
2. 按相机视锥切 4 个 cascade，`BuildCascadeViewProjection` 计算每个 cascade 的正交投影（含 texel 对齐量化，防止边缘闪烁）
3. `CollectShadowCasters` 收集投影物：过滤 `castsShadow()`、按 cascade 视锥剔除，产出：
   - `shadow_caster_instance_data`：所有 caster 实例数据的**紧凑压平数组**
   - `shadow_caster_instance_offsets`：每个 primitive 在紧凑数组中的起始偏移
   - `shadow_caster_cascade_masks`：每个 caster 落在哪些 cascade 里（bitmask）
4. 结果存入 `ShadowViewExtension`，挂在该帧所有 view 上

### 3.2 caster_instance_buffer 是什么

它是 **ShadowPass 专用的 instanced 顶点缓冲**，完整链路：

```
ShadowSystem 打包紧凑实例数组
        │
        ▼
ShadowPass::build 在 render graph 中创建 transient buffer
        │
        ▼
execute 时 writeBuffer 上传紧凑数组            (render_shadow_pass.cpp:91-97)
        │
        ▼
作为第 5 参数传入 SubmitMeshDrawSources        (render_shadow_pass.cpp:118-120)
        │
        ▼
内部绑定为 vertex buffer slot 1，              (mesh_processor_base.cpp:258-264)
每个 drawcall 用 setOffset(instance_offset)
指向紧凑数组中自己那一段
```

**为什么不复用主 pass 的实例缓冲**：shadow caster 集合与主 pass（gbuffer/opaque）不同——多了 `castsShadow()` 过滤和 cascade 剔除，数量、顺序、偏移空间都不一样，必须独立打包一份。每个 draw source 的 `instance_offset` 在 `buildMeshDrawCommands` 中由 `primitive_first_instance + instance_range` 算出（`mesh_processor_base.cpp:188-192`），前者正是传入的 `shadow_caster_instance_offsets`。

### 3.3 绘制循环

每个 cascade 一轮（`render_shadow_pass.cpp:111-121`）：

1. 把该 cascade 的 view-projection 写入 view 常量缓冲
2. `MakeCascadeViewport` 把 viewport 切到 atlas 对应象限（2x2 布局，cascade 0~3 = 左上/右上/左下/右下）
3. `SubmitMeshDrawSources(..., 1u << cascade)` 提交该 cascade 覆盖的 drawcall

shader 侧只有一个空的 fragment shader——深度由 OM 阶段自动写入。

## 4. 采样侧：shadow_csm.glsl

`shadow_csm.glsl` **不是 shadow pass 用的**，它是被光照 shader `#include` 的采样库：

- `forward_lit_pass.frag:47`、`deferred_light_pass.frag:32` 包含它
- `computeShadow(...)` → `CsmComputeDirectionalShadow(...)`：按 view depth 选 cascade，用同一份 cascade 矩阵把世界坐标投回光源空间，PCF 采样比较深度
- RenderGraph 路径中，shadow map 经 blackboard `ShadowMapKey` 传递（`render_shadow_pass.cpp:79` 写入 → `render_deferred_light_pass.cpp:89,97` 读取），绑定为 `Texture_SRV(4)`；cascade 矩阵从 `ShadowViewExtension` 塞进 push constant（`render_deferred_light_pass.cpp:273-275`）

```
ShadowSystem 算好 4 个 cascade 矩阵（存 ShadowViewExtension，一次）
        │
        ├─→ ShadowPass:  矩阵写 ViewCB → 光源视角画深度 → 进 atlas
        └─→ LightPass:   矩阵写 push constant → lighting PS 查表 → 阴影因子
```

## 5. GPU 绘制基础：drawcall 与 shader 的关系

### 5.1 状态机模型

GPU 是一台状态机：绑缓冲、绑 shader、设 viewport 这些调用本身不画任何东西，只是修改"当前状态"。`drawIndexed` 是唯一触发器——"用当前这套状态，画这批索引"。一个 pass 画 1000 个物体就是 1000 次"改状态 + draw"循环。

### 5.2 渲染管线阶段

`drawIndexed` 之后，固定硬件流水线按序执行：

```
① IA 输入装配 → ② VS 顶点着色 → ③ 光栅化 → ④ PS 像素着色 → ⑤ OM 输出合并
   (按索引捞顶点)   (每顶点一次)   (三角形→像素)  (每像素一次)   (深度测试+写入)
```

- **IA**：按 index buffer 组装三角形；每个顶点附带 `VertexIndex` 和 `InstanceIndex` 两个隐形编号
- **VS**：每顶点执行一次，输入是 `layout(location=N) in`，输出 `gl_Position`
- **光栅化**：找出三角形覆盖的像素，插值顶点属性作为 PS 输入；viewport 变换在此发生（shadow pass 切 4 个 viewport 就是把三角形画到 atlas 4 个象限）
- **PS**：每像素执行一次
- **OM**：深度测试/写入。shadow pass "只写深度" 即：PS 为空 + depth write 开启，OM 照样写 D32

### 5.3 三条输入通道

| 通道 | 内容 | 粒度 | 对应 shader 声明 |
|---|---|---|---|
| 顶点缓冲 | 网格顶点、实例数据 | 每(顶点×实例) | `layout(location=N) in` |
| binding set | 常量缓冲、纹理、sampler | 整次 draw 共享 | `layout(set=S, binding=B) uniform` |
| pipeline | VS/PS 字节码、顶点格式、深度/剔除状态、渲染目标格式 | 建管线时固化 | — |

顶点缓冲是裸字节，格式由 input layout（`bufferIndex` + `offset` + `stride` + `isInstanced`）描述；binding set 是资源登记表，CPU 侧条目与 shader 侧 set/binding 编号对号入座。

**Instancing**：画 1000 棵相同的树，槽 0 只放一棵树的顶点，槽 1 放 1000 份世界矩阵，`setInstanceCount(1000)`——GPU 对每个实例重画整棵树，VS 每次拿到不同矩阵。实例属性在 attribute 描述上标 `setIsInstanced(true)`。

## 6. 顶点缓冲槽映射：GPU 如何知道从哪读

"location ↔ slot" 的映射**在建 PSO 时就烧成了固定规则**，draw 时的 `setSlot` 只是把槽编号对应到具体缓冲地址。以 D3D12 后端（cutie-rhi）为例：

### 6.1 建管线时（一次性）

引擎侧 attribute 携带名字、`bufferIndex`（= slot）、offset/stride/isInstanced（如 `shadow_scene_feature.cpp:141` 的 `setBufferIndex(1)`）。RHI 翻译为 `D3D12_INPUT_ELEMENT_DESC`（`cutie-rhi/src/d3d12/d3d12-shader.cpp:276-296`）：

```cpp
desc.SemanticName  = "TEXCOORD";           // 属性名去掉尾部数字
desc.SemanticIndex = 3;                    // "TEXCOORD3" → 3
desc.InputSlot     = attr.bufferIndex;     // 槽号写死在此
desc.InputSlotClass= PER_INSTANCE_DATA;    // isInstanced 决定按实例步进
```

引擎在 `InputLayoutCache::getOrCreate` 中把所有 attribute 统一重命名为 `"TEXCOORD" + index`（`input_layout_cache.cpp:44`），以对齐 SPIRV-Cross 从 GLSL location 生成的 HLSL 语义名。

### 6.2 draw 时

`addVertexBuffer(...).setSlot(1)` 在 RHI 层变成数组的**下标**（`d3d12-graphics.cpp:397-422`）：

```cpp
VBVs[binding.slot].BufferLocation = buffer->gpuVA + binding.offset;  // slot=1 → VBVs[1]
VBVs[binding.slot].StrideInBytes  = inputLayout->elementStrides[binding.slot];
commandList->IASetVertexBuffers(0, maxVbIndex + 1, VBVs);
```

### 6.3 真正取数

IA 硬件对每个输入 element 的运算本质：

```
地址 = VBVs[InputSlot].BufferLocation
     + (isInstanced ? InstanceIndex : VertexIndex) * Stride
     + AlignedByteOffset
读出 → 填进 VS 输入签名中 SemanticName+SemanticIndex 匹配的寄存器
```

```
建管线时（烧进 PSO）：
  GLSL location=3 ──SPIRV-Cross──▶ 语义 TEXCOORD3
  attribute.bufferIndex=1 ──────▶ InputSlot=1
  PSO 创建按语义名匹配 VS 输入签名 ⟹ 规则 "TEXCOORD3 ← 槽1" 固化

draw 时：
  setSlot(1) ──▶ VBVs[1] = caster_instance_buffer 地址（数字 1 把两边对上）

GPU 取数：
  第 j 个实例 → VBVs[1].地址 + j*80 字节 → VS 的 TEXCOORD3
```

Vulkan 后端同构：`VkVertexInputAttributeDescription.binding` = bufferIndex，`vkCmdBindVertexBuffers` 数组下标 = slot。

## 7. 实战串讲：一次 shadow drawcall 的生命周期

```
CPU 事先：
  ① 建 PSO：VS/PS = directional_light_shadow_pass.{vert,frag}，深度写开 + Less + depth bias
  ② 建 input layout：声明槽0/槽1 字节格式
  ③ 建 binding set：set1(View) 放 cascade 矩阵常量缓冲

每帧 execute：
  ④ writeBuffer(caster_instance_buffer, 紧凑实例数组)     ← 填槽1内容
  ⑤ 切 viewport 到 atlas 象限                             ← 决定光栅化画到哪
  ⑥ writeBuffer(view_cb, cascade 光源 VP 矩阵)           ← 填 set1 内容
  ⑦ setGraphicsState(fb=shadow_fb, pipeline, 2 顶点槽, 索引槽)
  ⑧ drawIndexed(indexCount, instanceCount)

GPU 执行：
  IA:  读索引 → 槽0捞树顶点、槽1捞第 j 份实例矩阵
  VS:  每(顶点×实例)一次：world = mat4(a_Model0..3)
       gl_Position = u_LightViewProjection * world * pos
  光栅化: 三角形画进 atlas 象限
  PS:  空
  OM:  "每个像素离光多远" 写进 shadow map
  （换 cascade：改 viewport + view_cb 矩阵，重复④~⑧，4 个 cascade = 4 轮）
```

之后 DeferredLightPass 是另一个独立 drawcall：全屏三角形，绑 shadow map 当普通纹理 + push constant 带同一份矩阵，PS 里 `shadow_csm.glsl` 查表得阴影因子。

## 8. 易混点速查

| 疑问 | 答案 |
|---|---|
| shadow vert 里没用的 `layout location` 为什么还声明？ | 所有 mesh pass 共用同一份 `mesh_vertex_attributes` 建 input layout；D3D12 要求 input layout 每个 element 必须存在于 VS 输入签名中，否则 PSO 创建失败。真没用的只有 a_Normal / a_UV / a_InstanceColorTint，a_Model0..3 与 a_InstanceParams 都在用（拼矩阵、植被风摆动） |
| shadow_csm.glsl 是 shadow pass 的 shader 吗？ | 不是。它是光照 shader `#include` 的采样库，u_ShadowMap 对应 light pass 绑定的 SRV |
| drawIndexed 的参数里为什么没有数据？ | 参数只有数量（索引数、实例数）。数据全靠 draw 之前绑的状态——看一个 drawcall 画了什么，往前看它绑了什么 |
| VS/PS 执行顺序是先顶点后像素的串行？ | 是流水线阶段顺序，但每阶段内部海量并行：一次画几十万三角形 × 几百万像素，GPU 上万核心同时各跑各的实例。shader 内不能有跨顶点/跨像素逻辑——这正是阴影拆成"先记深度、再查表"两个 pass 的原因 |
| 对号入座有哪几个契约？ | location ↔ 顶点槽格式；set/binding ↔ binding set 条目；push constant layout ↔ CPU 结构体；语义名（TEXCOORD3）↔ VS 输入签名。任何一边单改，轻则花屏重则校验崩溃 |
