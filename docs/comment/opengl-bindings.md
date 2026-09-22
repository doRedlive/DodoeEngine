# OpenGL Program 与资源绑定对照

## 1. Program：一次绘制要执行的 shader 组合

项目在 `opengl-shader.cpp` 处理 shader 对象，在 `opengl-device.cpp` 中组合 program，并在 `opengl-commandlist.cpp` 的图形状态应用阶段选中 program。

| API | 作用 | 项目语义 |
|---|---|---|
| `glCreateShader(stage)` | 创建一个指定阶段的 shader 对象 | 保存 Cutie shader 的阶段和源文本 |
| `glShaderSource(shader, ...)` | 为 shader 提供源文本 | 将加载到的 GLSL 交给对象 |
| `glGetShaderiv` / `glGetShaderInfoLog` | 读取 shader 状态和诊断文本 | 出错时形成日志 |
| `glDeleteShader` | 回收 shader 对象 | 失败路径或不再需要时使用 |
| `glCreateProgram` | 创建 program 容器 | 创建图形/计算 pipeline 时使用 |
| `glAttachShader` | 将阶段对象挂到 program | 一个 program 可附着多个阶段 |
| `glLinkProgram` | 让各阶段接口成为一个可使用的 program | 属性、varying、资源接口在此检查 |
| `glGetProgramiv` / `glGetProgramInfoLog` | 读取 program 状态和诊断文本 | 链接失败时记录原因 |
| `glUseProgram` | 选择当前 program | 绘制或派发前调用 |
| `glDeleteProgram` | 回收 program | `GraphicsPipeline` / `ComputePipeline` 析构时调用 |

`glUseProgram` 是状态切换，而不是绘制命令；它只决定紧随其后的 shader 执行代码来源。

## 2. 纹理单元：纹理与 Sampler 的会合点

```text
纹理对象 ── glBindTextureUnit(unit, texture) ──┐
                                                ├─ unit N ── shader sampler binding N
Sampler 对象 ── glBindSampler(unit, sampler) ──┘
```

| API | 作用 | 项目位置 |
|---|---|---|
| `glBindTextureUnit(unit, texture)` | 将纹理直接放到编号为 `unit` 的纹理单元 | `applyGraphicsBindings` |
| `glBindSampler(unit, sampler)` | 为该单元设定独立采样状态 | `applyGraphicsBindings` |
| `glBindTexture(target, texture)` | 传统 target 型绑定，供创建和上传时的 `glTex*` 使用 | 资源创建/上传 |

`unit` 是运行时编号，不是 texture 对象名。shader 中的 `sampler2D` 绑定号和这个编号必须对应，项目以 `registerSpace * 6 + slot + arrayElement` 计算线性编号。

## 3. UBO、SSBO 与范围绑定

| API | 形式 | 适用对象 | 含义 |
|---|---|---|---|
| `glBindBufferBase(target, index, buffer)` | 整体绑定 | UBO、SSBO、原子计数等 indexed target | 第 `index` 槽使用整个缓冲 |
| `glBindBufferRange(target, index, buffer, offset, size)` | 区间绑定 | UBO、SSBO | 第 `index` 槽只可见 `[offset, offset + size)` |

项目绑定 constant buffer 和 shader storage buffer 时主要使用 `glBindBufferRange`。这让一个大环形缓冲可以切成多份：每次绘制只绑定属于自己的一段，而无需复制数据。

```glsl
layout(std140, binding = 0) uniform Camera { mat4 viewProj; };
layout(std430, binding = 3) buffer Instances { Instance data[]; };
```

前者对应 `GL_UNIFORM_BUFFER`，通常只读且布局遵守 `std140`；后者对应 `GL_SHADER_STORAGE_BUFFER`，可随机读写并通常使用 `std430`。两者的 `binding` 就是 OpenGL API 的 `index`。

## 4. Image：shader 可读写的纹理视图

```cpp
glBindImageTexture(unit, texture, mip, layered, layer,
                   access, format);
```

| 参数 | 含义 |
|---|---|
| `unit` | image binding 编号 |
| `mip` | 暴露给 shader 的 mip 级 |
| `layered` | 是否把所有层作为一个 layered image 暴露 |
| `layer` | 非 layered 时暴露哪一层 |
| `access` | `GL_READ_ONLY`、`GL_WRITE_ONLY` 或 `GL_READ_WRITE` |
| `format` | shader image 视图使用的像素格式 |

它和 `sampler2D` 的核心区别是：sampler 用于采样读取，image 用于按整数坐标读取或写入。项目以 `glBindImageTexture` 映射 storage texture/UAV；写入后若下一步要以另一种方式读取，应使用相应的 `glMemoryBarrier`。
