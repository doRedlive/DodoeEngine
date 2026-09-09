# 时域抗锯齿(TAA)

本文覆盖引擎的 TAA 实现:相机抖动、Motion Vector Buffer、上一帧深度乒乓、Resolve 着色器算法(YCoCg variance clipping / disocclusion / 自适应混合)、以及 RDG 与 baseline 两条路径的集成方式。

## 1. 总览与数据流

TAA 只在 **Deferred 路径**启用(Forward 使用 MSAA 4x,见 `render_settings.h` 的 `IsTaaEnabled()`):

```text
GBufferPass(Opaque)                      阴影/天空盒/光照/透明(HDR)
  │ 输出 Albedo/Normal/Position/Material/Emissive
  │ + GBufferMotionVector(RG16F, 每对象速度)
  │ + GBufferDepth(D32)
  ▼
TaaDepthCopyPass(Resolve)                GBufferDepth → TaaPrevDepthX(R32F) 拷贝
  ▼
TaaPass(Taa)                             HDR + MotionVector + Position + PrevDepth
  │   → 历史重投影 → disocclusion 检测 → YCoCg variance clipping
  │   → 自适应混合 → 锐化
  ▼
SceneHdrKey 被替换为 history write 目标   → PostProcess(ToneMapping) → Sprite → UI
```

两条实现路径,共享同一套 TAA 着色器(`taa_pass.frag`):

| 路径 | CPU 侧 | 说明 |
|---|---|---|
| RDG(DeferredRenderer) | `TaaFeature` + `TaaDepthCopyPass` + `TaaPass` | 主路径,资源经 RenderGraph import |
| baseline(BaselineRenderer) | `BaselineGBufferPass` + `BaselineTaaPass` | 无 RenderGraph 的参考实现,prev-depth 乒乓内嵌在 TAA pass 中 |

### 执行顺序注意

- `RenderPhase` 实际顺序(render_phase.h):`Opaque → Shadow → Skybox → Lighting → Decals → Transparent → Sprite → Resolve → Taa → PostProcess → UI → ...`
- DeferredRenderer feature 注册顺序:GBuffer → Shadow → Skybox → Lighting → Transparent → **TAA** → PostProcess → Sprite → UI。
- **Sprite 在 TAA 之后**渲染(SPR 混合到 TAA 后的 HDR 上),因此 sprite 像素不参与时域累积;透明网格(TransparentPass)在 TAA **之前**混合进 HDR,参与累积但没有独立速度(见 §7 限制)。

## 2. 相机抖动(render_viewport.cpp)

- `RenderViewport::buildViewFamily` 在 `IsTaaEnabled()` 时为每帧取 **Halton(2,3) 8 相位**序列(相位计数器 `m_jitter_sequence` 持续递增,不随 resize 重置)。
- 抖动以 NDC 偏移直接加在投影矩阵上:`jittered_proj[2][0] += jx; jittered_proj[2][1] += jy;`,即整体在屏幕平面平移 ±1 像素内。
- 当前帧的 `jitter_ndc` 与 **未抖动 VP**(`unjittered_view_projection = proj * view`)写入 `TaaViewExtension`。
- **jitter uv 约定**(历史采样补偿用,两条路径不同,勿混用):
  - RDG:`jitter_uv = (jx * 0.5, -jy * 0.5)`(NdcToUv 的 Y 翻转补偿,taa_feature.cpp);
  - baseline:`jitter_uv = (jx * 0.5, +jy * 0.5)`(配合 `Math::FlipClipSpaceY`,baseline_taa_pass.cpp)。
- 关系式:`jittered_uv = unjittered_uv + jitter_uv`(与 shader 中 `NdcToUv` 的 `0.5 - y*0.5` 映射一致)。

## 3. Motion Vector Buffer(每对象速度)

### 3.1 数据通路

```text
PrimitiveSceneInfo(双缓冲 prev 快照)
  → InstanceSceneData.prev_model          实例顶点缓冲 TEXCOORD9-12
  → lit_pass.vert: v_Motion = curr_uv(jittered) - (prev_uv(unjittered) + prev_jitter_uv)
  → gbuffer_pass*.frag: o_Motion(location 5) → GBufferMotionVector(RG16F)
  → TaaPass: history_uv = v_UV - motion
```

### 3.2 关键结构

| 位置 | 内容 |
|---|---|
| `mesh_draw_types.h` `InstanceSceneData` | `model(64B) + color_tint(16B) + params(16B) + prev_model(64B)`,stride **160B**。所有实例缓冲/顶点布局均以 `sizeof(InstanceSceneData)` 计算,改结构即全局生效 |
| `lit_scene_feature.cpp` `BuildMeshVertexAttributes` | 追加 instanced 属性 TEXCOORD9-12(offset 96..144);shadow/pick/baseline 的布局只声明到 TEXCOORD8,偏移不受影响 |
| `mesh_draw_types.h` `ViewMeshShaderData` | `view_projection + prev_view_projection + prev_jitter_uv`(208B),由 GBufferPass 在 execute 时写入 View CB;prev 值来自 `TaaFrameParamsKey`(经 `GBufferPassParameters` 从 build 传递到 execute) |
| `lit_pass.vert` | 读 `a_PrevModel0..3` 与 View CB 的 `u_PrevViewProjection/u_PrevJitterUV`;`curr_clip.w <= eps` 或 `prev_clip.w <= eps`(背面/跨帧越近面)时速度回退为 0 |

### 3.3 prev_model 双缓冲(`primitive_scene_info.h`)

`PrimitiveSceneInfo` 内部维护 `m_prev_frame_instance_data` 快照:

- `beginMotionFrame()`:帧级代数计数器(`inline static`),每帧由调用方递增一次;
- `advanceMotionFrame()`:对单个可见 primitive 幂等执行——把上一帧快照的 `model` 逐实例填入当前 `prev_model`,然后重新快照。语义保证:
  - 物体持续移动:prev = 上一帧姿态 ✓;
  - **物体移动后静止:快照等于当前姿态 → 速度归零**(不会残留陈旧速度导致静止拖影);
  - 首次出现:无快照 → prev = current → 速度 0。
- 调用点:`LitSceneFeature::setupMeshPassContexts`(RDG)与 `BaselineGBufferPass::setupView`(baseline),均在拷贝 instance data 之前执行。
- 边界情况:多 view family 同帧渲染时,第二个 family 会再次推进代数,其 prev 等于本帧姿态(速度为 0)——仅影响次要视口的物体运动补偿,相机补偿不受影响;foliage 实例集合重排时快照按索引对齐可能错位,产生一帧错误速度,由 variance clamp 兜底。

### 3.4 GBuffer 输出

- 第 6 个 color attachment:`GBufferMotionVector`,`RG16_FLOAT`,clear 0(`gbuffer_scene_feature.cpp` 的 `BuildGBufferDesc`/`MakeGBufferFramebufferInfo`)。
- `gbuffer_pass.frag` / `gbuffer_pass_nobindless.frag` 输出 `o_Motion = vec4(v_Motion, 0, 1)`(location 5);速度在顶点级计算、逐像素插值(标准做法)。
- `SceneTexturesKey.motion_vector` 由 GBufferPass 写入 blackboard,`TaaPass` 消费。

## 4. 上一帧深度乒乓(disocclusion 基础)

- `TaaDepthCopyPass`(phase `Resolve`,在 Taa 之前):全屏拷贝 GBufferDepth → `TaaPrevDepthA/B`(R32_FLOAT)之一,加载 DontCare。
- `TaaFeature` 以与 history 相同的奇偶 flip 管理两块乒乓目标,并发布 `TaaPrevDepthReadKey/TaaPrevDepthWriteKey`;revision 检测并入 TAA 的 reset 条件(resize/重建后历史作废)。
- 拷贝着色器 `taa_depth_copy.frag`(manifest 条目 `TaaDepthCopyPS`)。新增 shader 的完整注册链:manifest → `shader_library.h::getTaaDepthCopyPixelShader` → pass 引用。

## 5. Resolve 算法(taa_pass.frag)

### 5.1 历史采样(三路径,按优先级)

1. **每对象速度**:`history_uv = v_UV - motion`(MV 非零且位置缓冲有效时);
2. **世界坐标重投影**:`history_uv = NdcToUv(prevVP * world) + prev_jitter_uv`(静态几何,精确);
3. **天空盒回退**(位置缓冲为 0):`history_uv = scene_uv + prev_jitter_uv`(仅补偿抖动,无相机重投影)。

任一路径越出屏幕 → 该像素无历史,直接输出当前帧(相当于 blend 权重 0)。

### 5.2 Disocclusion 检测(深度交叉验证)

```text
expected_depth = (prevVP * world).z / .w          当前像素表面在上一帧的期望深度
stored_depth   = min(prev_depth[history_uv ± 2texel 十字])   ±2texel min-dilation 防斜面误判
disocclusion   = |stored - expected| > max(0.005, 0.01 * expected)
```

- 命中(遮挡切换/物体移走露背景/之前可见现在被挡)→ 该像素历史不可信,混合权重直接归 0,一帧内收敛、几乎无拖尾;
- 采样深度带 dilation 的原因:双线性插值深度在斜面上会超出阈值造成误判,取邻域最近深度做保守比较。

### 5.3 YCoCg Variance Clipping(抗闪烁)

```text
3×3 邻域(以未抖动 scene_uv 为中心) → YCoCg 空间的 min/max/μ/σ
box = intersect([min,max], [μ - γσ, μ + γσ]),  γ = 1.5 (u_Tuning.x)
history_clamped = YCoCgToRGB(clamp(history_ycc, box_min, box_max))
```

相比旧版的线性 HDR 空间硬 AABB clamp:高亮 HDR/镜面区域由 σ 软钳制吸收,不再逐帧硬切导致的周期性闪烁;min/max 硬盒仍兜底极端鬼影。

### 5.4 自适应混合权重

```text
alpha = base(0.9)
deviation = clamp(|history_ycc - clamped_ycc| * 6, 0, 1)      历史 deviate 出盒的程度
speed     = clamp(|motion| * resolution / 16, 0, 1)           像素速度
alpha -= alpha * deviation * 0.85
alpha  = min(alpha, mix(alpha, 0.35, speed))
alpha  = mix(alpha, 0, disocclusion)
alpha  = clamp(alpha, 0, base)
```

reset 帧(CPU 下发 `params.z = 1, params.w = 0`)时跳过自适应逻辑,直接输出当前帧。

### 5.5 时域锐化

混合结果对当前帧 4-tap 十字均值做 unsharp:`result += (result - blur4) * u_Tuning.w`(默认 0.2)。发生在 HDR/tonemap 之前,强度需保守。

## 6. 常量与调参

`TaaConstantsData`(192B,RDG 与 baseline 各有一份定义,必须保持一致):

| 字段 | 含义 |
|---|---|
| `prev_view_projection` | 上一帧未抖动 VP |
| `current_view_projection` | 当前帧未抖动 VP(预留给反向重投影扩展) |
| `params.xy / z / w` | 当前帧 jitter uv / reset 标志 / 基础混合权重 |
| `prev_params.xy` | 上一帧 jitter uv |
| `tuning = (1.5, 0.01, 16.0, 0.2)` | variance γ / disocclusion 相对深度阈值 / 速度归一(texels) / 锐化强度 |

历史缓冲:`TaaHistoryA/B`(RGBA16F)乒乓;reset 条件 = 首帧 + 目标 revision 变化(resize 重建,`RenderTargetHandle::getRevision`)。绑定:`SRV1=HDR, SRV2=history, SRV3=position, SRV4=motion, SRV5=prev_depth, sampler9=GlobalSamplers::Screen()`。

## 7. 与 Unity HDRP TAA / UE TSR 的对比(现状)

| 能力 | 本引擎 | Unity/UE |
|---|---|---|
| 相机抖动 + 未抖动重投影 | ✓ 8 相 Halton(2,3) | ✓ |
| 每对象 motion vector | ✓ 静态网格/foliage(蒙皮/骨骼/粒子无) | ✓ 全类型 + dilation |
| Disocclusion 检测 | ✓ 上一帧深度交叉验证 + 即时权重归零 | ✓(velocity/depth) |
| 邻域钳制 | ✓ YCoCg variance clipping(软) | ✓ 同级 |
| 自适应混合 | ✓ 偏离量 + 速度驱动 | ✓(另有更多统计项) |
| 上一帧深度 | ✓ R32F 乒乓拷贝 | ✓ |
| 透明物体独立处理 | ✗(透明在 TAA 前混合,无 MV) | ✓ |
| 时域上采样/超分 | ✗ | ✓(TSR/DLSS/FSR2) |
| 时域降噪集成(SSR/阴影) | ✗ | ✓ |

## 8. 关键文件索引

| 文件 | 职责 |
|---|---|
| `engine/res/shaders/taa_pass.frag` | Resolve 着色器(重投影/disocclusion/variance clip/自适应/锐化) |
| `engine/res/shaders/taa_depth_copy.frag` | 深度拷贝 |
| `engine/res/shaders/lit_pass.vert` | prev_model 输入 + v_Motion 计算(同时服务 GBuffer 与 forward opaque) |
| `engine/res/shaders/lit_gpu_scene.vert` | GPU-driven 路径 v_Motion 输出 0(link 兼容,无速度,回退静态重投影) |
| `render_pipeline/render_feature/taa_feature.cpp` | history + prev-depth 乒乓、TaaFrameParams(跨帧 VP/jitter) |
| `render_pipeline/render_feature/gbuffer_scene_feature.cpp` | GBuffer 6 attachment 定义 |
| `render_pipeline/passes/render_taa_pass.cpp` | TAA pass(绑定/常量) |
| `render_pipeline/passes/render_taa_depth_copy_pass.cpp` | 深度拷贝 pass |
| `render_pipeline/render_graph_import_keys.h` | `TaaFrameParams`/history/prev-depth import key |
| `render_scene/primitive_scene_info.h` | prev_model 帧推进 |
| `render_view/render_viewport.cpp` | Halton 抖动与 TaaViewExtension |
| `service/reference/passes/baseline_taa_pass.cpp` / `baseline_gbuffer_pass.cpp` | baseline 路径对应实现 |

## 9. 已知限制与后续方向

1. **蒙皮/骨骼/粒子无精确速度**(引擎尚无蒙皮管线):接入后需在蒙皮顶点变换处同时计算 prev 姿态速度;
2. **透明物体**:在 TAA 之前混合进 HDR,速度借用其背后不透明几何,快速透明物边缘可能残留;UE 的做法是透明单独走 forward pass 或自写 velocity;
3. **GPU-driven 路径**:`PrimitiveGpuData` 无 prev transform,lit_gpu_scene 输出零速度,该路径下 TAA 质量等同世界坐标重投影;
4. **Sprite/UI 在 TAA 后**,不参与时域累积(当前为有意设计);
5. **多 view family 同帧**时第二视口的物体速度为 0(见 §3.3);
6. 后续可做:TAA 上采样(渲染分辨率 < 输出分辨率)、tonemap 后锐化、历史透明处理、速度 buffer dilation、相机旋转时的天空盒速度补偿。
