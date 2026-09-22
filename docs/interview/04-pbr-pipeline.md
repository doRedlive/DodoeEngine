# 04 PBR 物理渲染管线

对应简历条目：完整 PBR 物理渲染管线。

## 一句话

> 金属粗糙度工作流：Cook-Torrance 直接光 + Split-Sum IBL（预滤波 cubemap + SH9 辐照度 + BRDF LUT），PCSS 风格软阴影、法线贴图、ACES 色调映射，前向/延迟双渲染路径共享同一套光照代码。

参考文档：`docs/reference/rendering/lighting-ibl.md`（数据烘焙流程与 UBO 布局的权威描述）。

## 直接光照（Cook-Torrance）

三个光照片元着色器共享同一套实现（`engine/res/shaders/`）：
`deferred_light_pass.frag`（延迟）/ `forward_lit_pass.frag`（前向 bindless）/ `forward_lit_pass_nobindless.frag`（前向传统绑定）。

- GGX 法线分布：α = roughness²
- Smith-Schlick 几何：k = (roughness+1)²/8
- Schlick 菲涅尔：F0 = mix(0.04, albedo, metallic)

## IBL 烘焙（加载时一次性 CPU 烘焙，`texture_manager.cpp`）

### 预滤波 mip 链（镜面）

- mip i 由上一级双线性采样做 GGX importance sampling：每纹素 32 个 Hammersley(2,3) 样本，`L = 2(N·H)H − N`，按 NdotL 加权；
- roughness = i / (mipCount − 1)；运行时 `textureLod(skybox, R, roughness * maxMip)`。

### SH9 辐照度（漫反射）

- 对 mip0 全部纹素按正交实 SH 基（L0+L1+L2 共 9 项）投影；
- 余弦瓣系数 `A = {π, 2π/3×3, π/4×5}` 并折叠 `A_i/π`——系数直接是出射漫反射辐亮度，着色器**不再除 π**；
- 运行时 `Σ coeff[i] * Y_i(N)`，**零纹理采样**。

### BRDF LUT（环境镜面响应）

- 128×128 RGBA32_FLOAT，横轴 NdotV、纵轴 roughness，Karis split-sum 积分（512 样本/纹素）；
- 运行时 `specular = prefiltered * (F * lut.r + lut.g)`；
- 启动时 `createBrdfLookupTexture` 生成一次，`TextureManager::getBrdfLut()` 取用。

### IBL 只计一次的技巧

光照 pass 用加法混合的多次全屏 draw：skylight 启用时先画一次 ambient/IBL-only draw（`u_LightColorIntensity=0`），之后每盏灯各一次 draw（此时 `u_IblParams.x=0` 不叠 IBL）。**IBL 与灯数量解耦**，场景只有 skylight 也能正确照亮。

## 阴影

- 方向光 **CSM 级联**（2×2 atlas，`render_shadow_pass.cpp` 每级联独立 viewport，`cascade_mask` 控制提交）；
- `shadow_csm.glsl`：**PCSS 风格软阴影**——`CsmFindBlocker`（Poisson 盘 blocker search）求平均遮挡深度 → 自适应 filter radius → PCF 采样；
- 点光阴影走 geom 立方体路径（`point_light_shadow_pass.geom`）；
- 采样 VP 与光照 pass 共用同一矩阵，depth bias 6 / slope 1.5。

**表述注意**：实现是 blocker search + 自适应 PCF 半径，属 PCSS 简化版；被追问就说"基于 blocker search 的 PCSS 风格软阴影"。

## 色调映射与后处理

- `tone_mapping_pass.frag:11` `TonemapACES`；
- 后处理链：ToneMapping → FXAA（ping-pong）；
- **加分项**：TAA（相机抖动 + motion vector + 上一帧深度 ping-pong + YCoCg variance clipping + disocclusion 检测，见 `docs/reference/rendering/temporal-antialiasing.md`）、MSAA resolve pass。

## 前向 / 延迟双路径

| | DeferredRenderer（默认） | ForwardRenderer |
|---|---|---|
| 不透明 | GBufferPass：6 张 MRT（albedo/normal/position/material/emissive/motion）+ 深度 → DeferredLightPass 合成 HDR | OpaquePass 直接着色输出 HDR |
| 光照代码 | `deferred_light_pass.frag` | `forward_lit_pass.frag`（同一套 IBL 公式） |
| UBO | 320 字节 `DeferredLightPushConstants` | 432 字节 `LitPassConstantBuffer` |

材质标量链路：`Material` → `MaterialSystem::resolveInstance` → processor 写 `PrimitiveMeshDrawShaderData.material_data` → GBuffer material RT（R=metallic, G=roughness, B=ao）。法线贴图在 GBuffer/前向像素着色器里经 TBN 重建。

## 高频追问

- **为什么 SH 存 9 项？** → L2 对漫反射（低频）够用；可扩 3 阶 16 项或烘焙 irradiance cubemap。
- **为什么 CPU 烘焙而不用 GPU compute？** → 一次性成本换运行时零开销与实现简单（1-2s/cubemap）；工业做法是 GPU compute GGX 卷积 + mipmap 链，这是已知的可优化点（诚实承认）。
- **prefilter 渐进式采样有什么问题？** → 从上一级采样引入轻微二次模糊，高粗糙度偏糊；可接受近似。
- **前向/延迟怎么共享光照？** → 同一套 IBL GLSL 函数 + 同一 `LightSceneInfo` 数据源 + 相同的 shadow VP 矩阵。
- **ACES 之前后处理顺序？** → 线性 HDR（含 IBL 截断 kIblMaxRadiance=3.0 防太阳亮斑）→ ACES → LDR FXAA。
- **AO 怎么处理的？** → 目前仅作为整体乘子，可升级 specular occlusion（dHdylorks 近似）——主动说出限制反而加分。
