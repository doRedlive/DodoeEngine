# cutie OpenGL 后端完善计划

## 目标与边界

目标是在现有 GLFW OpenGL 4.5 core 窗口中，让 cutie 的 OpenGL 后端形成可验证的最小图形闭环：离屏渲染、纹理/常量绑定、indexed/instanced draw，以及将最终离屏图像 present 到窗口默认 framebuffer。

本计划只覆盖 OpenGL 后端及其必要的 engine 接入；不试图一次实现 bindless、ray tracing、mesh shader、完整 descriptor table 或 Vulkan/DX12 级别的 resource-state 模型。未实现的 API 必须明确报错或返回受支持状态，不能静默产生错误画面。

## 当前状态

cutie OpenGL 后端（`engine/external/cutie-rhi/src/opengl/`）已有纹理、缓冲、sampler、VAO/FBO 名称分配、部分 copy/clear/query 的基础实现，但尚没有完整的 graphics binding、program、资源绑定或输入布局配置。因此当前的 draw call 并不能完成有效绘制。

| 模块 | 现状 | 影响 |
|---|---|---|
| Native texture / buffer handle | `createHandleForNative*` 返回空 | 无法包装外部 GL 对象 |
| Shader / program | `compileGLSL()` 返回 false；PSO 不创建 program | 所有 draw 无 shader program |
| Framebuffer | 只 `glGenFramebuffers`，未附着资源、未填充 `FramebufferInfoEx` | FBO 不完整，PSO cache 的 RT signature 不可靠 |
| Binding layout / set | 仅保存描述 | shader 无法获得 texture、sampler、UBO/SSBO |
| Input layout | 仅创建 VAO | 没有 vertex attribute format/binding/divisor |
| Graphics state | `applyGraphicsBindings` 和各状态函数为空 | 无 FBO、viewport、VAO、资源与固定功能状态 |
| Push constants | 空实现 | 当前 fullscreen / ImGui 等 shader 参数不能传入 |
| Draw | 未调用 graphics binding；索引格式/offset/base vertex 不完整 | 即使补 program 仍会绘制错误 |
| Present | 将离屏 texture 当作 backbuffer，随后仅 `glfwSwapBuffers` | 窗口默认 FBO 未被写入，通常黑屏 |

## 前置约束（P0）

### 0. OpenGL context 归属

所有 GL 调用必须在拥有 current GLFW context 的同一线程执行。当前实现仅在初始化时 `glfwMakeContextCurrent(window)`，因此 OpenGL 不能在未完成线程治理前与 DualThread/RenderGraph worker 并发使用。

第一阶段的基线策略：OpenGL 强制 single-thread / serial RenderGraph，窗口事件、资源创建、command-list 执行、present、资源析构与 `waitForIdle` 均在 context owner thread 执行。

后续若需多线程，必须先引入 `OpenGLContextOwner`：主线程释放 context，render thread 获取并在退出时释放；其他线程只能准备 CPU 数据，不能调用任何 device 或 GLFW/GL API。Debug 构建在每个 GL 入口断言 current context 与 owner thread。

### 跨后端 RHI 对齐原则

OpenGL 不是一套新的资源/参数模型。它必须遵循与 DX11、DX12、Vulkan 相同的 cutie 合约：

- `BindingLayoutItem::slot + arrayElement` 是唯一的运行时 binding slot；`VulkanBindingOffsets` 仅是 HLSL→SPIR-V 的编译期寄存器分段，GLSL 转换时必须移除这些偏移，不能在 command list 再引入 `16/32/48` 等私有区间。
- texture、sampler、UBO、SSBO、image 在 OpenGL 中是独立 namespace，因此都可使用同一个 RHI slot；这等价于 DX 的各类 register namespace 和 Vulkan 的 descriptor type。
- `PushConstants` 仍由 binding layout 的 slot 决定。OpenGL 以一个临时 UBO 模拟它，DX11 以共享 constant buffer 模拟它，DX12/Vulkan 使用原生能力；三者对调用方的布局与更新时机一致。
- GLSL 仅是 OpenGL 的编译输入；SPIR-V/DXIL 对应的 canonical reflection 必须继续用于生成 binding layout，避免 GL 路径丢失反射、得到不同的参数结构。
- 默认 framebuffer 无法包装为可采样/可作为 RHI texture 的资源，因此允许 OpenGL 在 present 时从 RHI 离屏 texture blit 到 FBO 0；这是唯一保留的窗口系统差异，不改变上层 render graph 合约。

### 1. Shader ABI 与能力策略

当前 engine 为 OpenGL 加载 `.spv`。OpenGL 4.5 core 不保证 SPIR-V 加载；必须在 Device 初始化时记录：GL version、`GL_ARB_gl_spirv`、`GL_SHADER_BINARY_FORMAT_SPIR_V` 是否可用，以及所需 entry point 是否已加载。

支持路径二选一，且须在代码和资源构建中固定为一种：

1. **SPIR-V 路径**：要求 `ARB_gl_spirv`（或 GL 4.6），使用 `glCreateShader` → `glShaderBinary(..., GL_SHADER_BINARY_FORMAT_SPIR_V, ...)` → `glSpecializeShader(entryName)` → attach/link。`glProgramBinary` 不能用于装载 SPIR-V。
2. **GLSL fallback**：提供 GL core profile GLSL 源码，使用 `glShaderSource` / `glCompileShader`；可由 SPIRV-Cross 在构建期生成，但必须固定版本、binding 规则和测试产物。

`layout(push_constant)` 不是可直接映射到 OpenGL generic uniform 的稳定 ABI。最小闭环统一采用“保留 UBO binding point 的 push-constant emulation”：shader 的 OpenGL 变体将 push-constant block 改为 std140 UBO；binding layout 中的 `PushConstants` 由 command list 更新/绑定该 UBO。不要依据裸字节数据猜测 `glUniform*` 类型或 location。

任一核心 shader、specialization 或 link 失败必须通过 message callback 报出 shader 名称、stage、info log，并使 pipeline 创建失败；禁止返回 `program == 0` 的可用 PSO。

## 实施计划

### 2. GLContext、错误检查与对象生命周期

**文件**：`opengl-backend.h`、`opengl-device.cpp`、各资源析构文件

- 在 `GLContext` 保存 `supportsSpirv`、所需 API capability 和 context-owner 调试信息。
- 增加统一的 GL error/debug callback 辅助函数；FBO compile/link/attach 等边界必须输出完整诊断。
- `Device` 初始化失败要形成可观察的失败状态，不能构造一个内部未完成的 device handle。
- 所有 GL object 析构均要求 context 仍 current；engine shutdown 顺序固定为：停止渲染 → `waitForIdle` → 释放 cutie 资源/device → 释放 context → 销毁 GLFW window。

**验收**：在 debug callback 开启时，创建/销毁 device 和空 command list 无 GL error；无 context 时的调用被断言捕获。

### 3. Native texture / buffer 包装

**文件**：`opengl-device.cpp`、`opengl-backend.h`、`opengl-texture.cpp`、`opengl-buffer.cpp`

- 当 `objectType == ObjectTypes::Cutie_GL_Device` 时，以传入的 GLuint 创建 wrapper，不调用 `glGen*`。
- Texture 和 Buffer 分别增加 `owned` 标记；只有 backend 创建的对象才在析构中 `glDelete*`。
- wrapper 必须保留并验证 `TextureDesc` / `BufferDesc` 与原生对象 target、dimension、format、size 的兼容性；不支持的 target 返回失败并报告错误。
- `getNativeObject` 仅接受匹配的 object type；不要把 FBO 0 当 texture ID 包装。

默认 framebuffer 是 FBO 0，不是 native texture。若后续需要让 RHI 表达它，应增加非拥有的 default-framebuffer wrapper（例如 `Cutie_GL_DefaultFramebuffer`），而不是复用 `createHandleForNativeTexture`。

**验收**：外部创建的 texture/buffer 可被 cutie 读写；销毁 wrapper 后原生 GLuint 仍有效；错误 object type/desc 明确失败。

### 4. Shader 编译、program 链接与 binding 映射

**文件**：`opengl-shader.cpp`、`opengl-device.cpp`、`opengl-pipeline.cpp`、`opengl-backend.h`

- `Shader` 提供 SPIR-V specialization 或 GLSL compile 的单一实现，并保存 `shaderObject`、entry point、编译日志。
- `createGraphicsPipeline` 验证至少 VS 存在；按 VS/HS/DS/GS/PS 创建、attach、link program，检查 `GL_LINK_STATUS`，成功后删除或释放仅用于 link 的 shader object。
- 对 `BindingLayoutDesc` 的每个 item 建立稳定 GL mapping：texture unit、sampler unit、UBO binding point、SSBO binding point、image unit；运行时一律以 `slot + arrayElement` 为键。多个 descriptor set/register space 在 OpenGL 中需要在 shader 转译期折叠或显式拒绝，不能悄悄改写 RHI slot。
- 通过 program interface query / uniform block query 验证 shader 内 binding 与 layout；不能假设所有资源都有普通 uniform location。
- pipeline 创建时缓存 input layout、program、binding map、固定功能状态与 framebuffer signature。

**验收**：最小 VS+FS pipeline link 成功；故意错误的 shader/link 可获得 info log；Fullscreen VS/Present PS 可创建 program，且 layout 不匹配会失败而非静默渲染。

### 5. Framebuffer 创建、附着与元数据

**文件**：`opengl-device.cpp`、`opengl-framebuffer.cpp`

Framebuffer 的 attachment 在 `createFramebuffer` 时建立，而不是每次 draw 时重新附着。Framebuffer 描述是不可变的；若资源或 mip/layer 改变，应创建/从 cache 获取另一个 FBO。

- 从 `FramebufferDesc` 构造完整 `FramebufferInfoEx`（color/depth format、width、height、array size、sample count）。
- 按 attachment target 和 `TextureSubresourceSet` 选择 API：2D 使用 `glFramebufferTexture2D`，array/3D/cube layer 使用 `glFramebufferTextureLayer`，必要时使用 `glFramebufferTexture`。
- 使用 `GL_COLOR_ATTACHMENTi`、`glDrawBuffers`；无 color attachment 时设置 `GL_NONE` draw/read buffer。
- 深度格式选择 `GL_DEPTH_ATTACHMENT`、`GL_STENCIL_ATTACHMENT` 或 `GL_DEPTH_STENCIL_ATTACHMENT`；支持 `isReadOnly` 的最小语义或明确拒绝不支持的组合。
- 创建后 `glCheckFramebufferStatus`，失败时输出每个 attachment 的资源、mip、layer、format 与 status。
- FBO 本身需要 `owned`/default-FBO 标记，避免删除 FBO 0。

**验收**：2D color、color+depth、array layer、MSAA（如启用）均通过 completeness 检查；FramebufferInfo 与 engine 的 render-target signature 一致。

### 6. Input layout 与缓冲绑定

**文件**：`opengl-device.cpp`、`opengl-inputlayout.cpp`、`opengl-commandlist.cpp`

`glBindVertexBuffer` 不会自动定义 attribute，因此不能只在 draw 前绑定 buffer。

- `createInputLayout` 依据 `VertexAttributeDesc` 创建 VAO attribute：location、format、offset、element stride、buffer index 和 instancing divisor。location 必须来自稳定的 shader location 约定或 program reflection，不能依赖不稳定的 name 顺序。
- draw 前把 `GraphicsState::vertexBuffers` 的 buffer 和 offset 绑定到 VAO 的对应 binding index；对 legacy 路径必须在 VAO 绑定状态下建立 `glVertexAttribPointer`。
- 绑定 index buffer 到 `GL_ELEMENT_ARRAY_BUFFER`；`drawIndexed` 使用 `IndexBufferBinding.format` 和 `offset`，不能固定 `GL_UNSIGNED_INT`。
- 正确处理 `startVertexLocation`、`startIndexLocation`、`startInstanceLocation`、base vertex/base instance 与 capability fallback；不支持的 base instance 需报错或限定为 0。

**验收**：非 indexed triangle、R16/R32 indexed quad、instance buffer quad 均正确；非零 buffer/index offset 与 base vertex 可通过像素测试。

### 7. Binding set 与 push-constant emulation

**文件**：`opengl-bindings.cpp`、`opengl-commandlist.cpp`、`opengl-backend.h`

- `applyGraphicsBindings` 在 `glUseProgram` 后遍历 `GraphicsState::bindings`，按 pipeline binding map 绑定所有最小闭环所需资源。
- 至少实现：`Texture_SRV` + `Sampler`（`glBindTextureUnit`/`glBindSampler`）、`ConstantBuffer`/`VolatileConstantBuffer`（`glBindBufferRange(GL_UNIFORM_BUFFER, ...)`）、structured/raw buffer（`glBindBufferRange(GL_SHADER_STORAGE_BUFFER, ...)`）、UAV image（如 compute 启用）。支持 binding array 和 `arrayElement`。
- sampler uniform 设置为 texture unit；separate texture/sampler 的配对规则必须由 layout mapping 明确定义。
- `setPushConstants` 仅缓存最新字节数据并做大小校验；在 draw 前上传至 pipeline 对应的临时/环形 UBO，按保留 binding point 绑定。数据大小不足或没有 PushConstants layout 时报告错误。
- binding set 的 layout、visibility 和 resource type 不匹配时失败；禁止将资源类型强制转换后继续执行。

**验收**：fullscreen pass 可采样两张 texture 和一个 sampler；Present 的 viewport 参数生效；ImGui/scissor draw 使用 texture 和 push-constant emulation 正确显示。

### 8. Graphics state、draw 与同步

**文件**：`opengl-commandlist.cpp`

`draw` 和 `drawIndexed` 开始前按以下固定顺序执行：验证 state → bind FBO → `glUseProgram` → 应用 binding set/push constants → bind VAO/VB/IB → 应用 viewport/scissor 与固定功能状态 → draw。

#### Framebuffer 与动态状态

- 绑定 state framebuffer；若为空，明确绑定 default FBO 或报错，不能沿用上一个 FBO。
- `ViewportState` 非空时使用正确的 GL API（单 viewport 用 `glViewport`/`glDepthRange`；indexed viewport 使用 `glViewportIndexedf`/`glScissorIndexed`）。
- scissor test 只由 `RasterState::scissorEnable` 控制；不要在 viewport 设置过程中无条件 `glEnable(GL_SCISSOR_TEST)`。

#### Blend / depth-stencil / raster

- Blend：按实际 color attachment 数量调用 indexed blend API，映射 factor/op/color mask，并实现 blend constant 和 alpha-to-coverage。
- Depth/stencil：映射 comparison/stencil op、front/back read/write mask、static/dynamic stencil reference、depth write 与 depth func。
- Raster：映射 cull/front-face/fill、depth bias、multisample。`depthClipEnable == false` 才应启用 `GL_DEPTH_CLAMP`，不能反向。
- OpenGL framebuffer 原点与其他后端不同；viewport/scissor 及 fullscreen shader 的 Y 翻转必须定义唯一约定，并以像素测试验证。

#### Resource states 与 barrier

第一阶段不维护 Vulkan 风格的每 subresource state，但不能把有依赖的 GL memory visibility 静默忽略：

- 图形纹理采样/RT 顺序由同一 context 的 command order 保证。
- image/SSBO 写后读、compute 写后采样等路径根据实际资源访问发出 `glMemoryBarrier`。
- 尚不支持的 UAV/compute barrier 组合返回错误或禁用相应 feature；`commitBarriers()` 不能永久空实现后宣称支持。

**验收**：RenderDoc/GL debug callback 中可观察到正确 program、FBO、VAO、UBO、texture/sampler 与 draw；blend、depth、cull、scissor 的组合像素测试通过。

### 9. Engine OpenGL present

**文件**：`gfx_context.cpp`（以及需要的 OpenGL present helper）

RenderGraph 继续把 `GL Backbuffer` 作为离屏 render-target texture；它不是窗口 backbuffer。Present 后，在 context owner thread：

1. 将离屏 texture attach 到缓存的 read FBO；
2. 绑定 `GL_DRAW_FRAMEBUFFER = 0`；
3. 按 framebuffer extent 用 `glBlitFramebuffer`（或 fullscreen composite）复制到默认 framebuffer；
4. 恢复必要状态并调用 `glfwSwapBuffers`。

处理 resize、0×0 framebuffer、MSAA、sRGB 与缩放：窗口最小化时进入 suspended 状态，不创建 0 尺寸 RT、不执行 draw/present；恢复后在 owner thread 重建离屏 texture/FBO。若 `glBlitFramebuffer` 的 format/sample 限制不满足，改用固定 fullscreen present pipeline。

**验收**：空场景、sprite、UI、ImGui、全链路均能显示；resize、最小化/恢复后无黑屏、旧帧或 GL error。

## 推荐实施顺序

```text
0. 强制 OpenGL single-thread，建立 current-context / debug 基线
1. 固定 shader ABI 与 SPIR-V/GLSL capability 策略
2. program 创建和 link 日志
3. Framebuffer 创建、metadata、completeness
4. Input layout / VAO / VB / IB
5. Binding layout/set 与 push-constant UBO emulation
6. Graphics state、draw/drawIndexed、必要 memory barrier
7. Native object wrapper 与所有权
8. Engine present、resize/minimize
9. 扩展：compute、UAV、descriptor table、bindless、完整 state tracking
```

顺序上 native wrapper 不再置于最前：它不是绘制闭环的前置条件；shader、FBO、VAO 和 binding set 才是。

## 分层验收矩阵

| 阶段 | 最小测试 | 必须观察的结果 |
|---|---|---|
| Context | 初始化/销毁 | GL debug callback 无错误，所有 GL 调用在 owner thread |
| Shader | 常量色 triangle | program link 成功；失败 shader 有 info log |
| FBO | 离屏 clear/readback | FBO complete，format/extent 与 desc 一致 |
| Vertex | indexed + instanced quad | R16/R32 index、offset、divisor 正确 |
| Binding | 采样 fullscreen | texture/sampler/UBO 三者均已绑定 |
| State | blend/depth/cull/scissor 像素测试 | 状态切换无残留 |
| Present | 离屏颜色块 → window | FBO 0 接收到最终图像 |
| 稳定性 | resize、最小化恢复、首次贴图加载、退出 | 无 GL error、无无效 context 调用、无泄漏/双重删除 |

每一步先使用 GL debug callback 和独立小测试定位，再接入 PresentPass、ImGuiPass 等复杂 engine pass；RenderDoc 抓帧用于确认最终状态，而不是替代自动化验收。
