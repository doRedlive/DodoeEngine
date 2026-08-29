# cutie-rhi 库内部讲解

> 本文是 `engine/external/cutie-rhi/`(NVIDIA NVRHI 的 fork 改名版,含自研 OpenGL 后端)的**库内部实现**讲解。引擎侧如何封装与使用它见 [rhi.md](rhi.md);命名空间统一为 `cutie`,各后端为 `cutie::vulkan / cutie::d3d12 / cutie::opengl / cutie::validation`。自研/改动部分在源码中普遍带 `// do@Redlive` 头注释。

## 1. 定位与来源

- 上游为 NVIDIA 开源的 NVRHI(D3D11/D3D12/Vulkan 三后端 + validation 层),本仓库 fork 改名 cutie;
- **`src/opengl/` 全目录为自研新增**(上游无 GL 后端),配套 `CUTIE_WITH_OPENGL` 构建选项与 `include/cutie/opengl.h`;
- 上游的 `VK_RenderPass / VK_Framebuffer` ObjectType 已标记 `[[deprecated]]`——Vulkan 路径全面改用 **dynamic rendering**(无 RenderPass 对象)。

## 2. 目录结构与构建

```text
cutie-rhi/
├── include/cutie/
│   ├── cutie.h               # 主头文件(约 3900 行):全部接口 + Desc/State 结构
│   ├── common/resource.h     # IResource / RefCounter / RefCountPtr / ObjectTypes
│   ├── common/               # misc.h / containers.h / resourcebindingmap.h / aftermath.h
│   ├── {vulkan,d3d12,d3d11,opengl,validation}.h   # 各后端 DeviceDesc + createDevice
│   └── utils.h
├── src/common/               # format-info、dxgi-format、state-tracking、misc、utils
├── src/vulkan/               # Vulkan 后端
├── src/d3d12/                # D3D12 后端
├── src/d3d11/                # D3D11 后端(本工程未启用)
├── src/opengl/               # 自研 GL 后端
└── src/validation/           # 校验层(装饰器)
```

**库目标拆分**(CMakeLists.txt:354-525,静态构建):

| 目标 | 内容 |
|---|---|
| `cutie` | 公共代码(src/common)+ **validation 层** |
| `cutie_d3d12` / `cutie_vk` / `cutie_gl` / `cutie_d3d11` | 各自后端源码,互不链接重叠 |

**本工程接入**(engine/external/cutie-rhi.cmake):静态库、不安装;开 VK + GL + VALIDATION + DX12,关 DX11/NVAPI/RTXMU/AFTERMATH;禁用 FetchContent,改用仓库内 vendored 的 DirectX-Headers、glad、glfw 头。对应 build 产物 `cutie.lib / cutie_d3d12.lib / cutie_vk.lib / cutie_gl.lib`。C++17;GL 目标私有链接 glad + `opengl32`。

## 3. 基础设施

### 3.1 IResource 与引用计数(common/resource.h)

- `IResource`(:105):极小接口 `AddRef/Release/GetRefCount` + `getNativeObject(ObjectType)`(返回 union Object,不 AddRef)。
- `RefCounter<T>`(:374):CRTP 基类,内部 `std::atomic<unsigned long> m_refCount = 1`(创建即 1),归零 `delete this`。后端实现类均以 `class Texture : public RefCounter<ITexture>` 继承。
- `RefCountPtr<T>`(:133):"mostly a copy of Microsoft::WRL::ComPtr"——Get/Detach/Attach/Reset 与跨类型转换;`ResourceHandle = RefCountPtr<IResource>`。
- **ObjectTypes 编码**(:33-90):`0x00aabbcc` = GAPI(aa:1=D3D11, 2=D3D12, 3=VK)+ 层(bb:0=native, 1=参考实现)+ 序号(cc)。cutie 扩展 GL 为 GAPI=4:`Cutie_GL_Device=0x00040101`、`Cutie_GL_DefaultFramebuffer=0x00040102`(opengl.h:8)。引擎 `createHandleForNativeTexture` 用它校验原生对象来源。

### 3.2 格式系统

- `Format` 枚举与 `FormatInfo{bytesPerBlock, blockSize, 通道位域, isSRGB}`(cutie.h:200-263);`src/common/format-info.cpp:28` 的静态表按枚举序排列(static_assert 保证),O(1) 查表,所有后端共享。
- **per-backend 解析**:D3D11/12 共用 `dxgi-format.{h,cpp}` 的 `DxgiFormatMapping{resourceFormat, srvFormat, rtvFormat}`——typeless 技巧的来源;Vulkan 在 `vulkan-constants.cpp` 有 `convertFormat`;GL 自研三张 switch 表(`opengl-format.cpp`:internal / base / type)。
- `queryFormatSupport` 返回 `FormatSupport` 位掩码(VK 用 `vkGetPhysicalDeviceFormatProperties`;D3D12 用 `CheckFeatureSupport` 且 srv/rtv 格式不同时查两次;GL 是静态白名单近似)。

### 3.3 状态追踪与屏障生成(src/common/state-tracking.cpp)

核心类 `CommandListResourceStateTracker`(state-tracking.h:87)——**每个 ICommandList 一份,只做记录与屏障生成,不接触 API**:

- 状态存放在挂在 Buffer/Texture 上的 `StateExtension`:纹理支持整图状态或按 subresource(index = mip + slice*mipLevels)的向量;带 `permanentState / enableUavBarriers / firstUavBarrierPlaced` 等标志。
- `requireTextureState / requireBufferState`(:163-334):比较 prior 与 required,生成 `TextureBarrier/BufferBarrier`(记录 stateBefore/After)。优化:
  - UAV 屏障同资源同批次只放一次;
  - buffer 屏障**合并**——已在批内的 buffer 把 stateAfter 按位或(典型:同一 buffer 同时当 VB 和 indirect args);
  - CPU 可见 buffer 与 volatile buffer 跳过转换。
- `setPermanentXxxState` 延迟到 `commandListSubmitted()` 写回;`keepXxxInitialStates` 在 `close()` 时把声明了 keepInitialState 的资源归位。
- **注意**:本层只负责"何时需要什么屏障";真正下发由后端 `commitBarriers()` 完成(见 §5.3 的 lazy drain 语义;GL 后端是例外,见 §6)。

### 3.4 misc / versioning

- `misc.h`:`arraysAreDifferent / arrayDifferenceMask`(≤32 元素数组逐位差异掩码,用于 binding 更新)、`checked_cast`(Debug 下 dynamic_cast + assert,Release 下 static_cast)、`static_vector`(containers.h,GraphicsState 固定上限容器,录制路径零堆分配)。
- `misc.cpp`:`TextureSlice::resolve`(把 -1 尺寸解析为 mip 实际尺寸)、`FramebufferInfoEx` 构造、`ICommandList::setResourceStatesForFramebuffer` 公共实现(color→RenderTarget、depth→DepthRead/Write)。
- `versioning.h`:64 位版本字(id | queue<<60 | submitted 位),供 UploadManager / scratch buffer / volatile CB 的生命周期复用判断。

## 4. IDevice 公共机制

### 4.1 资源创建

- **Buffer**:D3D12 的 CB 对齐 256B;`isVolatile` **完全不建 D3D12 资源**(由 upload 堆子分配承载,见 §5.2);`isVirtual` 只登记 desc;按 cpuAccess 选堆(Default/Readback/Upload)。Vulkan 建 VkBuffer + `VulkanAllocator::allocateBufferMemory`(直通封装,不用 VMA:扫描 memoryTypeBits → `vkAllocateMemory`,支持 dedicated/deviceAddress/export 扩展结构)。
- **Texture**:D3D12 `CreateCommittedResource[3]`(支持 tiled/virtual/shared);Vulkan 建 VkImage + 显存绑定。
- **无自动 mip 生成**:mipLevels 由调用者给定,库只负责 subresource 尺寸解析。
- **虚拟/tiled 资源**:`getTextureMemoryRequirements + bindTextureMemory` → D3D12 `CreatePlacedResource2`,Vulkan `bindImageMemory2`。

### 4.2 内存堆 IMemoryHeap

`HeapDesc{capacity, HeapType{DeviceLocal/Upload/Readback}}`:

- **D3D12**:真正的 `ID3D12Heap`(Tier1 有限制);普通资源走 committed,placed 仅用于 virtual/tiled 绑定。
- **Vulkan**:一块 `VkDeviceMemory`(可挂 dedicated/deviceAddress/export)。
- **GL**:空壳,capacity=0,全部接口返回 false——**GL 无显式内存管理**。

### 4.3 垃圾回收:引用存活 + 提交实例号轮询

两后端同模型,不是延迟 delete,而是"持有引用直到 GPU 完成":

- **D3D12**:Queue 持 fence,每次 ExecuteCommandLists 后 `Signal()` 递增 `lastSubmittedInstance`;`CommandListLifetimeTracker::runGarbageCollection` 清理 `submittedInstance <= completedInstance` 的命令列表实例(其 shared_ptr 持有 allocator/commandList,靠析构实现延迟释放)。
- **Vulkan**:**timeline semaphore**(`trackingSemaphore`),每次 submit 以 submissionID 为信号值;`getSemaphoreCounterValue` 读完成号;完成的 TrackedCommandBuffer 清空 `referencedResources`(该 vector 对所有被引用资源持 RefCountPtr,天然延迟释放)并归还命令缓冲池。
- **GL**:空实现(GC 无意义,对象销毁即时)。

引擎侧的 `device->runGarbageCollection()` 每帧调用即驱动上述轮询;`waitForIdle` = fence 等待(D3D12)/semaphore 等待(VK)/`glFinish`(GL)。

### 4.4 Upload:staging 与 UploadManager

- **UploadManager**(vk/d3d12 各一份,同构):64KB chunk 大缓冲 + bump-pointer 子分配;chunk 放不下→退役→从池里找已完成且够大的复用→否则新分配;submit 时标记 submitted 版本号。
- **writeBuffer**:D3D12 memcpy 进持久映射的 upload chunk + `CopyBufferRegion`;**volatile CB 特殊**——不拷贝资源,直接记录 upload chunk 的 GPU VA,draw 前重绑(见 §5.2);Vulkan ≤64KB 且 4 字节对齐时走 `vkCmdUpdateBuffer`(需先 endRenderPass),否则 upload + copy。**GL 直接 `glBufferSubData`,无 staging**。
- **writeTexture**:VK `vkCmdCopyBufferToImage` / D3D12 `CopyTextureRegion`(footprint 由 `GetCopyableFootprints` 计算)。**GL 直接 `glTexSubImage2D/3D`**(只支持 2D/3D,忽略 rowPitch)。
- **StagingTexture(读回)**:VK/D3D12 是真正的 staging 资源;GL 用普通 buffer 模拟,rowPitch 固定 `width*4`(粗糙近似)。

### 4.5 Feature 枚举(cutie.h:3036,25 项)

重要的:`ComputeQueue`、`CopyQueue`、`HeapDirectlyIndexed`(bindless)、`RayTracing*`、`Meshlets`、`VariableRateShading`、`VirtualResources`、`EnhancedBarriers`(仅 DX12)、`ConstantBufferRanges`。后端判定:Vulkan 按扩展位(`HeapDirectlyIndexed` ← `EXT_mutable_descriptor_type`);D3D12 用 `CheckFeatureSupport(OPTIONS1/5/6/7/12)`(`HeapDirectlyIndexed` 需 Tier3 + SM6.6);**GL 仅 `ConstantBufferRanges` 恒 true + `ComputeQueue`=supportsComputeShaders,其余 false**。

## 5. ICommandList 内部

### 5.1 命令缓冲生命周期

| 后端 | 机制 |
|---|---|
| Vulkan | 每个在飞 `TrackedCommandBuffer` 一个**独立 VkCommandPool**(transient)+ primary cmd buffer;`open()` 以 `eOneTimeSubmit` begin;submit 后进 lifetime tracker,完成归还池 |
| D3D12 | CommandList 维护 `InternalCommandList` 池(allocator + ID3D12GraphicsCommandList);`open()` 时队首若已完成则 Reset 复用,否则新建;`executed()` 包装成 instance 进 tracker |
| GL | 无命令缓冲,见 §6 |

### 5.2 资源绑定

- **Vulkan**:`BindingLayout::bake` 把 BindingLayoutDesc 转 `vk::DescriptorSetLayoutBinding`(registerSpace → descriptor set 编号);**每个 BindingSet 独享一个 VkDescriptorPool(maxSets=1)**——简单但开销偏高;bindless 走 `EXT_mutable_descriptor_type`;`bindBindingSets` 按 pipeline 的槽位映射 `bindDescriptorSets`。
- **D3D12**:`buildRootSignature`(d3d12-resource-bindings.cpp:663)拼接各 layout 的 root parameter(CBV/SRV/UAV descriptor、volatile CB root descriptor、push constants 32bit constants、descriptor table);**bindless 不走 root parameter**,而是 root signature flag `CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED`(配 SM6.6 `ResourceDescriptorHeap`)。按 (layouts, allowInputLayout) hash 缓存 root signature。
- **D3D12 descriptor heap**:4 个全局堆(RTV/DSV 非 shader-visible;CBV_SRV_UAV/Sampler shader-visible);`StaticDescriptorHeap` 线性分配 + 2 的幂扩容 + `CopyDescriptorsSimple` 搬迁;堆指针变化时 `SetDescriptorHeaps`。
- **volatile constant buffers**(D3D12):不建资源;writeBuffer 数据落 upload 堆,**每次 draw 前** `updateGraphicsVolatileBuffers` 把变化的 GPU VA 以 root CBV 重绑——"CPU 每帧写、零屏障"的常量更新。Vulkan 同概念(timeline 版本号判断是否重绑)。

### 5.3 公共状态缓存与 lazy drain 屏障

三个后端都缓存 `m_CurrentGraphicsState/m_CurrentComputeState` 并 diff:**barriers、pipeline/PSO、framebuffer/renderpass、bindings、viewport/scissor、stencil ref、blend factor、IB/VB**——只录制 diff 出变化的部分。

**屏障提交时机统一为 lazy drain**:`requireXxxState` 只入队;`setGraphicsState/setComputeState`、copy/clear/write 等入口各自调 `commitBarriers()`(若有活动 renderpass 会先结束),`close()` 时最后 `keepInitialStates + commitBarriers`。落地方式:

- Vulkan:`commitBarriersInternal` 转 **sync2** `vk::ImageMemoryBarrier2/BufferMemoryBarrier2` → `pipelineBarrier2`(state 映射在 vulkan-constants.cpp;depth/stencil 按 FormatInfo 拼 aspectMask);
- D3D12:enhanced barriers 走 `commandList7->Barrier`,否则 legacy `RESOURCE_BARRIER`(TRANSITION/UAV)。

**`enableImmediateExecution`**(cutie.h:3115):语义是"录制即执行"。D3D11 映射 immediate context(两 immediate CL 不能同时 open,validation 层原子计数检查);对 D3D12/Vulkan 实际不影响延迟模型;**GL 后端强制为 true**——见 §6。

### 5.4 其他

- Vulkan viewport 做 DX→VK 的 Y 翻转(`VKViewportWithDXCoords`)——引擎上层无需关心坐标约定差异。
- D3D12 预建 4 个 CommandSignature(draw/draw_indexed/dispatch/dispatch_mesh)供 `ExecuteIndirect`。
- 跨队列同步:Vulkan timeline semaphore 的 wait/signal 接口;D3D12 各 queue 独立 fence。

## 6. OpenGL 后端内部(自研,重点)

### 6.1 文件职责

| 文件 | 职责 |
|---|---|
| `opengl-backend.h`(593 行) | 全部类定义:`GLContext`、各资源类、`GLBindingMapping`、`CommandList`、`Device` |
| `opengl-device.cpp` | 能力查询、资源创建(Texture/Buffer/Shader/Framebuffer/Pipeline/BindingSet)、feature/format 查询 |
| `opengl-commandlist.cpp` | 即时式命令列表:状态应用、draw/dispatch、拷贝/清屏、push constants、barrier |
| `opengl-shader.cpp` | GLSL 编译 + "Vulkan 风格 GLSL" 重写(见 §6.4) |
| `opengl-format.cpp` | Format → GL internal/base/type 三张转换表 |
| 其余(opengl-buffer/texture/framebuffer/pipeline/sampler/inputlayout/query/heap/bindings.cpp) | 极小的构造/析构/getNativeObject(析构 glDeleteXxx;`owned=false` 表示包装外部原生对象) |

### 6.2 GLContext 与资源映射

`GLContext` 聚合能力标志(`supportsComputeShaders/supportsSSBO/supportsTextureStorage/supportsSpirv(硬编码 false)/...`)+ 上限值,`queryGLCapabilities` 用 `GLAD_GL_VERSION_4_x || GLAD_GL_ARB_xxx` 判定。错误统一走 `GLContext::error/warning/info` → IMessageCallback。

C++ 类 → GL 对象:

| 类 | 持有 |
|---|---|
| `Buffer` | `GLuint buffer` + `GLenum target`(创建时按用途选 ARRAY/ELEMENT/UNIFORM/SSBO/INDIRECT_BUFFER)+ owned |
| `Texture` | `GLuint texture` + `target` + 预解析的 internalFormat/format/type 三元组 |
| `StagingTexture` | **无 GL 纹理**,一个普通 Buffer 模拟 |
| `Shader` | `GLuint shaderObject` + GLSL 源码 |
| `InputLayout` | `GLuint vao`(DSA 在创建时配置全部顶点属性) |
| `Framebuffer` | `GLuint fbo`;创建时 attach + `glDrawBuffers` + `glCheckFramebufferStatus`,不完整即报错返回 null |
| `GraphicsPipeline` | `GLuint program` + bindingMap + **快照 blend/depth/raster 状态**(GL 全局状态机,draw 前重放) |
| `BindingLayout/BindingSet` | 只存 desc,**无 GL 对象** |
| `EventQuery/TimerQuery` | `GLsync fence` / GL_TIMESTAMP counter |

### 6.3 立即式命令列表

GL 无命令缓冲概念:`createCommandList` 强制 `enableImmediateExecution=true`,`executeCommandLists` 是空操作,`waitForIdle` = `glFinish`。因此 **open() 之后所有录制调用立即生效于 GL 上下文**;`open/close` 退化为"状态缓存清空/屏障提交点";`setGraphicsState` 仅缓存状态,真正应用发生在**每次 draw 前的 `applyGraphicsBindings`**。这直接决定了引擎的线程模型:GL 后端必须保证"调用命令列表的线程持有上下文"。

### 6.4 绑定模型与 GLSL 重写

- **绑定公式**(`opengl-commandlist.cpp:427`):`glBinding = registerSpace * 6 + slot + arrayElement`——每个 registerSpace 占 6 个绑定点带宽。落地:`glBindTextureUnit / glBindSampler / glBindBufferRange(GL_UNIFORM_BUFFER|GL_SHADER_STORAGE_BUFFER) / glBindImageTexture`。
- **GLSL 重写**(`opengl-shader.cpp` `makeOpenGLSource`):把 `layout(set=N, binding=M)` 正则改写为 `layout(binding = N*6+M)`(与 C++ 侧公式一致);`layout(push_constant)` 改为 `layout(std140, binding=0)`(隐藏 UBO,`setPushConstants` 惰性创建 `m_PushConstantBuffer` 并绑定);**把分离采样器模型折叠为 `sampler2D/samplerCube`**、`gl_VertexIndex→gl_VertexID`、`texture2D()→texture()`。效果:**同一份 Vulkan 风格 GLSL 可同时喂 GL 后端**(这也是引擎 ShaderLibrary 对 GL 发源码而非 SPIR-V 的原因)。
- **push constants**:数据缓存于 `m_PushConstantData`,applyBindings 时惰性创建隐藏 UBO 并 `glBindBufferBase`。

### 6.5 局限性清单

| 局限 | 详情 |
|---|---|
| 不支持 SPIR-V | `supportsSpirv=false`,createShader 检查 magic 并报错,只接受 GLSL 源码 |
| 屏障极简 | 状态追踪接口空实现;`commitBarriers()` 一律 `glMemoryBarrier(GL_ALL_BARRIER_BITS)`;无 subresource 状态 |
| 无显式内存管理 | Heap 空壳;上传直接 `glBufferSubData / glTexSubImage`,无 staging |
| StagingTexture 近似 | 普通 buffer,rowPitch 固定 width*4;`copyTexture/resolveTexture/clearTextureUInt` 空实现 |
| Compute 待完善 | `applyComputeBindings` 为空 |
| 无 RayTracing/Meshlet/SamplerFeedback | Feature 仅 ConstantBufferRanges / ComputeQueue |
| clear 每次临时建 FBO | clearTextureFloat/clearDepthStencil 的实现方式 |

## 7. Validation 层(src/validation/)

装饰器模式:`DeviceWrapper : RefCounter<IDevice>` 包住真实设备,`CommandListWrapper` 包住命令列表;每个方法先校验(失败报 IMessageCallback 并中止),通过则转发 underlying 对象。编译进公共 `cutie` 目标,经 `createValidationLayer(IDevice*)` 在任意真实设备外包一层。

校验内容四类:

1. **设备参数**:desc 合法性、volatile buffer 规则(仅 CB 可 volatile 且不能 virtual)、virtual resources 未启用却使用、renderState/Framebuffer 附件一致性;
2. **绑定完整性**:`BindingLocation` + `BindingSummary` 做同一 pipeline 内 **register 范围重叠检测**、重复绑定检测、set 与 layout 匹配(`validateBindingSetsAgainstLayouts`);
3. **命令列表状态机**:`{INITIAL, OPEN, CLOSED}` 状态 + "draw 前必须 setGraphicsState" 标志(引擎日志里 `Graphics state is not set before a drawIndexed call` 即出自这里)、push constants 字节数必须与 pipeline layout 一致且先 set 后 draw、indirect 参数 buffer 存在性;
4. **immediate CL 互斥**:原子计数保证同时只有一个 immediate command list 打开。

## 8. 与引擎层的衔接速查

| 引擎概念 | cutie-rhi 对应 |
|---|---|
| `GfxContext::device_` | `cutie::DeviceHandle`(可被 validation 包裹) |
| `Gfx*` Proxy(`m_gpu_ready`) | cutie `TextureHandle` 等 RefCountPtr 资源 |
| `DrawCommandList` 命令流 | 回放目标 = cutie `ICommandList`(`open→record→close→executeCommandList`) |
| `commitBarriers` 语义 | lazy drain(见 §5.3)——引擎在 setTextureState 后显式调用即"此刻需要屏障生效" |
| `GfxViewportSurface` 交换链 | 库不拥有交换链;D3D12/Vulkan 的 swapchain 由引擎 surface 层驱动,库只提供资源与提交 |
| `RenderSettings::SetDeviceCapabilities` | `queryFeatureSupport(HeapDirectlyIndexed / ComputeQueue)` |
