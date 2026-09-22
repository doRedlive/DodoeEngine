# OpenGL 纹理存储与纹理目标

本文接着缓冲区目标的讨论，说明 `glTexStorage*`、`GL_TEXTURE_2D` 等名字分别在做什么，并对应 Dodoe 的 OpenGL 后端实现。

## 1. 先分清三个概念

一张 OpenGL 纹理由三个互不相同的概念组成：

| 概念 | 例子 | 回答的问题 |
|---|---|---|
| 纹理对象 | `GLuint texture` | “是哪一张纹理？” |
| 纹理目标 | `GL_TEXTURE_2D` | “它是二维、立方体、数组还是三维纹理？” |
| 像素存储 | `glTexStorage2D(...)` | “为它准备多少级 mip、每级多大、像素内部怎样保存？” |

`GL_TEXTURE_2D` 这类名称来自 OpenGL 头文件，形式上是 `GLenum` 的具名常量。它们用可读的名字代替数值，并作为 API 的参数告诉驱动程序该按哪一种纹理类型处理对象。

纹理目标不能和上篇的 `GL_ARRAY_BUFFER` 混为一谈：前者描述**纹理的形状和采样方式**，后者描述**缓冲区在管线中的绑定用途**。

```cpp
glBindTexture(GL_TEXTURE_2D, texture); // 将 texture 作为二维纹理操作
glBindBuffer(GL_ARRAY_BUFFER, buffer); // 将 buffer 作为顶点数据源操作
```

## 2. `glTexStorage*`：一次确定纹理的存储

典型二维调用：

```cpp
glBindTexture(GL_TEXTURE_2D, texture);
glTexStorage2D(
    GL_TEXTURE_2D, // 纹理目标
    5,             // mip 级数
    GL_RGBA8,      // 内部像素格式
    1024, 1024);   // 第 0 级宽、高
```

它的含义是：为一张 `1024 × 1024` 的二维纹理准备 5 个 mip 级别，内部按 `RGBA8` 保存。5 级尺寸依次是 `1024²`、`512²`、`256²`、`128²`、`64²`。

`glTexStorage*` 建立的是 immutable storage（固定规格的纹理存储）：调用后，mip 数、内部格式和每级尺寸已经确定，不能再以不同规格重新定义同一个对象。之后仍然可以上传或更新像素，例如：

```cpp
glTexSubImage2D(GL_TEXTURE_2D, 0,
                0, 0, 1024, 1024,
                GL_RGBA, GL_UNSIGNED_BYTE, pixels);
```

这里 `glTexSubImage2D` 只是在已存在的第 0 级中写入 `pixels`，没有改变存储规格。

### 为什么常用它

- 资源的格式、mip 层级和大小在创建时就明确，状态更稳定。
- 所有 mip 级别会被一次性准备好，不会遗漏某一级。
- 创建纹理与上传内容可以清晰分开。

函数尾部的 `2D`、`3D` 指的是**为多少个尺寸分配存储**，不是简单地等于纹理目标：二维数组和 cubemap array 虽然每一层是二维图像，但它们还需要“层”这一维，因此使用 `glTexStorage3D`。

## 3. 常见 `GL_TEXTURE_*` 目标

| 目标 | 数据形状 | Shader 中常见采样器 | 常见场景 |
|---|---|---|---|
| `GL_TEXTURE_1D` | 一条像素线 | `sampler1D` | 渐变查找表；较少使用 |
| `GL_TEXTURE_2D` | 一张宽 × 高的图 | `sampler2D` | 颜色贴图、法线贴图、深度图、渲染目标 |
| `GL_TEXTURE_3D` | 宽 × 高 × 深的连续体积 | `sampler3D` | 雾、体积噪声、医学/体积数据 |
| `GL_TEXTURE_1D_ARRAY` | 多条同尺寸 1D 图 | `sampler1DArray` | 多组查找表 |
| `GL_TEXTURE_2D_ARRAY` | 多张同尺寸 2D 图，按 layer 索引 | `sampler2DArray` | 阴影图层、地形层、图集替代方案 |
| `GL_TEXTURE_CUBE_MAP` | 6 张正方形面，分别对应 ±X、±Y、±Z | `samplerCube` | 天空盒、环境反射、点光源阴影 |
| `GL_TEXTURE_CUBE_MAP_ARRAY` | 多个 cubemap，每个含 6 面 | `samplerCubeArray` | 多个探针或点光源阴影集合 |
| `GL_TEXTURE_2D_MULTISAMPLE` | 每个像素多个采样值 | `sampler2DMS` | MSAA 颜色/深度附件 |
| `GL_TEXTURE_2D_MULTISAMPLE_ARRAY` | 多层 MSAA 2D 图 | `sampler2DMSArray` | 分层 MSAA 渲染 |
| `GL_TEXTURE_BUFFER` | 用缓冲区内存作为一维纹理数据 | `samplerBuffer` | 大型只读查找数据；与普通 2D 图不同 |

### 2D 数组、3D 与 cubemap 的区别

这三个最容易混淆：

- **2D array**：第 3 个坐标是离散的 layer 编号；各层尺寸与格式相同，层之间不会做插值。
- **3D texture**：第 3 个坐标是连续的体积坐标；沿 Z 方向也可进行过滤插值。
- **cubemap**：采样坐标是方向向量 `(x, y, z)`；OpenGL 自动选择六个面之一并完成面内采样。

## 4. 纹理目标如何贯穿 API

`target` 不只是创建时的一个参数。它规定了后续相关 API 如何理解纹理对象：

```cpp
glBindTexture(GL_TEXTURE_2D, texture);
glTexStorage2D(GL_TEXTURE_2D, mipCount, GL_RGBA8, width, height);
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                GL_RGBA, GL_UNSIGNED_BYTE, pixels);
```

对于 cubemap，分配时目标是总的 `GL_TEXTURE_CUBE_MAP`；上传某一张面时使用专门的面目标：

```cpp
glTexStorage2D(GL_TEXTURE_CUBE_MAP, mipCount, GL_RGBA16F, size, size);
glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0,
                0, 0, size, size, GL_RGBA, GL_FLOAT, pixels);
```

对于 array texture，上传时则额外指定 layer，通常用 `glTexSubImage3D` 的 `zoffset` 表示。

## 5. 项目后端中的实际路径

`engine/external/cutie-rhi/src/opengl/opengl-device.cpp` 中的 `Device::createTexture` 做了以下事情：

```text
TextureDesc
  → textureTarget(desc) 选出 GL_TEXTURE_*
  → glGenTextures 得到 OpenGL 对象名
  → glBindTexture(target, texture)
  → glTexParameteri 设置过滤和寻址方式
  → 依纹理形状调用 glTexStorage2D / 3D / 2DMultisample / 3DMultisample
  → 返回 TextureHandle
```

按 OpenGL 的标准语义，纹理形状和存储函数应当如下对应：

| 引擎描述 | 预期 OpenGL 目标 | 对应存储函数 |
|---|---|---|
| `Texture1D` | `GL_TEXTURE_1D` | 一维 storage 入口 |
| `Texture1DArray` | `GL_TEXTURE_1D_ARRAY` | `glTexStorage2D` |
| `Texture2D` | `GL_TEXTURE_2D` | `glTexStorage2D` |
| `Texture2DArray` | `GL_TEXTURE_2D_ARRAY` | `glTexStorage3D` |
| `TextureCube` | `GL_TEXTURE_CUBE_MAP` | `glTexStorage2D` |
| `TextureCubeArray` | `GL_TEXTURE_CUBE_MAP_ARRAY` | `glTexStorage3D` |
| `Texture2DMS` | `GL_TEXTURE_2D_MULTISAMPLE` | `glTexStorage2DMultisample` |
| `Texture2DMSArray` | `GL_TEXTURE_2D_MULTISAMPLE_ARRAY` | `glTexStorage3DMultisample` |
| `Texture3D` | `GL_TEXTURE_3D` | `glTexStorage3D` |

### 当前实现中值得核对的点

`TextureDimension` 定义了 `Texture1DArray`、`Texture2DArray`、`TextureCubeArray` 和 `Texture2DMSArray` 等独立枚举值；但 `textureTarget(desc)` 目前只直接匹配 `Texture1D`、`Texture2D`、`Texture2DMS`、`TextureCube`、`Texture3D`。因此当描述实际使用这些独立的数组枚举值时，函数会落入默认分支并得到 `GL_TEXTURE_2D`。

后面的存储代码已经有 `Texture1DArray`、`Texture2DArray` 的专门分支，这与前面的目标选择不完全一致。数组纹理功能投入使用前，应确认描述层究竟约定“基础维度 + `arraySize`”还是“专用 Array 枚举”，并让目标映射和存储分支采用同一种约定。

`writeTexture` 当前只按 `Texture2D`、`TextureCube`、`Texture3D` 分派到 `glTexSubImage2D/3D`；数组和多重采样纹理没有上传分支。多重采样纹理通常也不从 CPU 逐像素上传，而是作为渲染附件写入或由 resolve/copy 得到内容。

此外，二维多重采样数组的 storage 调用应把 `sampleCount` 作为第二个参数；当前实现传入的是 `mips`。这与 `glTexStorage2DMultisample` 分支的参数选择不同，应在该资源类型启用前核对。

另一个细节是函数保存和恢复时读取的是 `GL_TEXTURE_BINDING_2D`，但创建的 `target` 可能是 cubemap 或数组目标。若调用前绑定的是非 2D 纹理，恢复的绑定点也应与实际 `target` 匹配。

## 6. 相关的采样参数

项目在创建后默认设置：

| 参数 | 当前值 | 含义 |
|---|---|---|
| `GL_TEXTURE_MIN_FILTER` | 单 mip 为 `GL_LINEAR`；多 mip 为 `GL_LINEAR_MIPMAP_LINEAR` | 图像缩小时怎样过滤 mip 和相邻像素 |
| `GL_TEXTURE_MAG_FILTER` | `GL_LINEAR` | 图像放大时做线性过滤 |
| `GL_TEXTURE_WRAP_S/T` | `GL_REPEAT` | U/V 超出 `[0, 1]` 后重复采样 |
| `GL_TEXTURE_WRAP_R` | 3D 纹理为 `GL_REPEAT` | W 方向的重复规则 |

这些参数决定“如何读纹理”，而 `glTexStorage*` 决定“纹理以什么规格存在”；两者职责不同。

## 7. 阅读代码时的速记

```text
GL_TEXTURE_2D              → 图像的形状/目标
glTexStorage2D             → 准备图像各 mip 的存储
glTexSubImage2D            → 向已准备好的区域写像素
glTexParameteri            → 设置采样过滤与越界规则
sampler2D                  → shader 中读取该类纹理的接口
```

因此，看到下面的代码可以直接读成：“创建一张可由 `sampler2D` 读取的二维图；有 `mipCount` 级、内部格式为 `internalFormat`、首级大小是 `width × height`。”

```cpp
glBindTexture(GL_TEXTURE_2D, texture);
glTexStorage2D(GL_TEXTURE_2D, mipCount, internalFormat, width, height);
```
