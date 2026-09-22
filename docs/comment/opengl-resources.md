# OpenGL 资源、Sampler 与 VAO 对照

## 1. 缓冲区：一段 GPU 字节，不等于一种数据

`GLuint` 缓冲对象只是一段内存；`GL_ARRAY_BUFFER`、`GL_UNIFORM_BUFFER`、`GL_SHADER_STORAGE_BUFFER` 等 target 决定当前 API 调用以什么角色使用它。

| API | 作用 | 项目调用位置 |
|---|---|---|
| `glGenBuffers` | 产生缓冲对象名 | `opengl-device.cpp` 的 `createBuffer` |
| `glCreateBuffers` | 以直接状态访问方式产生对象名 | `opengl-commandlist.cpp` 的临时/辅助缓冲 |
| `glBindBuffer(target, buffer)` | 将对象放入一个传统绑定槽，供后续 target 型 API 使用 | 创建、上传、映射、拷贝 |
| `glBufferData(target, size, data, usage)` | 为当前绑定槽中的缓冲建立或替换存储 | `createBuffer` |
| `glNamedBufferData(buffer, size, data, usage)` | 不需先 bind 的 `glBufferData` 版本 | 辅助缓冲创建 |
| `glBufferSubData` / `glNamedBufferSubData` | 覆盖缓冲区的一个字节范围 | `writeBuffer`、辅助数据更新 |
| `glMapBufferRange` / `glUnmapBuffer` | 将指定范围借给 CPU 读写，结束后归还 | `mapBuffer` / `unmapBuffer` |
| `glCopyBufferSubData` | 在 `READ` 与 `WRITE` 两个缓冲绑定槽之间拷贝字节 | `copyBuffer` |
| `glDeleteBuffers` | 回收缓冲对象 | `Buffer::~Buffer`、命令列表临时对象 |

`glMapBufferRange` 返回的是 CPU 指针，但 GPU 仍可能使用同一段内存；调用方需要遵守资源同步约定。它的 `offset` 与 `length` 用字节表示，访问标志决定读、写和失效行为。

## 2. 纹理对象与 Sampler 对象

纹理对象保存像素存储与部分参数；Sampler 对象只保存“怎样读取”。项目创建纹理时会设默认参数，创建 Sampler 时则可将过滤、寻址和 LOD 作为独立资源复用。

| API | 作用 |
|---|---|
| `glGenTextures` / `glDeleteTextures` | 创建/回收纹理对象名 |
| `glBindTexture(target, texture)` | 将纹理放入指定形状的传统绑定槽 |
| `glTexParameteri` | 修改当前绑定纹理的过滤或 U/V/W 寻址参数 |
| `glGenSamplers` / `glDeleteSamplers` | 创建/回收独立 Sampler |
| `glSamplerParameteri` / `glSamplerParameterf` | 设置 Sampler 的 min/mag filter、wrap、LOD 等 |
| `glBindSampler(unit, sampler)` | 将读取规则放到某个纹理单元；与 `glBindTextureUnit` 配对 |

当某个纹理单元同时绑定了纹理和独立 Sampler 时，采样以 Sampler 参数为准。因此同一张纹理可同时用于“线性重复”和“最近点采样”等不同用途。

## 3. VAO：顶点输入状态的容器

VAO（Vertex Array Object）保存“shader 输入属性如何从顶点/索引缓冲取数据”的连接关系。它不保存顶点字节本身。

```text
顶点缓冲 ── glVertexArrayVertexBuffer ──┐
                                         ├─ VAO ── glBindVertexArray ── 绘制
属性格式 ── glVertexArrayAttribFormat ──┘
索引缓冲 ── glVertexArrayElementBuffer ── VAO
```

| API | 作用 | 关键点 |
|---|---|---|
| `glGenVertexArrays` / `glCreateVertexArrays` | 创建 VAO | 前者配合传统 bind；后者可直接设置对象 |
| `glBindVertexArray` | 选中当前 VAO | 绘制时必须是预期 VAO |
| `glVertexArrayAttribFormat` | 指定浮点路径属性的分量数、类型、归一化和相对偏移 | 位置、UV、颜色通常走此路径 |
| `glVertexArrayAttribIFormat` | 指定整数路径属性格式 | 整数不会被转换为 float |
| `glVertexArrayAttribBinding` | 令属性编号使用某个顶点缓冲 binding 槽 | 将“格式”与“缓冲来源”分离 |
| `glEnableVertexArrayAttrib` | 启用属性编号 | 未启用的属性不会从缓冲读取 |
| `glVertexArrayVertexBuffer` | 为 binding 槽指定缓冲、首地址和 stride | 命令列表按绑定状态填入 |
| `glVertexArrayBindingDivisor` | 设置每隔多少实例推进一次数据 | 非零值用于实例数据 |
| `glVertexArrayElementBuffer` | 关联索引缓冲 | `glDrawElements*` 从它读取索引 |
| `glDeleteVertexArrays` | 回收 VAO | `InputLayout` 析构时调用 |

浮点属性与整数属性不可随意互换：例如骨骼索引应使用 `glVertexArrayAttribIFormat`，这样 shader 端接收的是整数值；权重则通常走浮点格式。

## 4. 对象生命周期的速记

```text
创建对象名 → 建立存储/状态 → 在命令提交时绑定 → 使用 → 析构时删除
```

OpenGL 的对象名只是句柄，真正的状态大多附着在对象本身。传统 bind 型 API 的风险是“当前绑定对象”是隐式状态；项目同时使用直接状态访问 API（名称含 `Named`、`Create`、`VertexArray`）来减少这类依赖。
