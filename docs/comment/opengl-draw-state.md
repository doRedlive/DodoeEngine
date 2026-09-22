# OpenGL 绘制、计算与固定状态对照

## 1. 三类绘制调用

| API | 数据来源 | 实例支持 | 特殊基址 |
|---|---|---|---|
| `glDrawArrays` | 顶点按顺序从 VAO 读取 | 否 | 无 |
| `glDrawArraysInstanced` | 同上 | `instanceCount` | 无 |
| `glDrawArraysInstancedBaseInstance` | 同上 | 是 | 指定 `baseInstance` |
| `glDrawElements` | 先从 VAO 的索引缓冲取索引 | 否 | 无 |
| `glDrawElementsInstanced` | 索引绘制 | 是 | 无 |
| `glDrawElementsInstancedBaseVertexBaseInstance` | 索引绘制 | 是 | 同时偏移顶点索引和实例编号 |

`glDrawArrays*` 的 `first` 表示首个顶点；`glDrawElements*` 的 `indices` 在有 element buffer 时是索引数据的字节偏移，并不是 CPU 指针。

## 2. 间接绘制：参数也放在 GPU

| API | 作用 |
|---|---|
| `glMultiDrawArraysIndirect` | 从 `GL_DRAW_INDIRECT_BUFFER` 连续读多条非索引绘制参数 |
| `glMultiDrawElementsIndirect` | 从同一类缓冲读多条索引绘制参数 |
| `glMultiDrawElementsIndirectCount` | 绘制条数也由 GPU 缓冲决定 |

它们与此前的 `isDrawIndirectArgs → GL_DRAW_INDIRECT_BUFFER` 正好对应。GPU 可先通过计算着色器生成可见物体的绘制参数，再直接发起多条绘制，CPU 无须逐对象提交。

## 3. 计算派发

| API | 作用 |
|---|---|
| `glDispatchCompute(x, y, z)` | 直接指定三个维度的工作组数量 |
| `glDispatchComputeIndirect(offset)` | 从 `GL_DISPATCH_INDIRECT_BUFFER` 的指定字节位置读取工作组数量 |

工作组数量不是线程数量；总调用数量还要乘以 GLSL 中 `local_size_x/y/z`。

## 4. 渲染区域与颜色输出

| API | 作用 | 备注 |
|---|---|---|
| `glViewport` | NDC 到窗口/附件像素区域的映射 | 通常覆盖当前渲染目标 |
| `glScissor` | 限制实际可写入的矩形 | 需启用 `GL_SCISSOR_TEST` |
| `glDepthRange` | 将 NDC 深度映射到指定范围 | 项目跟随 viewport 一起设置 |
| `glEnablei` / `glDisablei` | 单独开关第 N 个颜色附件能力 | 项目用于独立混合 |
| `glBlendColor` | 设置常量混合颜色 | 仅当混合因子使用常量颜色时生效 |
| `glBlendFuncSeparatei` | 为颜色与 alpha 分别设置源/目标混合因子 | `i` 是颜色附件编号 |
| `glBlendEquationSeparatei` | 分别设置颜色与 alpha 的混合运算 | 如 add、subtract |
| `glColorMaski` | 控制某个颜色附件的 RGBA 写入开关 | 不影响深度/模板 |

## 5. 深度、模板与光栅化

| API | 作用 |
|---|---|
| `glEnable` / `glDisable` | 开关深度、模板、剔除、混合、scissor、polygon offset 等能力 |
| `glDepthMask` | 开关深度缓冲写入；不等于深度测试开关 |
| `glDepthFunc` | 指定新片元和已有深度值如何比较 |
| `glStencilFuncSeparate` | 正面和背面各自设置模板比较规则与参考值 |
| `glStencilMaskSeparate` | 正面和背面各自设置模板写掩码 |
| `glStencilOpSeparate` | 为模板失败、深度失败、通过分别设置操作 |
| `glCullFace` | 选择剔除正面、背面或两面 |
| `glFrontFace` | 指定顺时针或逆时针为正面 |
| `glPolygonMode` | 选择点、线或填充方式 |
| `glPolygonOffset` | 为深度值加与斜率相关的偏移，缓解共面闪烁 |

项目在 `applyGraphicsState` 中集中设置这些状态。它们是 OpenGL 上下文状态，不属于单个 mesh；因此两个 draw 之间只要状态不同，就需要重新设定。
