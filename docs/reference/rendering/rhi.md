# RHI:图形抽象层

RHI(Rendering Hardware Interface)分两层:

- **cutie-rhi**(`engine/external/cutie-rhi/`):第三方库,NVIDIA 开源 NVRHI 的 fork 改名版,提供 D3D12 / Vulkan / D3D11 后端与统一接口;**OpenGL 后端是本仓库自研新增**(`src/opengl/`,上游 NVRHI 不含 GL)。库内部实现详解见 [cutie-rhi.md](cutie-rhi.md)。
- **引擎封装层**(`engine/src/runtime/function/graphics/`):`GfxContext`/`GfxBackend`/`gfx.h` Proxy 资源/`DrawCommandList`,在上层与 cutie 之间提供与线程模型、生命周期管理配合的胶水。

## 1. cutie-rhi 接口体系

### 1.1 核心接口

所有接口继承 `IResource`(`common/resource.h`),引用计数由 `RefCountPtr<T>` 管理,归零自毁。`include/cutie/cutie.h` 是主头文件(约 3900 行),包含全部 Desc/State 结构。

| 接口 | 职责 | 关键方法 |
|---|---|---|
| `IDevice` | 全局资源工厂 + 命令提交 | `createTexture/Buffer/Shader/Framebuffer/BindingSet/GraphicsPipeline/CommandList`、`executeCommandList(s)`、`waitForIdle`、`runGarbageCollection`、`queryFeatureSupport`、`writeDescriptorTable`(bindless) |
| `ICommandList` | GPU 命令录制 | `open/close`、`writeTexture/writeBuffer`、`setGraphicsState/setComputeState`、`draw/drawIndexed/drawIndexedIndirect`、`dispatch`、`setTextureState/setBufferState/commitBarriers`、`beginMarker/endMarker` |
| `ITexture` / `IBuffer` | GPU 资源 | `getDesc()`;句柄类型 `TextureHandle = RefCountPtr<ITexture>` |
| `IFramebuffer` | 渲染目标集合 | `getFramebufferInfo()`(格式签名,用于 PSO 匹配) |
| `IBindingSet` / `IDescriptorTable` | 资源绑定 | 后者是**可变、无追踪**的 bindless 描述符表 |
| `IGraphicsPipeline` | 图形 PSO | `getFramebufferInfo()` |
| `IShader` / `ISampler` / `IInputLayout` | 着色器/采样器/顶点布局 | — |

### 1.2 各后端 DeviceDesc 与创建

每个后端在 `include/cutie/{opengl,vulkan,d3d12}.h` 提供 `DeviceDesc` 与 `createDevice(const DeviceDesc&)`:

| 后端 | DeviceDesc 关键字段 | 说明 |
|---|---|---|
| OpenGL | `messageCallback`、`glLoaderFunc`(GLAD loader,引擎传 `glfwGetProcAddress`)、`glMajorVersion/glMinorVersion`(0=自动) | 构造时 `gladLoadGL` → `queryGLCapabilities()`(记录 `supportsComputeShaders/supportsSSBO/supportsTextureStorage` 等能力)→ 预建一条 `enableImmediateExecution=true` 的立即命令列表 |
| Vulkan | `instance/physicalDevice/device`、graphics/compute/transfer 三队列(+queue index)、instance/device 扩展、`bufferDeviceAddressSupported` | 由 `VulkanBackend` 先创建 VkInstance/Swapchain/逻辑设备再填入 |
| D3D12 | `pDevice`、graphics/compute/copy 三条 `ID3D12CommandQueue`、四个描述符堆大小(RTV=1024/DSV=1024/SRV=16384/Sampler=1024) | 由 `D3D12Backend` 创建 factory/device/queues 后填入 |
| validation | `createValidationLayer(IDevice*)` 装饰器 | 可包裹任意后端,参数校验;引擎在 `enable_validation` 时启用(Vulkan/D3D12) |

### 1.3 OpenGL 后端特点(自研)

- **命令列表是立即状态机**而非录制队列:`open/close` 只是标志位,`setGraphicsState` 仅缓存状态,真正的 GL 调用在每次 `draw/drawIndexed` 内的 `applyGraphicsBindings` 重放绑定。
- 纹理创建(`opengl-device.cpp:182`):`glGenTextures` → 保存/恢复 `GL_TEXTURE_BINDING_2D`(不破坏调用方绑定)→ 设置默认采样参数 → 支持 immutable storage 时用 `glTexStorage2D/3D`。
- Binding Set 创建时只记录 desc,真正绑定推迟到 draw 时按 `registerSpace*6 + slot + arrayElement` 线性映射到 `glBindTextureUnit/glBindBufferRange` 等。
- `createFramebuffer` 末尾执行 `glCheckFramebufferStatus`,非 `GL_FRAMEBUFFER_COMPLETE` 时报错并返回 nullptr(这就是 36055 incomplete 错误的来源)。
- `commitBarriers` 简化为 `glMemoryBarrier(GL_ALL_BARRIER_BITS)`;资源状态追踪接口为空实现。
- `waitForIdle()` = `glFinish()`。
- 着色器只接受 GLSL 源码(SPIR-V 不可用)。

### 1.4 cutie 命令列表与提交模型

Vulkan/D3D12 后端的 `ICommandList` 是真正的录制对象:`open()` → 录制 → `close()` → `IDevice::executeCommandList()` 提交到队列。D3D12 后端自带校验日志(如 instanced draw 的 index buffer 校验、enhanced barrier 布局检查),错误经 `IMessageCallback` 上报。

## 2. 引擎封装层

### 2.1 GfxBackend:窗口/实例层抽象

`backend/gfx_backend.h` 定义基类,按 `RenderBackendApiType`(OpenGL/Vulkan/D3D12,定义于 `render_settings.h:13`)用 `Managed<>` 工厂创建:

| 后端 | initialize 职责 |
|---|---|
| `OpenGLBackend` | `glfwMakeContextCurrent(window)`(持有初始上下文所有权)→ 按 PresentMode 设 `glfwSwapInterval` → `gladLoadGL` → 记录 framebuffer 尺寸 |
| `VulkanBackend` | validation layer 检查 → `VkInstance`(1.3,debug messenger)→ surface(GLFW 或原生 HWND)→ 选物理设备(优先独显)→ 逻辑设备(**硬性要求** dynamicRendering/descriptorIndexing/timelineSemaphore/bufferDeviceAddress)→ swapchain → command pool |
| `D3D12Backend` | DXGI factory(debug)→ enable debug layer → 选适配器(显存最大、跳过软件适配器)→ `D3D12CreateDevice(FL 12.0)` → InfoQueue 回调 → DIRECT/COMPUTE/COPY 三条队列 |

**OpenGL 上下文所有权**(opengl_backend.cpp:50-66)是 GL 后端线程模型的核心:

- 成员 `std::thread::id m_context_owner` + `std::mutex m_context_mutex`。
- `acquireContext()`:加锁;若上下文已被**其他线程**持有则返回 false;否则 `glfwMakeContextCurrent` 并转移 owner。
- `releaseContext()`:仅当前 owner 可释放(`glfwMakeContextCurrent(nullptr)`)。
- 引擎上层经 `GfxContext::acquireOpenGLContext()/releaseOpenGLContext()` 使用;非 GL 后端两者均为 no-op。

### 2.2 GfxContext:RHI 顶层持有者

`gfx_context.h:40`。职责:

- 持有 `GfxDeviceHandle device_`(cutie device,可被 validation 层包裹)、持久命令列表 `cmd_`、主交换链表面 `m_main_surface_`、backend。
- `initialize()` 按 api_type 分派:创建 `GfxBackend` → 组装后端 `DeviceDesc`(错误回调 `RhiMessageCallback`:Error 以上打 `DO_ERROR("RHI::ERROR: ...")`)→ `createDevice` → (可选)validation 层 → 创建主 `GfxViewportSurface` → 创建持久命令列表。
- 初始化完成后查询设备能力(`HeapDirectlyIndexed` → bindless、`ComputeQueue` → compute),写入 `RenderSettings::SetDeviceCapabilities` 并 `ResolveFeatures` 决定 gpu_driven/bindless 是否实际生效。
- **交换链管理**全部转发给 `GfxViewportSurface`:`getSwapchainTextures()`(每张后台缓冲图包成 `GfxTexture` proxy)、`getSwapchainFramebuffer(i)`、`acquireNextSwapchainImage`、`presentSwapchainImage`、`recreateSwapchain`。Surface 内部按 API 分别维护 Vulkan semaphore/fence、D3D12 `IDXGISwapChain4`(3 张后台缓冲)、GL 默认帧缓冲尺寸。
- `createViewportSurface()/destroyViewportSurface()`:辅助视口(编辑器多视口)。
- **`GfxRenderScope`**:thread_local 标记"当前线程允许立即创建 GPU 资源",详见 threading.md。

### 2.3 gfx.h Proxy 资源

引擎层资源统一采用 **"Proxy + 惰性实体化"** 模式,以 `GfxTexture` 为例:

```cpp
class GfxTexture {
    cutie::TextureHandle m_rhi{};      // cutie 引用计数句柄,实体化后非空
    GfxTextureDesc       m_desc{};     // 创建参数,Proxy 阶段即可读取
    String               m_debug_name{};
    bool                 m_gpu_ready{false};

    void initializeGpu(GfxDeviceHandle device) {
        if (!m_gpu_ready) { m_rhi = device->createTexture(m_desc); m_gpu_ready = true; }
    }
};
using GfxTextureHandle = Ref<GfxTexture>;   // 引擎自研侵入式智能指针
```

同一模式的类:`GfxTexture`、`GfxBuffer`、`GfxFramebuffer`(desc 持有纹理 Proxy,`toRHI()` 转换时要求附件已就绪)、`GfxBindingSet`、`GfxGraphicsPipeline`。

关键性质:

| 性质 | 说明 |
|---|---|
| 两套引用计数 | `Ref<T>`(引擎 ControlBlock 内联存储)管理 Proxy;`RefCountPtr` 管理 cutie RHI 资源。Proxy 销毁不直接销毁 GPU 对象,由 device 的 GC(`runGarbageCollection`)延迟回收 |
| desc 先行 | `getWidth()/getFormat()` 等读 `m_desc`,**不依赖 RHI 就绪**——上层逻辑(如 sprite 自然尺寸计算)可在实体化前运行 |
| `isGpuReady()` | 全链路就绪判定:命令执行时守卫、缓存重试、渲染跳过都依赖它 |

### 2.4 DrawCommandList:双模式命令录制

`draw_command_list.h:25`,`class DrawCommandList : public CommandList<GfxCommandList>`。全局单例 `GDrawCommandList`。

**命令流底层**(`runtime/core/container/command_list.h`):

- `Command`:侵入式单链节点 + 类型擦除的 `m_execute/m_destroy` 函数指针(零虚调用)。
- `CommandImpl<T>`(CRTP):固定长命令;`VarCmd<T>`:命令体后紧跟变长数据区(上传数据内联存储,录制时 `memcpy` 拷贝,调用方临时缓冲可立即释放)。
- `LinearAllocator`(默认 64KB 块)分配,`append(CommandList&&)` O(1) 拼接两条流,`execute(TExecutor&)` 沿链回放——TExecutor 即 cutie `ICommandList`。

**双模式**:

| 模式 | 触发条件 | 行为 |
|---|---|---|
| 立即 | `m_immediate==true`(如 `GDrawCommandList.setDevice(GfxContext&)`) | 借命令列表(`acquireCommandList` 池化)→ open → 调用 → close → `device->executeCommandList` → 归还 |
| 延迟 | `m_immediate==false`(渲染管线每帧的 `frame_ctx.command_list`) | `recordCommand<T>()`/`VarCmd::Create` 追加进命令流,稍后 `execute()` 回放 |

**资源创建的特殊规则**(跨线程安全的关键,详见 threading.md):

- `createTexture/createBuffer`:在 `GfxRenderScope` 内立即 `initializeGpu`(渲染线程同帧可用);scope 外录制 `CreateTextureCommand/CreateBufferCommand`(携带 device + proxy + 变长数据),回放时实体化并上传。
- `createFramebuffer/createBindingSet`:scope 内且依赖资源全部 `isGpuReady()` 才立即实体化;否则返回未就绪 Proxy(由缓存重试或命令执行时守卫兜底)。
- `createGraphicsPipeline` 总是立即(shader 与格式签名不依赖未就绪资源)。
- `createSampler/createBindingLayout/createInputLayout/createShader`:直接转发 device(无 Proxy 包装)。

**执行时守卫**:所有延迟命令的 `execute()` 对引用的资源做 `isGpuReady()` 检查,未就绪则跳过并告警——防止跨线程录制顺序差异导致对空 RHI 句柄操作。

**`detachRecordedCommands()`**:在录制锁内用移动构造把整条命令流(含分配器)转出,供渲染线程回放,不阻塞生产者继续录制。

## 3. 错误上报与诊断

- cutie `IMessageCallback` → `GfxContext` 内部 `RhiMessageCallback`(gfx_context.cpp:17):Info/Warning 静默,Error 打 `DO_ERROR("RHI::ERROR: ...")`。
- D3D12Backend 额外挂 `ID3D12InfoQueue1` 回调(`[D3D12]` 前缀日志,`SetBreakOnSeverity(CORRUPTION/ERROR)`)。
- Vulkan validation 通过 debug messenger → `DO_ERROR`。
- 引擎层诊断:`SetGraphicsStateCommand` 执行时对未就绪 framebuffer/binding set 打 `DO_WARN`;drain 延迟资源命令时打 `DO_INFO` 计数。
- RenderGraph 支持 `dumpToJSON`/`dumpToDOT` 导出结构。

## 4. 设计约束速查

给上层代码的硬性规则:

1. **不要在非渲染线程直接调用 `IDevice`**——经 `DrawCommandList`,由 Proxy/延迟命令机制保证在渲染线程实体化。
2. **`GfxTexture::getRHIHandle()` 在 `isGpuReady()` 为 false 时是空句柄**——绑定 desc 中引用资源前必须确认就绪(或依赖缓存与守卫)。
3. 帧内创建的瞬态资源由 RenderGraph transient pool 管理,**不要跨帧持有**。
4. GL 后端下 GPU 提交线程 = 持有上下文的线程;默认双线程模式通过 acquire/release 在主线程与渲染线程间转移上下文(render_settings.h / opengl_backend)。
