# 光照与基于图像的照明（IBL）

本文档描述 dodoe 渲染器的 PBR 直接光照与 IBL 体系：数据烘焙流程、着色器采样方式、CPU/GPU 参数接线，以及两条渲染路径（RenderGraph 主路径 / baseline 参考路径）如何共享这套系统。

## 1. 总览

```
SkyLight cubemap (.docubemap, RGBA32_FLOAT, 6 面)
        │ TextureManager::loadCubemapTexture（加载时一次性烘焙）
        ├──► Prefiltered mip 链（GGX 卷积，mip i ↔ roughness = i / (mipCount-1)）
        │         └── 运行时镜面: textureLod(skybox, R, roughness * maxMip)
        └──► SH9 irradiance 系数（9 × vec4，存于 TextureCubemap）
                  └── 运行时漫反射: Σ coeff[i] * Y_i(N)，零纹理采样

启动时一次性生成
        └──► BRDF LUT（128×128 RGBA32_FLOAT，(NdotV, roughness) → env BRDF scale/bias）
                  └── 运行时: specular = prefiltered * (F * lut.r + lut.g)
```

直接光照（Directional / Point / Spot）为标准 Cook-Torrance：

- GGX 法线分布（α = roughness²）
- Smith-Schlick 几何（k = (roughness+1)²/8）
- Schlick 菲涅尔，F0 = mix(0.04, albedo, metallic)

## 2. 加载时烘焙（CPU，TextureManager）

### 2.1 Prefiltered mip 链

`loadCubemapTexture`（`engine/src/runtime/function/render/texture/texture_manager.cpp`）：

1. 6 面读入 RGBA32_FLOAT，做 face 翻转后上传为 mip 0。
2. mip 数由面尺寸推出：`mip_count` 使 `face_size >> mip_count == 0` 的最小值（如 1024 → 11 级）。
3. mip i（i ≥ 1）由 **mip i-1 双线性采样**做 GGX importance sampling 卷积：
   - roughness = i / (mipCount - 1)，α = roughness²
   - 每纹素 32 个 Hammersley(2,3) 样本，`L = 2(N·H)H − N`，按 NdotL 加权平均
   - 渐进式（从上一级采）会引入轻微二次模糊，属可接受近似；成本约 1-2 s/cubemap（1024 面一次性）

### 2.2 SH9 Irradiance 系数

- 对 mip 0 全部纹素按正交实 SH 基（L0 + L1 + L2，共 9 项）投影，均匀立体角权重 `4π / (6·size²)`。
- 余弦瓣卷积系数 `A = {π, 2π/3 ×3, π/4 ×5}`，并折叠 `A_i/π`（即系数直接是出射漫反射辐亮度，着色器**不再除 π**）。
- 结果以 `Vector4f[9]` 存入 `TextureCubemap::setIrradianceSH`（`texture.h`）。
- fallback cubemap（黑色）SH 自然为零 → 无天光时 IBL 漫反射为黑。

### 2.3 BRDF LUT

`createBrdfLookupTexture` 在 `TextureManager::initialize` 时生成：

- 128×128 RGBA32_FLOAT，横轴 NdotV、纵轴 roughness
- Karis split-sum 积分（512 样本/纹素，几何项用 k=(r+1)²/8 的 Schlick-GGX，与直接光一致）
- 取用：`TextureManager::getBrdfLut()`

## 3. 运行时着色

三个光照片元着色器共享同一套 IBL 代码（`engine/res/shaders/`）：

| 着色器 | 用途 |
|---|---|
| `deferred_light_pass.frag` | 延迟光照 pass（RenderGraph 路径 + baseline 路径共用） |
| `forward_lit_pass.frag` | 前向不透明/透明（bindless） |
| `forward_lit_pass_nobindless.frag` | 前向（传统绑定） |

### 3.1 着色器公式

```glsl
// 漫反射（SH 系数已含 A_i/π，故不再除 π）
vec3 irradiance = evalIrradianceSH(N);          // Σ coeff[i] * Y_i(N)，clamp ≥ 0
vec3 kD = (1 - F) * (1 - metallic);
vec3 ibl_diffuse = kD * irradiance * albedo;

// 镜面
vec3 prefiltered = textureLod(skybox, R, roughness * u_IblParams.z).rgb;
vec2 env_brdf = textureLod(u_BrdfLut, vec2(NdotV, roughness), 0).rg;
vec3 ibl_specular = prefiltered * (F * env_brdf.x + env_brdf.y);

// 强度
float diffuse_strength  = u_IblParams.x;
float specular_strength = u_IblParams.x * u_IblParams.y * pow(1.0 - roughness, 2.0);
color = (ibl_diffuse * diffuse_strength + ibl_specular * specular_strength) * ao;
```

要点：

- **绘制结构（ambient 单独一次）**：光照 pass 采用加法混合的多次全屏 draw。skylight 启用时先画一次 ambient/IBL-only draw（`u_CameraPosition.w = 1`，`u_LightColorIntensity = 0`，直接光贡献为零），之后每盏非 Sky 灯各一次 draw（此时 `u_IblParams.x = 0`，不叠加 IBL）。因此 IBL 永远只计一次，与灯数量无关；场景只留 skylight 也能正确照亮，物体全黑仅出现在既无 skylight 也无灯时。
- **SkyLight intensity 接管了原全局常数**：`u_IblParams.x` 来自场景中启用的 SkyLightComponent.intensity；没有启用的 SkyLight 时为 0，即 IBL 整体关闭。
- **粗糙度抑制镜面**：`pow(1 - roughness, 2)` 保证全粗糙表面（如默认地面）不出现"反射感"，光滑表面在掠射角仍有菲涅尔增强。
- `u_IblParams.y` 为镜面基准强度（当前 0.35，所有 CPU 端填值处一致）。
- `u_IblParams.z` 为 cubemap 的最高 mip 索引（= mipCount-1），着色器不再写死 5。
- 亮部截断 `kIblMaxRadiance = 3.0` 保留，防止太阳盘在全粗糙 mip 中残留亮斑。

### 3.2 UBO 布局（std140，CPU 结构体与 GLSL 成员顺序一一对应）

延迟光照（`DeferredLightPassUBO` / `DeferredLightPushConstants`，缓冲区 320 字节）：

```
  0: light_color_intensity     48: light_view_projection (mat4)
 16: light_position_radius    112: shadow_params
 32: light_direction_type     128: camera_position (w: >0.5 为仅天空调试路径)
                              144: irradiance_sh[9]
                              288: ibl_params (x=intensity, y=0.35, z=maxMip)
```

前向（`OpaquePassUBO` / `LitPassConstantBuffer`，缓冲区 432 字节）：

```
  0: camera_position         128: point_light_colors[4]
 16: directional_...         192: point_light_positions[4]
 32: directional_..._flags   256: light_count_flags (x = 点光数)
 48: dir_light_view_projection   272: irradiance_sh[9]
112: shadow_params               416: ibl_params
```

### 3.3 Pass 绑定槽（set = Pass）

| 槽 | 延迟光照 | 前向 |
|---|---|---|
| INPUT0 (1) | albedo | shadow map |
| INPUT1 (2) | normal | skybox (TextureCube) |
| INPUT2 (3) | position | **BRDF LUT（新增）** |
| INPUT3 (4) | shadow map | — |
| INPUT4 (5) | material | — |
| INPUT5 (6) | skybox (TextureCube) | — |
| INPUT6 (7) | **BRDF LUT（新增）** | — |
| SAMPLER (9) | 共享采样器（linear / clamp，mip 生效） | 同左 |

## 4. CPU 接线点

| 路径 | 文件 | 说明 |
|---|---|---|
| RenderGraph 延迟 | `render_pipeline/passes/render_deferred_light_pass.cpp` | `DeferredLightPushConstants` + 布局 + LUT 绑定；执行时从 `LightSceneInfo::getSkyLightData()` 取 SH/强度/maxMip |
| RenderGraph 前向 | `render_pipeline/passes/render_opaque_pass.cpp`、`render_transparent_pass.cpp` | `LitPassConstantBuffer` 由 `BuildLitPassConstantBuffer` 填充（含 SkyLight 扫描）；`MakeLitPassBindingLayout` / `CreateLitPassBindingSet` 挂 LUT |
| baseline 延迟 | `service/reference/passes/baseline_lighting_pass.cpp` | 与 RenderGraph 延迟路径同构；阴影贴图来自 `BaselineShadowPass`（2048² D32，单方向光），无方向光时绑 fallback |
| baseline 阴影 | `service/reference/passes/baseline_shadow_pass.cpp` | 取第一个启用的 Directional 光，`BuildDirectionalLightViewProjection` 烘焙 shadow depth（ShadowVS/ShadowPS，depth bias 6/slope 1.5），采样 VP 与光照 pass 共用同一矩阵 |

烘焙产物所在：`TextureManager`（`texture_manager.cpp/.h`）——`loadCubemapTexture`（prefilter + SH）、`createBrdfLookupTexture`（LUT）；SH 存于 `TextureCubemap`（`texture.h`）。

材质标量（metallic/roughness/ao）链路：`Material` → `render_scene.cpp`（overrides）→ `MaterialSystem::resolveInstance`（`MaterialInstance` 字段）→ `lit_mesh_processor.cpp` / `baseline_gbuffer_pass.cpp`（`PrimitiveMeshDrawShaderData.material_data`）→ GBuffer material RT (R=metallic, G=roughness, B=ao)。

## 5. 已知限制与后续方向

- Prefilter 为 CPU 渐进式近似（32 样本 + 上一级双线性），高粗糙度下略偏糊；工业做法是 GPU compute GGX 卷积（每 mip 数百样本）。
- SH 只取 L2（9 项），高频率环境反射漫反射偏平滑；可用 3 阶（16 项）或烘焙 irradiance cubemap。
- 无 env probe 系统：全场景单一天光；大世界需按区域插值多组 SH/cubemap。
- 掠射角锐利反射目前只有 IBL 兜底，缺乏 SSR；`skybox_specular` cubemap 含太阳时高粗糙表面仍可能看到亮区（已被 kIblMaxRadiance 截断到 3.0）。
- AO 仅作为整体乘子；可升级为 specular occlusion（如 dHdylorks 近似）。
