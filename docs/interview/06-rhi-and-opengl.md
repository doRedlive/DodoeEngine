# 06 跨平台 RHI 与 OpenGL 后端

对应简历条目：跨平台 RHI 硬件抽象层。含三道面试高频题：OpenGL 后端难点 / OpenGL 版本选择 / Vulkan 与 OpenGL 的区别。

## 一句话

> 基于 NVRHI（fork 改名 cutie-rhi）二次开发，新增自研 OpenGL 4.5 后端，统一三套图形后端与着色器编码规范；封装 Bindless 描述符表管理，配合能力探测做硬件特性自动降级。

## 分层架构

```text
引擎封装层  GfxContext（device/持久命令列表/交换链/后端）
            GfxBackend（按 API 初始化窗口上下文）
            gfx.h Gfx* Proxy（GfxTexture/Buffer/Framebuffer/BindingSet/Pipeline）
            DrawCommandList（立即/延迟双模式命令录制）
库层        cutie-rhi（NVRHI fork）IDevice / ICommandList / validation 装饰器
后端        cutie_d3d12 / cutie_vk / cutie_gl(自研)
```

- **cutie-rhi**：`engine/external/cutie-rhi/`，上游 NVIDIA NVRHI 含 D3D12/Vulkan/D3D11 + validation；**`src/opengl/` 全目录自研新增**（上游无 GL）。自研/改动源码带 `// do@Redlive` 头注释。
- 上游 Vulkan 路径已改用 **dynamic rendering**（无 RenderPass/VkFramebuffer 对象）。

## 引擎封装层要点

### Proxy + 惰性实体化

`GfxTexture` 等持有 `desc + m_gpu_ready + m_rhi`：desc 先行（实体化前可读尺寸/格式），`initializeGpu` 时才建真实 GPU 对象。两套引用计数：引擎 `Ref<T>` 管 Proxy，cutie `RefCountPtr` 管 GPU 资源；Proxy 销毁不直接销毁 GPU 对象，由 device GC（fence/semaphore 轮询）延迟回收。

### DrawCommandList 双模式

- 立即模式（`GDrawCommandList.setDevice` 后）：借池化命令列表 open→调用→close→execute；
- 延迟模式（渲染管线每帧命令流）：`recordCommand` 追加进命令流（侵入式链表 + 64KB 线性分配器，变长数据内联 memcpy），稍后回放；`append` O(1) 拼接。
- **资源创建按线程分流**：游戏线程走 Request（建 Proxy + 数据内联 + 入队，渲染线程回放时实体化）；渲染线程直接创建同帧可用（见 05）。

### Bindless

`DescriptorTableManager` 全局描述符表：`createDescriptor` 去重、`allocateSlot/releaseDescriptor` 空闲槽回收；纹理加载时分配 slot 并 `writeDescriptorTable`。sprite 全场景一次 `drawIndexed(6, N)` 靠 bindless 索引。

### 能力探测与自动降级

初始化时 `queryFeatureSupport`（`HeapDirectlyIndexed` → bindless、`ComputeQueue` → compute 等）→ `RenderSettings::SetDeviceCapabilities` → `ResolveFeatures` 决定 gpu_driven/bindless 是否实际生效；不满足时 `culling_path` 强制 `CpuOnly`，sprite 走传统绑定路径。

---

# 面试题 A：完善 OpenGL 后端有什么难点，怎么解决的？

> 总起句：GL 后端的本质难点是"**把现代 API 的显式语义翻译回隐式状态机的现实**"，同时不能让上层架构妥协。挑以下五点讲，每点都是"问题 → 解决"。

## 1. 渲染目标模型差异（framebuffer 问题）

**问题**：我们 fork 的 Vulkan 路径已全面改用 dynamic rendering，不需要 RenderPass/Framebuffer 对象，直接对 VkImage 渲染；而 GL 必须为每个附件组合物化一个 FBO。且 GL 的窗口呈现目标（默认帧缓冲）**根本不是纹理**，无法像 VkImage 那样被采样或拷贝。

**解决**：
- 引擎层统一 `GfxFramebuffer` 抽象；GL 后端 `createFramebuffer` 时 attach → `glDrawBuffers` → `glCheckFramebufferStatus` 完整性校验，不完整报错返回空（GL 36055 incomplete 错误的来源与捕获点）；配 `FramebufferCache` 按附件签名复用 + `isGpuReady()` 不满足时重建自愈。
- 默认帧缓冲特殊处理：`createDefaultFramebuffer`（opengl-device.cpp:606）把 `fbo=0` 包成 `owned=false` 的 Framebuffer 挂进 `GfxViewportSurface`；present 直接 `glfwSwapBuffers`。渲染图 `ImportedBackBuffer` 路径在 GL 下硬性断言：backbuffer 只能作为唯一附件、clear 由 pass 自己做——用规则约束抹平"它不是普通纹理"的差异。

## 2. 没有命令缓冲——立即模式

**问题**：GL 调用立即生效，无法"录制一堆命令再提交"，与引擎"命令录制优先"主线冲突。

**解决**：GL 命令列表实现为立即状态机：`open/close` 退化为标志位；`setGraphicsState` 只缓存状态，真正绑定重放在每次 draw 前的 `applyGraphicsBindings`。cutie 的 `ICommandList` 接口语义三后端保持一致，上层无感知。

## 3. 单上下文 vs 双线程

**问题**：GL 上下文同一时刻只能 current 在一个线程；多线程录命令必乱。

**解决**：`OpenGLBackend` 用 `m_context_owner(thread::id) + m_context_mutex` 做显式所有权转移（opengl_backend.cpp:89-107，acquire 时 `glfwGetCurrentContext` 回读校验）；渲染线程首帧 acquire 后常驻，主线程零设备调用；RenderGraph 在 GL 下走 direct_mode 串行录制（细节见 05）。

## 4. 只吃 GLSL，不吃 SPIR-V

**问题**：三后端一套着色器源码的诉求与 GL 只接受 GLSL 冲突。

**解决**：ShaderLibrary 按 manifest 对 GL 发 GLSL 源码；自研 GLSL 重写器（`opengl-shader.cpp` `makeOpenGLSource`）：正则把 `layout(set=N, binding=M)` 改写为 `layout(binding = N*6+M)`（与 C++ 侧绑定公式一致）、`push_constant` 改成隐藏 std140 UBO、分离采样器折叠为 `sampler2D/samplerCube`、`gl_VertexIndex→gl_VertexID`。**一份 Vulkan 风格着色器喂三个后端**。

## 5. 能力缺失的降级

bindless、显式屏障、显式内存都没有 → 能力探测（`queryGLCapabilities`）→ `ResolveFeatures` 自动关掉 bindless/GPU driven；屏障 `commitBarriers` 简化为 `glMemoryBarrier(GL_ALL_BARRIER_BITS)`；内存 Heap 空壳，上传直接 `glBufferSubData/glTexSubImage2D` 无 staging。

**可补充的小细节**（体现动手深度）：纹理创建时保存/恢复 `GL_TEXTURE_BINDING_2D` 防止污染调用方绑定；InputLayout 直接建 VAO 一次配齐（DSA 思路）；push constant 惰性创建隐藏 UBO 再 `glBindBufferBase`；clear 实现每次临时建 FBO。

---

# 面试题 B：用的 OpenGL 什么版本，为什么？

**事实**：创建窗口时请求 **4.5 Core Profile**（window.cpp:24-26）；后端 glad 加载后逐项探测能力，每项都是 `GLAD_GL_VERSION_4_x || 对应 ARB 扩展` 双判定（opengl-device.cpp:144-153），4.5 不可用可按扩展降级。

**四条理由（按重要性）**：

1. **功能上限**：目标是"用 GL 跑通与 D3D12/Vulkan 同一套上层架构"，需要 compute shader、SSBO、texture storage（4.3）、buffer storage（4.4）、DSA + `glBindTextureUnit`（4.5）。4.5 是不追 4.6 前提下功能最全的一档。
2. **DSA 与立即式后端天然契合**：direct state access 让"创建即配置完成"（VAO/纹理参数/FBO attach 一次成型），不需要"先 bind 再改"，减少全局状态机污染——对没有命令缓冲的立即式实现是实打实的简化。
3. **Core Profile 语义干净**：抛弃 legacy 固定管线与兼容层，翻译层不用伺候两套语义。
4. **为什么不追 4.6**：4.6 主要加 SPIR-V 加载与 indirect parameters；SPIR-V 路线与"GLSL 重写器统一着色器源码"冲突，indirect 这块 GL 本来就走降级路径。同时 4.5 桌面驱动覆盖率（Windows + Mesa）足够当一条稳定的兼容/调试后端。

---

# 面试题 C：Vulkan 和 OpenGL 最大的区别？Vulkan 有而 GL 没有的东西怎么处理？

## 最大区别（一句话结论）

> 从"你声明意图、驱动替你做决定"变成"你对一切显式负责"。GL 是隐式全局状态机：同步、内存、布局、调度全在驱动里；Vulkan 把命令录制、管线、屏障、描述符、内存分配、队列全部显式化。这直接决定了引擎形态——命令录制模型在 Vulkan/D3D12 下是真录制回放，到 GL 退化成立即状态机 + 串行。

| 维度 | Vulkan | OpenGL | 对本项目的影响 |
|---|---|---|---|
| 命令模型 | 录制→提交，多线程录制 | 立即生效，单上下文 | RenderGraph 并行录制仅限 D3D12/Vulkan，GL 走 direct_mode |
| 同步 | 显式 barrier/fence/semaphore，子资源粒度 | 粗粒度 `glMemoryBarrier` | 屏障推导保留，GL 落地简化 |
| 着色器 | SPIR-V 离线编译，PSO 可缓存 | GLSL 运行时编译 | GLSL 重写器统一源码方言 |
| 资源绑定 | descriptor set/table，可 bindless | 绑定点 + texture unit | 能力探测决定 bindless 是否生效 |
| 内存 | 显式分配/VMA | driver-managed | GL Heap 空壳，直接上传 |

## "Vulkan 有、GL 没有"的处理清单（得分点，逐条给策略）

| 缺失能力 | 策略 | 做法 |
|---|---|---|
| Bindless 描述符表 | **不硬模拟，走降级** | `queryFeatureSupport(HeapDirectlyIndexed)` 返回 false → 关掉 bindless + GPU driven，上层回落传统 BindingSet；sprite 写了 bindless/traditional 双路径，语义等价 |
| Push constant | **低成本可模拟就模拟** | GLSL 重写为隐藏 std140 UBO（binding=0），惰性创建，draw 前绑定；语义完全对齐，只损失一点性能 |
| 显式屏障/子资源状态 | **接口保留、落地简化** | RenderGraph 照常推导屏障；GL 状态追踪空实现，`commitBarriers` 一律 `glMemoryBarrier(GL_ALL_BARRIER_BITS)`，靠 GL 驱动隐式同步兜底 |
| SPIR-V | **换方言不换管线** | ShaderLibrary 给 GL 发 GLSL 源码，重写器保证三后端同源 |
| compute/transfer 独立队列 | **架构留位、运行时收敛** | `AsyncCompute` 标志与能力位存在，GL 下不分流，主上下文串行 |
| 显式内存管理 | **接受差距** | Heap 空壳、无 staging 直接上传；GL 后端定位是兼容/调试路径，性能路径是 D3D12/Vulkan |

## 收尾考量（升华一句）

> 原则是把后端差异压进 RHI 和能力探测层，上层只见 `Gfx*` 抽象和 `ResolveFeatures` 的结果。对 GL 缺失的能力分三种策略——**能低成本模拟的模拟**（push constant、默认帧缓冲包装），**不能模拟的降级**（bindless、GPU driven），**不值得追平的接受差距**（显式内存、多队列）。每个后端都在自己能力边界内跑同一套上层架构。

## 高频追问

- **"为什么基于 NVRHI 而不是从零写 RHI？"** → D3D12/Vulkan 后端一致性与 validation 层已工业级成熟，精力集中在差异化价值：自研 GL 后端 + 上层架构（RenderGraph/管线/脚本）；同时 fork 内深度改造（dynamic rendering、GLSL 重写）证明吃透而非套壳。
- **"validation 层是干嘛的？"** → 装饰器模式包裹任意真实设备：参数校验、绑定完整性（register 范围重叠/重复绑定）、命令列表状态机（draw 前 setGraphicsState 标志、push constant 先 set 后 draw）、immediate CL 互斥。
- **"GL 下 swapchain 和 Vulkan 有什么不同？"** → GL 无 swapchain 对象：acquire 是 `glfwGetFramebufferSize` 更新尺寸、present 是 `glfwSwapBuffers`、后缓冲就是默认帧缓冲（fbo=0 包装）；Vulkan 则是完整 WSI（surface 格式/present mode 选择、image semaphore/fence、suboptimal 处理），封装在 `GfxViewportSurface` 按 API 分支。
- **"GL 后端 compute 支持吗？"** → 能力探测支持 compute shader/SSBO（4.3+），GpuScene 上传等 compute 路径可走；但 `applyComputeBindings` 尚未完整实现，GPU driven 整体在 GL 下被 ResolveFeatures 关闭——如实说。
