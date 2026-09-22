# 学习笔记

这里存放面向代码阅读的补充笔记；它们解释概念和后端 API，但不替代 `docs/reference/` 下的正式架构文档。

## OpenGL 后端 API 对照

本组文档以 `engine/external/cutie-rhi/src/opengl/` 的实际调用为范围。入口页给出全部 API 的分类索引；专题页解释参数、状态归属和项目中的调用时机。

| 文档 | 内容 |
|---|---|
| [opengl-api-index.md](opengl-api-index.md) | 全部已使用 API 的分类索引、源文件地图与阅读顺序 |
| [opengl-enums.md](opengl-enums.md) | Cutie RHI 描述枚举：资源、格式、绑定、状态、能力与 OpenGL 落点 |
| [opengl-resources.md](opengl-resources.md) | 缓冲区、纹理、采样器、VAO 与对象生命周期 |
| [opengl-textures.md](opengl-textures.md) | `glTexStorage*`、`GL_TEXTURE_*` 目标、mip 与像素上传 |
| [opengl-bindings.md](opengl-bindings.md) | Program、纹理单元、UBO/SSBO、image load/store 的绑定关系 |
| [opengl-draw-state.md](opengl-draw-state.md) | 绘制、间接绘制、计算派发，以及混合/深度/模板/光栅状态 |
| [opengl-framebuffer-sync.md](opengl-framebuffer-sync.md) | Framebuffer、清除与拷贝、屏障、栅栏、时间查询和调试标记 |
