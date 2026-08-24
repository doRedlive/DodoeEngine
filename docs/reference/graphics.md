# Graphics

## 目录

`engine/src/runtime/function/graphics/`

| 文件或目录 | 类型与职责 |
|---|---|
| `gfx.h` | 图形 API 通用类型、资源 handle、状态、pipeline 描述 |
| `gfx_context.*` | `GfxContext`、设备、swapchain、图形上下文与 backend 选择 |
| `draw_command_list.*` | `DrawCommandList`、资源状态、copy、draw、dispatch 命令记录 |
| `draw_executor.*` | 命令列表提交与执行 |
| `backend/d3d12_backend.*` | D3D12 backend |
| `backend/vulkan_backend.*` | Vulkan backend |
| `backend/opengl_backend.*` | OpenGL backend |

## 命令路径

```text
Render pass
  -> DrawCommandList
  -> DrawExecutor
  -> GfxContext / backend device
  -> backend command list and queue
```

`DrawCommandList` 记录资源状态转换、buffer/texture 写入、graphics state、compute state、draw、dispatch 和 present 前操作。`DrawExecutor` 将命令序列映射到当前 backend。

## 资源和状态

`GfxTextureHandle`、`GfxBufferHandle`、`GfxFramebufferHandle`、`GfxPipelineHandle`、`GfxBindingSetHandle` 等由图形资源层持有。资源状态通过 `GfxResourceStates` 与 `setBufferState` / `setTextureState` / `commitBarriers` 协调。

graphics state 由 viewport/scissor、pipeline、binding set、vertex buffer、index buffer、blend/depth/raster state 组成。resource state 与 graphics state 必须在发出 draw 或 dispatch 前完整提交。

## Backend

每个 backend 实现 device、queue、command list、texture、buffer、framebuffer、shader、pipeline、binding set、swapchain/present 所需的图形接口。backend 文件不保存高层 World、RenderGraph 或 Editor 状态。
