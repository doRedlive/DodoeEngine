# GBuffer 流程与 Shader 说明

本文以当前 Deferred Renderer 的实际代码为准，按 Shader 模块解释一帧 GBuffer 的数据如何产生、被保存，以及如何在 Deferred Light Pass 中重新组合成 HDR 光照结果。

## 1. 完整数据流

    RenderSystem::renderFrame
      └─ DeferredRenderer::render
          ├─ setupMeshPassContexts
          ├─ buildMeshDrawCommands
          └─ buildOrderedPasses
              ├─ GBufferPass
              │   ├─ LitVS / LitGpuSceneVS
              │   └─ GBufferPS 或 GBufferNoBindlessPS
              │       └─ 6 张 MRT + D32 深度
              ├─ Shadow / Skybox
              └─ DeferredLightPass
                  ├─ FullscreenVS
                  └─ DeferredLightPS
                      └─ GBuffer + 阴影 + 天空盒 + BRDF LUT → HDR

主入口位于 [RenderSystem::renderFrame](../../../engine/src/runtime/function/render/render_system.cpp:211)，Deferred 路径在 [DeferredRenderer::render](../../../engine/src/runtime/function/render/render_pipeline/deferred_renderer.cpp:96) 中先准备 mesh，再构建 RenderGraph pass。

GBuffer 的附件由 [GBufferSceneFeature::BuildGBufferDesc](../../../engine/src/runtime/function/render/render_pipeline/render_feature/gbuffer_scene_feature.cpp:21) 定义：

| location | 纹理 | 格式 | 内容 |
|---:|---|---|---|
| 0 | GBufferAlbedo | RGBA8_UNORM | 基础颜色，已乘实例 tint |
| 1 | GBufferNormal | RGBA16_FLOAT | 世界空间法线 |
| 2 | GBufferPosition | RGBA32_FLOAT | 世界空间位置 |
| 3 | GBufferMaterial | RGBA8_UNORM | R=metallic，G=roughness，B=AO，A=选中标记 |
| 4 | GBufferEmissive | RGBA16_FLOAT | 自发光颜色 |
| 5 | GBufferMotionVector | RG16_FLOAT | 当前帧与上一帧的 UV 位移 |
| Depth | GBufferDepth | D32 | 深度测试和后续深度相关处理 |

RenderGraph 在 [GBufferPass::build](../../../engine/src/runtime/function/render/render_pipeline/passes/render_gbuffer_pass.cpp:40) 中把这些纹理注册为写入资源，并写入 SceneTexturesKey，之后 DeferredLightPass 从 blackboard 读取它们。

## 2. Shader 接口约定

所有 GBuffer Shader 都包含 [shader_parameter_sets.glsl](../../../engine/res/shaders/common/shader_parameter_sets.glsl)。项目把 descriptor set 分成：

| Set 宏 | 数字 | 用途 |
|---|---:|---|
| DOE_SET_GLOBAL | 0 | 全局时间等 |
| DOE_SET_VIEW | 1 | 当前 View 的矩阵、GPU Transform Buffer |
| DOE_SET_PASS | 2 | DeferredLight 等 pass 独有资源 |
| DOE_SET_MATERIAL | 3 | 材质采样器和传统材质纹理 |
| DOE_SET_PRIMITIVE | 4 | 当前 draw 的材质/纹理索引 |
| DOE_SET_BINDLESS | 5 | 全局 bindless 纹理数组 |

顶点 Shader 到像素 Shader 的接口必须严格匹配：

    location 0: v_Normal        世界空间法线
    location 1: v_UV            网格 UV
    location 2: v_WorldPosition  世界空间坐标
    location 3: v_TexIndex      基础色纹理索引，flat
    location 4: v_ColorTint      实例颜色 tint
    location 5: v_Selected       编辑器选中标记，flat
    location 6: v_CurrClip       当前帧 clip 坐标
    location 7: v_PrevClip       上一帧 clip 坐标

## 3. 模块一：lit_pass.vert / LitVS

文件：[lit_pass.vert](../../../engine/res/shaders/mesh/lit_pass.vert)

Manifest 中 GBufferVS 和 LitVS 都指向该文件：[shader_manifest.json](../../../engine/res/shaders/shader_manifest.json:3)。因此它既服务 GBuffer，也服务前向不透明路径。

### 3.1 顶点输入

    a_Position       // 局部坐标
    a_Normal         // 局部法线
    a_UV             // UV
    a_Model0..3      // 当前实例 model 矩阵
    a_InstanceColorTint
    a_InstanceParams
    a_PrevModel0..3  // 上一帧实例 model 矩阵

这些实例属性由 CPU 侧 InstanceSceneData 提供。LitSceneFeature::BuildMeshVertexAttributes 会把网格顶点流和实例流组合成最终 input layout。

### 3.2 主流程

main() 做五件事：

1. 组装当前帧和上一帧的 model 矩阵；
2. 用 transpose(inverse(mat3(model))) 计算法线矩阵；
3. 对 foliage 应用顶点风摆动；
4. 计算世界坐标、当前 clip 坐标和上一帧 clip 坐标；
5. 把材质索引、tint、选中标记和运动矢量所需数据传给片元阶段。

变换链：

    local_position
      → model
      → world_position
      → u_ViewProjection
      → curr_clip
      → gl_Position

法线使用逆转置矩阵，避免非均匀缩放时法线方向错误。

### 3.3 Foliage 风摆动

applyFoliageWind() 只在 instance_params.w > 0.5 时启用。它使用时间、风相位、随机变化和弯曲强度计算正弦摆动，并通过 clamp(local_position.y, 0, 1) 让底部基本不动、顶部摆动更明显。

### 3.4 运动矢量输入

当前 clip：

    v_CurrClip = u_ViewProjection * world_position;

上一帧 clip：

    prev_world = prev_model * vec4(a_Position, 1.0);
    v_PrevClip = u_PrevViewProjection * prev_world;

上一帧速度数据在 GBuffer fragment Shader 中进一步转成 UV 运动矢量。

## 4. 模块二：lit_gpu_scene.vert / LitGpuSceneVS

文件：[lit_gpu_scene.vert](../../../engine/res/shaders/mesh/lit_gpu_scene.vert)

这是 GPU-driven 绘制路径使用的顶点 Shader。它和 lit_pass.vert 的输出接口保持一致，因此可以复用 GBuffer 像素 Shader。

### 4.1 与 CPU 路径的区别

CPU/传统路径：

    a_Model0..3 → 当前 model 矩阵

GPU-driven 路径：

    a_TransformIndex
      → transforms[a_TransformIndex].local_to_world
      → 当前 model 矩阵

对应的 TransformBuffer 位于 DOE_SET_VIEW / DOE_VIEW_BINDING_TRANSFORMS。

### 4.2 为什么 motion vector 被置零

GPU-driven 结构当前只提供当前 transform，没有上一帧 transform，因此：

    v_CurrClip = vec4(0.0, 0.0, 0.0, 1.0);
    v_PrevClip = vec4(0.0, 0.0, 0.0, 1.0);

结果是 GBuffer fragment Shader 输出零 motion。TAA 后续可以退回基于世界坐标的重投影，但 GPU-driven 路径没有精确的每对象速度。

### 4.3 其它逻辑

它仍然保留 foliage 风摆动、世界空间法线、UV、纹理索引、实例 tint 和选中标记。因此 GPU-driven 主要改变 model 矩阵和 draw 参数的来源，不改变 GBuffer 的像素格式。

## 5. 模块三：gbuffer_pass.frag / GBufferPS

文件：[gbuffer_pass.frag](../../../engine/res/shaders/mesh/gbuffer_pass.frag)

这是 bindless 模式下的 GBuffer 像素 Shader。ShaderLibrary 在 [getGBufferPixelShader](../../../engine/src/runtime/function/render/shader/shader_library.h:33) 中根据 RenderSettings::IsBindlessActive() 选择它。

### 5.1 材质输入

Primitive 常量：

    ivec4 u_DrawData;
    vec4  u_MaterialData;
    vec4  u_EmissiveData;

CPU 侧在 LitMeshProcessor::setupMeshDrawCommand 中填充：

    u_DrawData.x = base color descriptor index
    u_DrawData.y = metallic/roughness/AO descriptor index
    u_DrawData.z = 是否存在 metallic/roughness/AO 纹理
    u_DrawData.w = normal descriptor index，没有则为 -1

    u_MaterialData.xyz = 默认 metallic / roughness / AO
    u_EmissiveData.xyz = 默认 emissive 颜色
    u_EmissiveData.w   = emissive descriptor index + 1，没有则为 0

Bindless 纹理数组是 u_Textures[1024]，纹理和采样器通过 sampler2D(u_Textures[index], u_TextureSampler) 组合。

### 5.2 法线贴图

如果 u_DrawData.w >= 0，先采样法线纹理，并把 [0, 1] 映射到 [-1, 1]。代码没有显式 tangent vertex attribute，而是使用 dFdx/dFdy 对世界坐标求导，重建屏幕空间切线基，再由 perturbNormal() 生成扰动后的世界空间法线。

如果法线纹理是全白 fallback，则跳过扰动，继续使用几何法线。

### 5.3 基础色

    albedo = texture(u_Textures[v_TexIndex], v_UV).rgb;
    albedo *= v_ColorTint.rgb;

v_TexIndex 来自 Primitive 常量，因此同一个 GBuffer PSO 可以服务不同材质，只需更换 descriptor index。

### 5.4 Metallic / Roughness / AO

初始值来自 u_MaterialData：

    metallic  = clamp(value, 0.0, 1.0);
    roughness = clamp(value, 0.04, 1.0);
    ao        = clamp(value, 0.0, 1.0);

如果存在 MR/AO 纹理，按常见通道约定读取：

    R → AO
    G → Roughness
    B → Metallic

最终是“材质常量 × 纹理通道”，不是直接覆盖。

### 5.5 自发光

    emissive = u_EmissiveData.rgb;

有 emissive 纹理时再乘以纹理 RGB，最后写入 o_Emissive。它不在 GBuffer 阶段做光照，而是在 DeferredLightPS 中加回 HDR。

### 5.6 六个 MRT 输出

    o_Albedo   = vec4(albedo, 1.0);
    o_Normal   = vec4(n, 1.0);
    o_Position = vec4(v_WorldPosition, 1.0);
    o_Material = vec4(metallic, roughness, ao, float(v_Selected));
    o_Emissive = vec4(max(emissive, vec3(0.0)), 1.0);
    o_Motion   = vec4(motion, 0.0, 1.0);

这里没有做光照。GBuffer 只保存描述表面所需的数据，真正的光照由 DeferredLightPS 完成。

### 5.7 Motion Vector

ndcToUv() 将 NDC 坐标转换成纹理 UV：

    current_uv  = ndcToUv(curr_clip.xy / curr_clip.w);
    previous_uv = ndcToUv(prev_clip.xy / prev_clip.w) + u_PrevJitterUV.xy;
    motion      = current_uv - previous_uv;

GBufferMotionVector 会被后续 TAA pass 使用。

## 6. 模块四：gbuffer_pass_nobindless.frag / GBufferNoBindlessPS

文件：[gbuffer_pass_nobindless.frag](../../../engine/res/shaders/mesh/gbuffer_pass_nobindless.frag)

它和 bindless 版本的输出完全一致，区别只在纹理取得方式。

Bindless 版本使用 u_Textures[index]，材质只需提供 descriptor index。

Non-bindless 版本在 DOE_SET_MATERIAL 中固定绑定：

    binding 1: sampler
    binding 2: base color texture
    binding 3: metallic roughness texture

当前 non-bindless 版本没有读取 normal texture 和 emissive texture，因此：

    o_Emissive = vec4(0.0);

这不是 GBuffer 布局变化，而是传统绑定变体的功能差异。两种变体都写 6 个 MRT，所以 DeferredLightPS 不需要区分它们。

## 7. 模块五：fullscreen.vert / FullscreenVS

文件：[fullscreen.vert](../../../engine/res/shaders/post/fullscreen.vert)

DeferredLightPass 不绘制模型，而是绘制覆盖全屏的矩形，不需要 vertex buffer：

    pos = NDC[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);

CPU 侧使用 6 个顶点组成两个三角形，同时生成屏幕 UV：

    v_UV = vec2(pos.x * 0.5 + 0.5,
                0.5 - pos.y * 0.5);

所以每一个屏幕像素都会执行一次 DeferredLightPS。

## 8. 模块六：deferred_light_pass.frag / DeferredLightPS

文件：[deferred_light_pass.frag](../../../engine/res/shaders/mesh/deferred_light_pass.frag)

CPU 侧由 [DeferredLightPass::build](../../../engine/src/runtime/function/render/render_pipeline/passes/render_deferred_light_pass.cpp:61) 创建 fullscreen pipeline，并为每个光源调用 draw_fullscreen_light()。

### 8.1 输入资源

    u_Albedo
    u_Normal
    u_Position
    u_ShadowMap
    u_Material
    u_SkyboxTexture
    u_BrdfLut
    u_Emissive

绑定来自 [render_deferred_light_pass.cpp](../../../engine/src/runtime/function/render/render_pipeline/passes/render_deferred_light_pass.cpp:201)：

| GLSL | 内容 |
|---|---|
| INPUT0 | Albedo |
| INPUT1 | Normal |
| INPUT2 | Position |
| INPUT3 | ShadowMap |
| INPUT4 | Material |
| INPUT5 | Skybox cubemap |
| INPUT6 | BRDF LUT |
| INPUT8 | Emissive |
| SAMPLER | 屏幕采样器 |

### 8.2 UBO

DeferredLightPassUBO 包含当前 fullscreen draw 的光源参数：

    u_LightColorIntensity
    u_LightPositionRadius
    u_LightDirectionType
    u_CascadeViewProjections[4]
    u_CascadeSplits
    u_CameraDirection
    u_ShadowParams
    u_CameraPosition
    u_IrradianceSH[9]
    u_IblParams
    u_EmissiveParams

u_LightDirectionType.w 决定光源类型：

    w < 0.5：Directional
    w >= 0.5：Point/Spot 路径

CPU 侧先为天空环境画一次，再为每盏 Directional、Point、Spot 光源各画一次。

### 8.3 片元读取 GBuffer

    albedo   = texture(u_Albedo, v_UV).rgb;
    normal   = texture(u_Normal, v_UV).xyz;
    position = texture(u_Position, v_UV).xyz;
    material = texture(u_Material, v_UV).rgb;

如果法线长度小于 0.1，认为当前像素没有有效几何，直接输出黑色。

### 8.4 直接光照：Cook-Torrance PBR

核心入口是 evaluateDirectPBR(albedo, N, V, L, radiance, metallic, roughness)。

内部使用：

    F0 = mix(0.04, albedo, metallic)
    F  = Fresnel-Schlick
    D  = GGX 法线分布
    G  = Smith-Schlick 几何项

最后：

    specular = D * G * F / (4 * NdotV * NdotL)
    kD       = (1 - F) * (1 - metallic)
    result   = (kD * albedo / PI + specular) * radiance * NdotL

Directional 光使用方向和阴影；Point 光根据距离、半径和 range 计算衰减；Spot 当前也复用了该路径的参数结构。

### 8.5 阴影

computeShadow() 调用 [shadow_csm.glsl](../../../engine/res/shaders/common/shadow_csm.glsl) 中的 CsmComputeDirectionalShadow()：

    世界坐标
      → 根据相机深度选择 cascade
      → cascade_view_projection
      → 阴影图 atlas UV
      → bias / 过滤
      → shadow factor

阴影因子只乘在 Directional light radiance 上，不影响 IBL。

### 8.6 IBL

evaluateIBL() 使用：

1. u_IrradianceSH[9]：球谐函数估计漫反射环境光；
2. u_SkyboxTexture 的预过滤 mip：粗糙度相关的镜面反射；
3. u_BrdfLut：补偿 split-sum BRDF。

关系可概括为：

    irradiance  = SH9(N)
    prefiltered = textureLod(skybox, reflect(-V, N), roughness * maxMip)
    envBRDF     = textureLod(brdfLut, vec2(NdotV, roughness), 0)

AO 最后乘在 IBL 结果上。

### 8.7 一次 DeferredLightPS 的执行顺序

    读取 GBuffer
      ↓
    检查法线是否有效
      ↓
    计算 IBL
      ↓
    加 emissive
      ↓
    如果是 Sky-only draw：直接输出
      ↓
    如果是 Directional：计算 CSM 阴影 + 直接光
      ↓
    否则：计算 Point/Spot 直接光
      ↓
    写入 HDR o_Color

## 9. CPU 侧如何驱动 DeferredLightPS

DeferredLightPass 的 draw 次数不是“每个像素在 Shader 中循环所有灯”，而是：

    一次 fullscreen draw：天空 IBL + emissive
    每个非 Sky 光源一次 fullscreen draw：直接光

这样可以保证 IBL 只累加一次；每盏灯也可以使用自己的 UBO 参数。

最终 HDR 颜色可以抽象成：

    HDR = IBL + Emissive
        + DirectionalLight
        + PointLight_0
        + PointLight_1
        + SpotLight_0
        + ...

## 10. 与 TAA 的连接

GBuffer 阶段生成的 GBufferMotionVector 不进入 DeferredLightPS，而是由后续 TAA pass 使用：

    lit_pass.vert
      → v_CurrClip / v_PrevClip
    gbuffer_pass.frag
      → o_Motion
    GBufferMotionVector
      → TaaPS
      → 历史帧重投影

CPU 侧在 GBufferPass 执行时把当前 ViewProjection、上一帧未抖动 ViewProjection 和上一帧 jitter 写入 View 常量缓冲区，见 [render_gbuffer_pass.cpp](../../../engine/src/runtime/function/render/render_pipeline/passes/render_gbuffer_pass.cpp:108)。

## 11. 调试建议

出现“物体没显示”时：

1. 检查 lit_pass.vert 的 gl_Position 和深度；
2. 检查 GBufferAlbedo 是否有颜色；
3. 检查 GBufferNormal 是否接近零；
4. 检查 GBufferPosition 是否为世界坐标；
5. 检查 DeferredLightPS 是否因法线长度过小直接返回；
6. 检查 u_CameraPosition.w、u_LightDirectionType.w；
7. 最后检查阴影图、天空 cubemap 和 BRDF LUT。

出现“材质颜色错误”时：

1. 检查 u_DrawData.x 是否指向 base color；
2. 检查 bindless descriptor 是否和材质实例对应；
3. 检查 non-bindless 的 binding 2/3；
4. 检查 v_ColorTint 是否把颜色再次乘了一次；
5. 检查 MR/AO 的 R/G/B 通道顺序。

出现“TAA 拖影”时：

1. 检查 lit_pass.vert 的 a_PrevModel0..3；
2. 检查 u_PrevViewProjection 是否为上一帧未抖动矩阵；
3. 检查 u_PrevJitterUV 的 Y 方向约定；
4. 如果走 LitGpuSceneVS，要注意它当前输出的是零 motion。

## 12. 相关文件索引

| 文件 | 职责 |
|---|---|
| [lit_pass.vert](../../../engine/res/shaders/mesh/lit_pass.vert) | CPU/实例化路径顶点变换、法线、motion 输入 |
| [lit_gpu_scene.vert](../../../engine/res/shaders/mesh/lit_gpu_scene.vert) | GPU-driven 顶点路径 |
| [gbuffer_pass.frag](../../../engine/res/shaders/mesh/gbuffer_pass.frag) | Bindless GBuffer 像素输出 |
| [gbuffer_pass_nobindless.frag](../../../engine/res/shaders/mesh/gbuffer_pass_nobindless.frag) | 传统固定材质槽 GBuffer 输出 |
| [fullscreen.vert](../../../engine/res/shaders/post/fullscreen.vert) | 全屏矩形生成 |
| [deferred_light_pass.frag](../../../engine/res/shaders/mesh/deferred_light_pass.frag) | GBuffer 采样、PBR、IBL、阴影、HDR 合成 |
| [shadow_csm.glsl](../../../engine/res/shaders/common/shadow_csm.glsl) | CSM 阴影采样辅助函数 |
| [render_gbuffer_pass.cpp](../../../engine/src/runtime/function/render/render_pipeline/passes/render_gbuffer_pass.cpp) | GBuffer RenderGraph pass 和 draw 提交 |
| [render_deferred_light_pass.cpp](../../../engine/src/runtime/function/render/render_pipeline/passes/render_deferred_light_pass.cpp) | DeferredLight fullscreen draw 和绑定 |
| [shader_library.h](../../../engine/src/runtime/function/render/shader/shader_library.h) | Shader 名称到实际变体的选择 |
