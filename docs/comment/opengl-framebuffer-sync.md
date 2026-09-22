# OpenGL Framebuffer、同步、查询与调试对照

## 1. Framebuffer：把纹理变成渲染附件

Framebuffer Object（FBO）不存像素，它只是“颜色、深度、模板附件怎样组合”的描述。

| API | 作用 | 何时使用 |
|---|---|---|
| `glCreateFramebuffers` / `glGenFramebuffers` | 创建 FBO | 正常创建路径/临时清除路径 |
| `glBindFramebuffer` | 选择读、写或同时读写的 FBO | 附着、清除、绘制前 |
| `glFramebufferTexture` | 附着完整纹理或适用的纹理层级 | 通用路径 |
| `glFramebufferTexture2D` | 附着 2D 纹理或 cubemap 的单一面 | 2D/cubemap 路径 |
| `glFramebufferTextureLayer` | 附着数组、3D 或 cubemap array 的指定层 | layer 渲染 |
| `glDrawBuffer` / `glDrawBuffers` | 选择哪个/哪些颜色附件接收片元输出 | FBO 完成前设置 |
| `glReadBuffer` | 选择像素读取来源 | 深度专用 FBO 通常设为 `GL_NONE` |
| `glCheckFramebufferStatus` | 验证所有附件尺寸、格式与输出配置可用 | `createFramebuffer` 最后 |
| `glDeleteFramebuffers` | 回收 FBO | `Framebuffer` 析构和临时路径 |

`glFramebufferTexture2D` 的 cubemap 情况要传入 `GL_TEXTURE_CUBE_MAP_POSITIVE_X + face`；数组纹理不是传一个新的 target，而是用 `glFramebufferTextureLayer` 指定 layer。

## 2. 清除与 GPU 内拷贝

| API | 作用 |
|---|---|
| `glClearColor` / `glClearDepthf` / `glClearStencil` | 设置随后 `glClear` 要使用的颜色、深度、模板值 |
| `glClear(mask)` | 根据 mask 清除当前 draw FBO 的附件 |
| `glCopyImageSubData` | 在两张纹理的指定 mip/layer/区域间复制像素，不经过 shader |
| `glCopyBufferSubData` | 在当前 read/write buffer 的指定字节范围间复制 |

项目针对单独清颜色或深度/模板的路径，会暂时建立 FBO、附着目标 mip/layer、执行清除后回收；常规绘制则复用缓存的 FBO。

## 3. `glMemoryBarrier` 与 fence 解决的问题不同

| 工具 | 保证什么 | 项目用途 |
|---|---|---|
| `glMemoryBarrier(bits)` | 同一 GPU 命令序列中，先前 shader 写入对后续指定类型访问可见 | `commitBarriers` 目前使用 `GL_ALL_BARRIER_BITS` |
| `glFenceSync` | 在命令流中插入一个“到这里为止完成”的标记 | `execute` 后建立 fence |
| `glClientWaitSync` | CPU 查询或等待 fence 是否完成 | `waitForIdle`、事件查询 |
| `glDeleteSync` | 回收 fence | 下次设置或析构时 |
| `glFinish` | CPU 等待此前所有 GPU 工作完成 | `waitForIdle` 的最终等待 |

要点：barrier 处理的是 GPU 访问可见性；fence 处理的是 CPU 与 GPU 的完成时序。前者不让 CPU 等待，后者本身也不自动替代资源可见性屏障。

## 4. GPU 时间查询

| API | 作用 |
|---|---|
| `glGenQueries` / `glDeleteQueries` | 创建/回收查询对象 |
| `glQueryCounter(query, GL_TIMESTAMP)` | 在 GPU 命令流当前位置记录时间戳 |
| `glGetQueryObjectiv(..., GL_QUERY_RESULT_AVAILABLE)` | 查询结果是否已经可读 |
| `glGetQueryObjectui64v(..., GL_QUERY_RESULT)` | 读取 64 位 GPU 时间戳 |

项目为一个事件保存开始和结束两个 query；两者差值是 GPU 耗时。读取前先检查可用性，避免 CPU 因结果尚未产生而卡住。

## 5. 调试分组与能力查询

| API | 作用 |
|---|---|
| `glPushDebugGroup` / `glPopDebugGroup` | 把命令范围标上名称，供 RenderDoc 等工具显示层级 |
| `glGetIntegerv` | 查询单个整数状态，例如版本、限制、当前绑定 |
| `glGetIntegeri_v` | 查询 indexed target 的某一个编号槽，例如 UBO/SSBO binding |

`glGet*` 是状态读取，不应放在高频绘制热路径。项目主要在设备初始化、资源创建时保存/恢复状态或获取能力上限时使用它们。
