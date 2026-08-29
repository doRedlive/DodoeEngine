# Render

> 深入的架构讲解(管线/RenderGraph/每帧流程/资源链路)见 [rendering/README.md](rendering/README.md)。

## 目录

`engine/src/runtime/function/render/`

| 子模块 | 关键类型 |
|---|---|
| `render_system.*` | `RenderSystem`、图形上下文、渲染帧入口 |
| `render_settings.*` | backend、pipeline、threading、present、culling 设置 |
| `render_pipeline/*` | `RenderPipeline`、`BaseRenderer`、Feature、Pass、Deferred/2D renderer |
| `render_graph/*` | `RenderGraph`、`RenderGraphBuilder`、Pass、resource、blackboard、transient pool |
| `render_view/*` | `RenderView`、`RenderViewFamily`、`RenderViewTarget`、`RenderViewport`、view extensions |
| `render_scene/*` | `RenderScene`、primitive、light、sprite scene info 与 delta |
| `mesh_draw/*` | mesh batch、processor、pass type、draw command、draw list、cache |
| `shader/*` | shader library、reflection、parameter、descriptor table、global shader/sampler |
| `material/*` | `Material`、`MaterialSystem` |
| `render_service/*` | render target、framebuffer cache、input layout cache、shared service |
| `pipeline_state/*` | PSO key、memory/disk cache |
| `gpu_driven/*` | GPU scene、culling、instance/indirect data |
| `pixel2d/*` | sprite、tileset、sprite manager |

## Deferred Renderer

`DeferredRenderer` 继承 `BaseRenderer`。它注册 `BaseSceneFeature`、`LightingFeature`、`PostProcessFeature`、`SpriteFeature`、`UIFeature`、`GizmoFeature`、`ImGuiFeature`、`PresentFeature`。

每帧流程：

```text
RenderViewFamily builds visible primitives
BaseSceneFeature prepares mesh pass contexts
CPU or GPU culling builds draw commands
Feature imports and passes are collected
RenderGraph compiles and executes
```

## Render Graph

`RenderGraph` 管理 texture/buffer/backbuffer resource record、pass dependency、topological levels、resource access validation、automatic barrier 与 pass culling。`RenderGraphPassBuilder` 创建或导入资源，并声明 read、color write、depth write、UAV write、buffer access 与 export state。

`RenderGraphBlackboard` 在 pass 之间传递以类型 key 标识的资源产品。`IRenderPass` 使用 `Produces` / `Consumes` 声明产品依赖，`RenderPassBuildContext` 提供当前 view、scene、graphics context、shared render service 和 graph imports。

## View 与 Mesh Draw

`RenderView` 保存 view/projection/view-projection matrix、viewport rect 和 extension container。`MeshViewExtension` 保存可见 primitive、mesh pass relevance、per-pass primitive indices、instance scene data、shadow matrix 与 frame time。

`BaseSceneFeature` 的 mesh processor 构建 GBuffer 与 directional-shadow 的 `MeshDrawList` 和 cached command。`MeshPassType`、`MeshPassRelevance`、`MeshDrawCommandCache` 是 mesh pass 与最终绘制命令之间的边界。

## Shader 与参数

`ShaderReflector` 读取 shader resource kind、set、binding、array/format 等信息。`ShaderParameterSet` 分为 Global、View、Pass、Material、Primitive、Bindless。`ShaderParameterBinder` 将参数 binding set 写入 graphics state。

shader resource 名称、参数 struct 成员、reflection 结果和 binding layout/set 必须一致。RenderGraph texture/buffer handle 是 graph 生命周期资源；直接 `Gfx*Handle` 是图形资源生命周期对象。
