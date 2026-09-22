# Dodoe OpenGL API 对照索引

范围：`engine/external/cutie-rhi/src/opengl/` 中当前实际调用的 OpenGL 函数。第三方代码的示例目录不计入；`gl*` 名称只是 OpenGL 入口，真正的资源语义由 Cutie RHI 的 `TextureDesc`、`BufferDesc`、`GraphicsState` 等描述决定。

## 先按文件找入口

| 文件 | 主要职责 | 建议配合阅读的文档 |
|---|---|---|
| `opengl-device.cpp` | 创建资源、Framebuffer、Pipeline、输入布局、fence/query | [资源](opengl-resources.md)、[Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `opengl-commandlist.cpp` | 上传、绑定、状态应用、绘制、派发、屏障 | [绑定](opengl-bindings.md)、[绘制与状态](opengl-draw-state.md) |
| `opengl-shader.cpp` | 着色器对象、日志和错误信息 | [绑定](opengl-bindings.md) |
| `opengl-*.cpp` 的析构文件 | 回收纹理、缓冲、程序、VAO、Sampler、Query | [资源](opengl-resources.md) |

## API 分类总表

### 对象与资源

| API | 一句话作用 | 专题 |
|---|---|---|
| `glGenTextures` / `glDeleteTextures` | 创建/销毁纹理对象名 | [资源](opengl-resources.md) |
| `glGenBuffers` / `glCreateBuffers` / `glDeleteBuffers` | 创建/销毁缓冲对象 | [资源](opengl-resources.md) |
| `glGenSamplers` / `glDeleteSamplers` | 创建/销毁独立采样器状态 | [资源](opengl-resources.md) |
| `glGenVertexArrays` / `glCreateVertexArrays` / `glDeleteVertexArrays` | 创建/销毁顶点输入状态对象 | [资源](opengl-resources.md) |
| `glBufferData` / `glNamedBufferData` | 建立缓冲存储 | [资源](opengl-resources.md) |
| `glBufferSubData` / `glNamedBufferSubData` | 更新一段缓冲字节 | [资源](opengl-resources.md) |
| `glMapBufferRange` / `glUnmapBuffer` | 将指定缓冲区范围映射给 CPU | [资源](opengl-resources.md) |
| `glCopyBufferSubData` / `glCopyImageSubData` | GPU 内缓冲/纹理拷贝 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |

### 纹理与采样器

| API | 一句话作用 | 专题 |
|---|---|---|
| `glBindTexture` / `glBindTextureUnit` | 按纹理目标或纹理单元关联纹理 | [纹理](opengl-textures.md)、[绑定](opengl-bindings.md) |
| `glTexStorage2D` / `glTexStorage3D` | 建立固定规格的普通纹理存储 | [纹理](opengl-textures.md) |
| `glTexStorage2DMultisample` / `glTexStorage3DMultisample` | 建立 MSAA 纹理存储 | [纹理](opengl-textures.md) |
| `glTexSubImage2D` / `glTexSubImage3D` | 向已有纹理的指定区域上传像素 | [纹理](opengl-textures.md) |
| `glTexParameteri` | 设置纹理对象自身的采样参数 | [纹理](opengl-textures.md) |
| `glSamplerParameteri` / `glSamplerParameterf` | 设置独立 Sampler 的过滤、寻址和 LOD 参数 | [资源](opengl-resources.md) |
| `glBindSampler` | 将 Sampler 绑定到纹理单元 | [绑定](opengl-bindings.md) |

### 顶点输入、程序与绑定

| API | 一句话作用 | 专题 |
|---|---|---|
| `glBindVertexArray` | 选中当前 VAO | [资源](opengl-resources.md) |
| `glVertexArrayAttribFormat` / `glVertexArrayAttribIFormat` | 描述浮点/整数顶点属性的格式 | [资源](opengl-resources.md) |
| `glVertexArrayAttribBinding` / `glVertexArrayVertexBuffer` | 将属性接到顶点缓冲绑定槽 | [资源](opengl-resources.md) |
| `glVertexArrayElementBuffer` | 将索引缓冲关联到 VAO | [资源](opengl-resources.md) |
| `glEnableVertexArrayAttrib` / `glVertexArrayBindingDivisor` | 启用属性、设置实例步进率 | [资源](opengl-resources.md) |
| `glCreateProgram` / `glAttachShader` / `glLinkProgram` / `glDeleteProgram` | 组装与回收可用的 shader program | [绑定](opengl-bindings.md) |
| `glCreateShader` / `glShaderSource` / `glGetShaderiv` / `glGetShaderInfoLog` / `glDeleteShader` | 建立 shader 对象、提供源文本、读取状态和日志、回收对象 | [绑定](opengl-bindings.md) |
| `glGetProgramiv` / `glGetProgramInfoLog` | 查询 program 状态与日志 | [绑定](opengl-bindings.md) |
| `glUseProgram` | 选中当前绘制或计算使用的 program | [绑定](opengl-bindings.md) |
| `glBindBufferBase` / `glBindBufferRange` | 将缓冲整体或一段范围绑定到 indexed target | [绑定](opengl-bindings.md) |
| `glBindImageTexture` | 将纹理 mip/layer 绑定为 shader image | [绑定](opengl-bindings.md) |

### Framebuffer、清除与读取

| API | 一句话作用 | 专题 |
|---|---|---|
| `glCreateFramebuffers` / `glGenFramebuffers` / `glDeleteFramebuffers` | 创建/销毁 Framebuffer 对象 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glBindFramebuffer` | 选中读或写 Framebuffer | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glFramebufferTexture` / `glFramebufferTexture2D` / `glFramebufferTextureLayer` | 将纹理、cubemap 面或数组层附着到 Framebuffer | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glDrawBuffer` / `glDrawBuffers` / `glReadBuffer` | 指定颜色附件输出与读取来源 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glCheckFramebufferStatus` | 验证附件组合是否可渲染 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glClearColor` / `glClearDepthf` / `glClearStencil` / `glClear` | 设置清除值并执行清除 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |

### 绘制、计算与固定状态

| API | 一句话作用 | 专题 |
|---|---|---|
| `glDrawArrays` / `glDrawArraysInstanced` / `glDrawArraysInstancedBaseInstance` | 不用索引的普通/实例化绘制 | [绘制与状态](opengl-draw-state.md) |
| `glDrawElements` / `glDrawElementsInstanced` / `glDrawElementsInstancedBaseVertexBaseInstance` | 使用索引的普通/实例化绘制 | [绘制与状态](opengl-draw-state.md) |
| `glMultiDrawArraysIndirect` / `glMultiDrawElementsIndirect` / `glMultiDrawElementsIndirectCount` | 从间接参数缓冲一次发出多条绘制 | [绘制与状态](opengl-draw-state.md) |
| `glDispatchCompute` / `glDispatchComputeIndirect` | 直接或从缓冲参数派发计算工作组 | [绘制与状态](opengl-draw-state.md) |
| `glViewport` / `glScissor` / `glDepthRange` | 设置屏幕映射、裁剪矩形与深度映射 | [绘制与状态](opengl-draw-state.md) |
| `glEnable` / `glDisable` / `glEnablei` / `glDisablei` | 开关全局或某个颜色附件的能力 | [绘制与状态](opengl-draw-state.md) |
| `glBlendColor` / `glBlendFuncSeparatei` / `glBlendEquationSeparatei` / `glColorMaski` | 配置每个颜色附件的混合和写掩码 | [绘制与状态](opengl-draw-state.md) |
| `glDepthMask` / `glDepthFunc` | 设置深度写入和比较方式 | [绘制与状态](opengl-draw-state.md) |
| `glStencilFuncSeparate` / `glStencilMaskSeparate` / `glStencilOpSeparate` | 分别配置正反面的模板测试 | [绘制与状态](opengl-draw-state.md) |
| `glCullFace` / `glFrontFace` / `glPolygonMode` / `glPolygonOffset` | 配置剔除、正面方向、填充模式、深度偏移 | [绘制与状态](opengl-draw-state.md) |

### 同步、查询、调试与能力查询

| API | 一句话作用 | 专题 |
|---|---|---|
| `glMemoryBarrier` | 让先前 shader 写入对后续访问可见 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glFenceSync` / `glClientWaitSync` / `glDeleteSync` / `glFinish` | GPU/CPU 完成顺序与等待 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glGenQueries` / `glQueryCounter` / `glGetQueryObjectiv` / `glGetQueryObjectui64v` / `glDeleteQueries` | GPU 时间戳查询的生命周期和读取 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glPushDebugGroup` / `glPopDebugGroup` | 在图形调试器中标记命令范围 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
| `glGetIntegerv` / `glGetIntegeri_v` | 查询版本、限制和 indexed binding 的能力 | [Framebuffer 与同步](opengl-framebuffer-sync.md) |
