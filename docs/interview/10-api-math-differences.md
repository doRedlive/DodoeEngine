# 10 图形 API 的数学与约定差异及统一处理

对应面试题：OpenGL 和 Vulkan 在数学计算上有没有差异，怎么处理的。

## 一句话

> 严格说不是"数学计算"不同，而是**坐标系与约定不同**：Y 方向、深度范围、绕序、viewport 语义、常量缓冲对齐。这些差异在我的引擎里被压在三层统一处理：投影矩阵出口一处翻转（FlipClipSpaceY）、RHI 状态翻译（viewport/frontFace/depthRange）、以及以 SPIR-V 为统一 IR 的构建期/反射期吸收。

## 差异清单与处理（每条都有代码锚点）

### 1. Y 方向：GL 的 NDC Y 向上（原点左下），D3D12/Vulkan 约定 Y 向下（原点左上）

**处理：投影矩阵出口统一翻转，一处全局解决。**

- `Math::FlipClipSpaceY`（math.h:94）：`flip[1][1] = -1.0f; return flip * m;`——negate Y 通道；
- 所有相机产出 VP 时统一应用：游戏相机（camera_system.cpp:60/68，透视与正交都翻）、编辑器相机（EditorCamera.cpp:239/243）、baseline 各 pass 与 UI pass 内部再翻 VP 副本；
- Vulkan 侧由 cutie 的 `VKViewportWithDXCoords`（vulkan-graphics.cpp:528，负高度 + Y 偏移）把 DX 风格 viewport 转回 Vulkan 原生——因此**内容只需要一份 DX 约定**，Vulkan 的翻转在 RHI 层吸收，不会和投影翻转叠加。

> 讲法："我没有让每个后端各自适配，而是规定引擎内部统一使用 DX 约定，在投影出口一次性翻转；Vulkan 的 viewport 差异由 RHI 的负高度转换兜住。这样 shader、UI、拾取射线全链路只有一套坐标心智。"

### 2. 深度范围：GL 传统 [-1,1]，D3D12/Vulkan [0,1]

**事实**：
- 引擎的 `GfxViewport` 统一带 minZ/maxZ（各 pass 设 0..1，如 baseline_shadow_pass.cpp:259）；GL 经 `glDepthRange(viewport.minZ, viewport.maxZ)` 应用（opengl-commandlist.cpp:414），D3D12/Vulkan 是 viewport 原生字段；
- 投影矩阵用的是 GLM 默认（RH_NO，即 GL 约定 [-1,1]）+ 上述 Y 翻转。

**要会讲的知识点（展示深度，主动说清取舍）**：
- 经典解法三选一：① `GLM_FORCE_DEPTH_ZERO_TO_ONE` 让投影直接产 [0,1]；② GL 4.5 的 `glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)`——这正是选 4.5 的理由之一（见 06 面试题 B）；③ 自写 ZO 投影（`perspectiveRH_ZO/orthoRH_ZO`，math.h:89 已封装入口）；
- 工程实践是 **reversed-Z**（near=1、far=0 + Greater 深度测试 + 浮点深度特性）：远平面精度大幅提升，配合 [0,1] 深度最自然；
- 诚实表述："引擎当前深度链路是统一视口 + glDepthRange 的方案，glClipControl 是 GL 侧更标准的收敛点，在后续计划里。"——把已知问题说成已知问题，比被戳穿强。

### 3. 绕序 / 裁剪面：GL 默认 front=CCW，D3D12 默认 CW

**处理**：光栅状态里带 `frontCounterClockwise` 字段，由管线状态统一表达，GL 后端翻译为 `glFrontFace(state.frontCounterClockwise ? GL_CCW : GL_CW)`（opengl-commandlist.cpp:553）。上层永远不感知后端默认值——**默认值差异由 RHI 显式状态消灭**。

### 4. viewport 语义：DX 风格（左上原点，minZ/maxZ）vs GL（左下原点）vs Vulkan（左下原点 + 负高度翻转）

**处理**：引擎内部统一 DX 风格 viewport；GL 直接 `glViewport`（尺寸语义一致，Y 已在投影层处理），Vulkan 在 cutie 内 `VKViewportWithDXCoords` 转换。三后端的 viewport 都从同一个 `GfxViewportState` 来。

### 5. 矩阵布局：column-major / row-major

**处理**：
- CPU 侧统一 GLM（列主序，`operator[]` 取列）；着色器统一 GLSL mat4（列主序语义）；
- **关键点**：SPIR-V 字节码里带 ColMajor/RowMajor decoration，SPIRV-Cross 生成 HLSL 时会按 decoration 产出正确的 row_major 声明——所以 D3D12 路径（GLSL→SPV→HLSL→DXIL）矩阵语义自动一致，不需要手写转置（见 08 构建管线）。
- 顺带的知识点：剔除平面提取时 glm 列主序要**取行**（row3 ± row_i，Gribb-Hartmann），GPU/CPU 两侧实现保持同构（gpu-scene 文档"血泪清单"第 4 条）。

### 6. 常量缓冲对齐：std140/std430 vs cbuffer 对齐规则

**处理与教训（真实踩坑，很加分）**（gpu-scene 文档"血泪清单"）：
- C++ 结构体与 GLSL std430 数组**步长必须一致**：`alignas(16)` 会把非 16 倍数结构体（如 40 字节的 `PrimitiveGpuData`）撑到 48，GPU 侧从 1 号元素起整体错位——**规则：只有尺寸本就是 16 倍数的结构体才允许 alignas(16)**；
- std140 常量缓冲与 C++ struct 逐字段对齐约定（如 `CullingParams`，多个 compute pass 共用同一 buffer，各 pass 覆盖自己布局需要的字段）；
- 材质常量缓冲**不走手工对齐**：按 SPIR-V 反射的变量偏移填充（`MaterialSystem::buildConstantBufferData`），从机制上消灭对齐错误（见 08）。

## 收尾表述（升华）

> "我的原则是：坐标与约定的差异不在业务代码里散落处理，而是收敛到三个点——投影出口（FlipClipSpaceY 一处）、RHI 状态翻译（viewport/frontFace/depthRange）、构建期统一 IR（SPIR-V 反射 + SPIRV-Cross 生成 HLSL）。业务代码、shader、调试工具链全程只见一套 DX 约定。"

## 高频追问

- **"Vulkan 需要翻转 Y 吗？"** → NDC 本身 Y 向上，但交换链/RT 像素坐标 Y 向下且没有 glClipControl 类机制，工业做法就是负 viewport 高度（我的 cutie fork 里就是这么做的）或投影翻转，二选一，不能都做。
- **"picked ray / 屏幕坐标反推怎么保证跨后端一致？"** → EditorCamera::screenToRay 直接用翻转后的 VP 求逆（EditorCamera.cpp:246），因为整条链路只有一套约定。
- **"为什么不用左手系？"** → 资产管线（Assimp 导入）与学习资料多为右手系，引擎内部 RH + DX 朝向约定是常见组合（很多商业引擎同样"右手系 + Y 向下 NDC"）。
- **"半角/全角精度、mediump 之类的差异呢？"** → 桌面三后端都是 f32，无 mobile mediump 问题；若上移动端才是真问题（GLSL 重写器预留了方言扩展点，但未做）。
