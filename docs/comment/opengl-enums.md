# Cutie RHI 枚举与 OpenGL 对照

本页解释 `cutie.h` 中驱动资源创建、资源绑定和绘制状态的枚举。它们是 RHI 的**意图描述**：OpenGL 后端把已支持的意图转换为 `GLenum`、OpenGL 状态或原生对象；不能自然映射的项目会被忽略、简化，或由能力查询拒绝。

> 不要把 RHI 枚举和值相同的 OpenGL 枚举混为一谈。`Format::RGBA8_UNORM`、`PrimitiveType::TriangleList` 是跨后端语义；`GL_RGBA8`、`GL_TRIANGLES` 才是 OpenGL 参数值。

## 1. 后端与资源基础

| 枚举 | 值 | 含义与 OpenGL 现状 |
|---|---|---|
| `GraphicsAPI` | `D3D11`、`D3D12`、`VULKAN`、`OPENGL` | 设备实际使用的图形 API。`opengl::Device::getGraphicsAPI()` 返回 `OPENGL`。 |
| `HeapType` | `DeviceLocal` | 优先由 GPU 使用的显存。 |
|  | `Upload` | 适合 CPU 写、GPU 读的上传内存。 |
|  | `Readback` | 适合 GPU 写、CPU 读的回读内存。 |
| `CpuAccessMode` | `None`、`Read`、`Write` | 资源是否允许 CPU 访问。在当前 OpenGL 缓冲创建路径中，它还影响 `GL_STATIC_DRAW`、`GL_STREAM_READ`、`GL_STREAM_DRAW` 的 usage 提示。 |

`HeapType` 是显式内存 API 需要的抽象；当前 OpenGL 后端的 heap/bind-memory 路径尚未提供真正的别名内存分配，因此不要把它当作 OpenGL 内存池功能。

## 2. `TextureDimension`：纹理到底是什么形状

| 值 | RHI 含义 | 标准 OpenGL 目标 | 当前后端注意点 |
|---|---|---|---|
| `Unknown` | 未指定形状 | 无 | 不应创建实际纹理。 |
| `Texture1D` | 一维像素线 | `GL_TEXTURE_1D` | 目标选择支持；存储路径仍应单独核对。 |
| `Texture1DArray` | 多条同规格 1D 图 | `GL_TEXTURE_1D_ARRAY` | 枚举存在，但目标选择逻辑未直接匹配该独立值。 |
| `Texture2D` | 宽 × 高图像 | `GL_TEXTURE_2D` | 常规路径；`arraySize > 1` 时目标选择为 2D array。 |
| `Texture2DArray` | 多张同规格 2D 图 | `GL_TEXTURE_2D_ARRAY` | 有 storage 分支，但目标选择逻辑需与独立枚举约定统一。 |
| `TextureCube` | 六个方向面组成的立方体纹理 | `GL_TEXTURE_CUBE_MAP` | 常规路径；上传按 `POSITIVE_X + face` 选择面。 |
| `TextureCubeArray` | 多个 cubemap | `GL_TEXTURE_CUBE_MAP_ARRAY` | 需要统一独立枚举与 `arraySize` 的表达方式。 |
| `Texture2DMS` | 每像素多个采样点的 2D 图 | `GL_TEXTURE_2D_MULTISAMPLE` | 用专用 multisample storage。 |
| `Texture2DMSArray` | 多层 MSAA 2D 图 | `GL_TEXTURE_2D_MULTISAMPLE_ARRAY` | 存储分支与参数语义需要启用前核对。 |
| `Texture3D` | 宽 × 高 × 深的连续体积 | `GL_TEXTURE_3D` | 通过 `glTexStorage3D` 与 `glTexSubImage3D` 处理。 |

`Texture2DArray` 与 `Texture3D` 的第三维看起来相似，但含义不同：array 的 layer 是离散编号，层间不插值；3D 的 Z 是连续坐标，可参与过滤。

## 3. `Format`：像素或顶点元素如何解释

命名规则：`R/RG/RGB/RGBA` 是通道集合，数字是每通道位数，后缀定义数值解释方式。

| 后缀 | 含义 | 典型用途 |
|---|---|---|
| `UINT` / `SINT` | 无符号/有符号整数；不归一化 | 索引、ID、整数属性、整数 image |
| `UNORM` / `SNORM` | 整数存储，读取时映射到 `[0,1]` 或 `[-1,1]` | 普通颜色、压缩法线 |
| `FLOAT` | 浮点数 | HDR、位置、法线、计算数据 |
| `SRGB` | 采样读取时进行 sRGB 到线性空间变换 | 色彩纹理；不应用于法线、粗糙度等数据纹理 |

| 格式组 | 全部值 | 常见 OpenGL 内部格式方向 |
|---|---|---|
| 8 位单/双通道 | `R8_UINT`、`R8_SINT`、`R8_UNORM`、`R8_SNORM`；`RG8_UINT`、`RG8_SINT`、`RG8_UNORM`、`RG8_SNORM` | `GL_R8*`、`GL_RG8*` |
| 16 位单/双通道 | `R16_UINT`、`R16_SINT`、`R16_UNORM`、`R16_SNORM`、`R16_FLOAT`；`RG16_UINT`、`RG16_SINT`、`RG16_UNORM`、`RG16_SNORM`、`RG16_FLOAT` | `GL_R16*`、`GL_RG16*` |
| 8 位四通道与兼容布局 | `RGBA8_UINT`、`RGBA8_SINT`、`RGBA8_UNORM`、`RGBA8_SNORM`、`BGRA8_UNORM`、`BGRX8_UNORM`、`SRGBA8_UNORM`、`SBGRA8_UNORM`、`SBGRX8_UNORM` | 多数为 `GL_RGBA8*`；sRGB 为 `GL_SRGB8_ALPHA8` |
| 16 位四通道 | `RGBA16_UINT`、`RGBA16_SINT`、`RGBA16_UNORM`、`RGBA16_SNORM`、`RGBA16_FLOAT` | `GL_RGBA16*` |
| 32 位标量/向量 | `R32_UINT`、`R32_SINT`、`R32_FLOAT`；`RG32_UINT`、`RG32_SINT`、`RG32_FLOAT`；`RGB32_UINT`、`RGB32_SINT`、`RGB32_FLOAT`；`RGBA32_UINT`、`RGBA32_SINT`、`RGBA32_FLOAT` | `GL_R32*` 至 `GL_RGBA32*` |
| 打包颜色 | `BGRA4_UNORM`、`B5G6R5_UNORM`、`B5G5R5A1_UNORM`、`R10G10B10A2_UNORM`、`R11G11B10_FLOAT` | 移动端/带宽敏感颜色、HDR 场景颜色 |
| 深度/模板 | `D16`、`D24S8`、`X24G8_UINT`、`D32`、`D32S8`、`X32G8_UINT` | `GL_DEPTH_COMPONENT*` 或 `GL_DEPTH*_STENCIL*` |
| 块压缩 | `BC1_UNORM`、`BC1_UNORM_SRGB`、`BC2_UNORM`、`BC2_UNORM_SRGB`、`BC3_UNORM`、`BC3_UNORM_SRGB`、`BC4_UNORM`、`BC4_SNORM`、`BC5_UNORM`、`BC5_SNORM`、`BC6H_UFLOAT`、`BC6H_SFLOAT`、`BC7_UNORM`、`BC7_UNORM_SRGB` | 对应 S3TC、RGTC、BPTC 压缩格式 |
| 特殊 | `UNKNOWN`、`COUNT` | 无格式/枚举边界，不作为实际资源格式。 |

OpenGL 后端用三个转换函数分别得到：外部像素通道布局、内部存储格式和元素类型。它们在 `opengl-format.cpp` 中，而不是仅靠一个 `GLenum` 完成所有语义。

### `FormatKind` 与 `FormatSupport`

| 枚举 | 值 | 含义 |
|---|---|---|
| `FormatKind` | `Integer`、`Normalized`、`Float`、`DepthStencil` | `getFormatInfo` 给出的格式大类；VAO 设置时尤其要区分整数路径和浮点路径。 |
| `FormatSupport` | `None` | 不支持。 |
|  | `Buffer`、`IndexBuffer`、`VertexBuffer` | 可作为一般/索引/顶点缓冲元素。 |
|  | `Texture`、`DepthStencil`、`RenderTarget`、`Blendable` | 可创建纹理、深度模板附件、颜色附件、参与混合。 |
|  | `ShaderLoad`、`ShaderSample`、`ShaderUavLoad`、`ShaderUavStore`、`ShaderAtomic` | shader 的读取、采样、image/存储读写、原子操作能力。 |

`FormatSupport` 是位标志，可用按位或组合多个值；它表达能力，不是资源当前状态。

## 4. `ResourceStates`：跨后端的资源访问意图

| 分组 | 值 | 语义 |
|---|---|---|
| 初始 | `Unknown`、`Common` | 未知或通用状态。 |
| 缓冲角色 | `ConstantBuffer`、`VertexBuffer`、`IndexBuffer`、`IndirectArgument`、`StreamOut` | 分别供常量、顶点、索引、间接参数、流输出使用。 |
| shader 访问 | `PixelShaderResource`、`NonPixelShaderResource`、`ShaderResource`、`UnorderedAccess` | 图形像素阶段、其他阶段、两者合并、任意顺序读写。 |
| 渲染附件 | `RenderTarget`、`DepthWrite`、`DepthRead` | 颜色写入、深度写入、深度只读。 |
| 数据转移 | `CopyDest`、`CopySource`、`ResolveDest`、`ResolveSource` | 拷贝与 MSAA resolve 的源/目标。 |
| 呈现与高级功能 | `Present`、`AccelStructRead`、`AccelStructWrite`、`AccelStructBuildInput`、`AccelStructBuildBlas`、`ShadingRateSurface`、`OpacityMicromapWrite`、`OpacityMicromapBuildInput`、`ConvertCoopVecMatrixInput`、`ConvertCoopVecMatrixOutput` | 交换链和高级光追/可变着色率/协作向量用途。 |

它也是位标志。在 D3D12/Vulkan 中它可驱动精细 transition；当前 OpenGL 后端不保存每个子资源的真实状态，`commitBarriers()` 统一使用 `GL_ALL_BARRIER_BITS`。因此它在 OpenGL 路径里更像跨后端兼容语义，而非严格状态机。

`SharedResourceFlags` 的 `Shared`、`Shared_NTHandle`、`Shared_CrossAdapter` 也是跨 API 资源共享标志；OpenGL 后端没有相应实现。

## 5. shader 阶段和资源绑定类型

### `ShaderType`

| 分组 | 值 |
|---|---|
| 基础阶段 | `Vertex`、`Hull`、`Domain`、`Geometry`、`Pixel`、`Compute` |
| 扩展图形阶段 | `Amplification`、`Mesh`、`AllGraphics` |
| 光追阶段 | `RayGeneration`、`AnyHit`、`ClosestHit`、`Miss`、`Intersection`、`Callable`、`AllRayTracing` |
| 集合/空值 | `None`、`All` |

`ShaderType` 是位标志。OpenGL 后端的图形 pipeline 读取 VS/HS/DS/GS/PS 字段；高级阶段是否可用由 `Feature` 与实际驱动能力共同决定。

### `ResourceType`

| 值 | RHI 语义 | 当前 OpenGL 绑定方式 |
|---|---|---|
| `None`、`Count` | 空值/边界 | 不绑定。 |
| `Texture_SRV` | 只读纹理视图 | `glBindTextureUnit` |
| `Texture_UAV` | 可读写纹理视图 | `glBindImageTexture` |
| `TypedBuffer_SRV/UAV` | 有元素格式的缓冲读/写视图 | 当前归入 SSBO 范围绑定 |
| `StructuredBuffer_SRV/UAV` | 按结构 stride 访问的缓冲读/写视图 | 当前归入 SSBO 范围绑定 |
| `RawBuffer_SRV/UAV` | 原始字节/word 访问的缓冲读/写视图 | 当前归入 SSBO 范围绑定 |
| `ConstantBuffer` | 常量数据 | `GL_UNIFORM_BUFFER` 的范围绑定 |
| `VolatileConstantBuffer` | 高频变更的常量数据 | 当前同样走 UBO 范围绑定 |
| `Sampler` | 过滤和寻址规则 | `glBindSampler` |
| `PushConstants` | 小型按 draw/dispatch 改变的数据 | 当前用内部临时 UBO 模拟。 |
| `RayTracingAccelStruct`、`SamplerFeedbackTexture_UAV` | 高级专用资源 | 当前 OpenGL 后端未落实。 |

`SRV` 表示只读视图，`UAV` 表示可读写视图；它们是 D3D 风格术语，OpenGL 分别更接近 sampler/texture fetch 与 image/SSBO。

## 6. 图元与固定渲染状态

### `PrimitiveType`

| 值 | OpenGL 值 |
|---|---|
| `PointList` | `GL_POINTS` |
| `LineList` | `GL_LINES` |
| `LineStrip` | `GL_LINE_STRIP` |
| `TriangleList` | `GL_TRIANGLES` |
| `TriangleStrip` | `GL_TRIANGLE_STRIP` |
| `TriangleFan` | `GL_TRIANGLE_FAN` |
| `PatchList` | `GL_PATCHES` |
| `TriangleListWithAdjacency`、`TriangleStripWithAdjacency` | 枚举存在；当前图元转换函数没有对应分支。 |

### 混合：`BlendFactor`、`BlendOp`、`ColorMask`

| 枚举 | 值 | 含义 |
|---|---|---|
| `BlendFactor` | `Zero`、`One` | 混合因子为 0 或 1。 |
|  | `SrcColor`、`InvSrcColor`、`DstColor`、`InvDstColor` | 使用源/目标颜色或其反值。 |
|  | `SrcAlpha`、`InvSrcAlpha`、`DstAlpha`、`InvDstAlpha`、`SrcAlphaSaturate` | 使用 alpha 相关因子。 |
|  | `ConstantColor`、`InvConstantColor` | 使用 `blendConstantColor`。 |
|  | `Src1Color`、`InvSrc1Color`、`Src1Alpha`、`InvSrc1Alpha` | 双源混合因子；需驱动与 shader 输出配合。 |
| `BlendOp` | `Add`、`Subtract`、`ReverseSubtract`、`Min`、`Max` | 源与目标如何合并。 |
| `ColorMask` | `Red`、`Green`、`Blue`、`Alpha`、`All` | 位标志；控制颜色附件可写入的通道。 |

### 光栅化、深度和模板

| 枚举 | 值 | 含义 |
|---|---|---|
| `RasterFillMode` | `Solid`/`Fill`、`Wireframe`/`Line` | 填充三角形内部或只画边线。 |
| `RasterCullMode` | `Back`、`Front`、`None` | 剔除背面、正面或不剔除。 |
| `ComparisonFunc` | `Never`、`Less`、`Equal`、`LessOrEqual`、`Greater`、`NotEqual`、`GreaterOrEqual`、`Always` | 深度或模板参考值与已有值的比较规则。 |
| `StencilOp` | `Keep`、`Zero`、`Replace`、`IncrementAndClamp`、`DecrementAndClamp`、`Invert`、`IncrementAndWrap`、`DecrementAndWrap` | 模板/深度测试后如何更新模板值。 |

这些值由 `applyBlendState`、`applyDepthStencilState`、`applyRasterState` 转成 `glBlend*`、`glDepth*`、`glStencil*`、`glCullFace`、`glPolygonMode` 等状态调用。

## 7. `SamplerAddressMode` 与 `SamplerReductionType`

| 枚举 | 值 | 含义 | 当前 OpenGL 落点 |
|---|---|---|---|
| `SamplerAddressMode` | `Clamp` / `ClampToEdge` | 超出 UV 范围时取边缘像素 | `GL_CLAMP_TO_EDGE` |
|  | `Wrap` / `Repeat` | UV 按 1.0 周期重复 | `GL_REPEAT` |
|  | `Border` / `ClampToBorder` | 超出范围时取 border color | `GL_CLAMP_TO_BORDER` |
|  | `Mirror` / `MirroredRepeat` | 每次重复翻转方向 | `GL_MIRRORED_REPEAT` |
|  | `MirrorOnce` / `MirrorClampToEdge` | 镜像一次后钳制 | 当前转换函数无专门分支。 |
| `SamplerReductionType` | `Standard` | 普通颜色/数值采样 | 默认。 |
|  | `Comparison` | 与参考深度比较，返回比较结果 | 设置 `GL_COMPARE_REF_TO_TEXTURE`。 |
|  | `Minimum`、`Maximum` | 取采样范围的最小/最大规约 | 当前创建路径未单独映射。 |

## 8. 能力、队列与消息

| 枚举 | 值 | OpenGL 现状 |
|---|---|---|
| `Feature` | `ComputeQueue`、`CopyQueue`、`DeferredCommandLists` | OpenGL 的命令模型与显式多队列不同；这些不能简单等同。 |
|  | `ConstantBufferRanges`、`ShaderSpecializations` | 可由当前机制部分表达，具体取决于实现。 |
|  | `ConservativeRasterization`、`FastGeometryShader`、`Meshlets`、`VariableRateShading` | 依赖扩展或当前未完整实现。 |
|  | `RayQuery`、`RayTracingAccelStruct`、`RayTracingClusters`、`RayTracingOpacityMicromap`、`RayTracingPipeline` | 当前 OpenGL 后端未落实。 |
|  | `SamplerFeedback`、`ShaderExecutionReordering`、`SinglePassStereo`、`Spheres`、`VirtualResources`、`WaveLaneCountMinMax`、`CooperativeVectorInferencing`、`CooperativeVectorTraining`、`EnhancedBarriers` | 多数属于其他 API 或高级能力；应以 `queryFeatureSupport` 返回值为准。 |
| `MessageSeverity` | `Info`、`Warning`、`Error`、`Fatal` | 后端向引擎消息回调报告诊断信息的严重程度。 |
| `CommandQueue` | `Graphics`、`Compute`、`Copy`、`Count` | 统一队列概念；当前 OpenGL 后端主要在同一上下文顺序执行。 |

## 9. 读代码时的判断顺序

```text
先看 Desc 中的枚举
  → 再看 OpenGL 后端是否有 switch / 映射
    → 再看 GLContext capability 是否允许
      → 最后看 CommandList 是否真的发出了对应 GL 调用
```

例如 `ResourceType::Texture_UAV` 并不只意味着“这是一张纹理”：它进一步决定绑定路径为 image、可访问的 mip/layer、读写权限以及随后可能需要的 memory barrier。
