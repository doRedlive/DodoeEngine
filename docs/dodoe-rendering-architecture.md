Dodoe Rendering Architecture Module
1. 目标
本文档定义 Dodoe 引擎的渲染架构重构方案，目标是将当前渲染系统重构为更接近 Unreal Engine 的分层结构，做到：

场景数据与渲染执行分离
支持一个帧内多个视口、多视图渲染
支持延迟/前向混合渲染链路
引入独立的 RenderGraph 作为帧内执行图
引入类似 UE 的 Mesh Draw Command 体系，用于高效提交渲染命令
让渲染系统具备可扩展性，便于后续接入阴影、后处理、TAA、LOD、实例化等功能
本方案将现有系统重构为以下核心层级：

逻辑组件层
SceneGraph 层
RenderScene 层
ViewFamily / View 层
Mesh Draw / MDC 层
RenderGraph 层
RenderingPipeline 层
2. 总体设计原则
2.1 职责分离
SceneGraph 负责接收逻辑组件提交的渲染态数据，并承担“桥接 + 场景组织”职责
RenderScene 负责维护场景级渲染实体与全局可查询数据
RenderView 负责单个视图的相机、裁剪、viewport、uniform 数据
RenderViewFamily 负责管理一个渲染请求中的多个视图
Mesh Draw 负责将可渲染对象整理为可提交的 draw command
RenderGraph 负责帧内 pass 和资源依赖调度
RenderingPipeline 负责统筹整个渲染帧的流程
2.2 多视口优先
系统不能把“一帧渲染”等同于“一个视图”。

必须显式支持：

分屏渲染
编辑器多视口
主视口 + 预览视口
左右眼立体渲染
局部摄像机视图
因此，渲染入口应为 RenderViewFamily，其中包含一个或多个 RenderView。

2.3 UE 风格数据流
建议采用如下链路：

逻辑组件 -> SceneGraph -> RenderScene -> RenderViewFamily / RenderView -> Mesh Draw Command -> RenderGraph -> GPU

3. 核心模块说明
3.1 SceneGraph
SceneGraph 在本项目中承担 UE 中 FPrimitiveSceneProxy 所对应的职责外壳，并额外承担场景拓扑组织功能。

职责
接收组件层提交的渲染数据
维护组件与渲染对象之间的映射
管理节点层级、挂接关系、Transform、Bounds、Visibility
管理 dirty 状态与同步机制
将数据转换为 RenderScene 可消费的内部结构
语义定位
SceneGraph 不是纯粹的图结构容器，也不是单纯的 proxy；它是二者的组合体：

对外：扮演渲染桥接层
对内：扮演场景节点组织器
不应承担的职责
不直接执行渲染 pass
不直接管理 GPU 资源生命周期
不直接组织 RenderGraph 执行顺序
3.2 RenderScene
RenderScene 对应 UE 的 FScene。

职责
维护所有可渲染实体的全局注册表
维护场景级别的数据缓存
维护 Primitive / Instance / Material / Light 等场景对象信息
支持按视图查询可见对象
向渲染管线提供 scene 级数据访问能力
关键能力
注册 / 注销 render object
同步 scene graph 更新
管理 primitive scene info
提供 culling 所需的基础数据
提供 mesh draw 构建所需的 primitive 信息
3.3 PrimitiveSceneInfo
PrimitiveSceneInfo 是场景内部的运行期实体信息，类似 UE 的 primitive scene info。

职责
持有 primitive 的场景状态
绑定 bounds、visibility、LOD、material、mesh data
作为 scene 内部可查询对象参与 culling 和 draw 构建
记录当前 primitive 的 dirty 状态和运行时缓存
建议属性
ID / Handle
Owner component 引用或标识
World transform
Local bounds / World bounds
Visibility flags
Cast shadow / Receive shadow
Static mesh / dynamic mesh 标志
Material slots
Instance 数据
3.4 RenderView 与 RenderViewFamily
这是多视口支持的核心。

RenderView
RenderView 对应 UE 的 FSceneView，表示单个视角。

职责：

保存相机参数
保存 view / projection / view-proj 矩阵
保存 viewport rect
保存 frustum 和裁剪结果
保存 per-view uniform 数据
保存本视图的后处理参数
RenderViewFamily
RenderViewFamily 对应 UE 的 FSceneViewFamily，表示一次渲染请求中的视图集合。

职责：

组织一帧内的多个 RenderView
保存共享的 family 级参数
保存时间、曝光、环境、场景全局参数
为 RenderGraph 和 Pipeline 提供统一入口
典型场景
分屏 2 人游戏：1 个 family，2 个 view
编辑器四视口：1 个 family，4 个 view
主视口 + 小地图：1 个 family，2 个 view
VR 双眼：1 个 family，2 个 view
3.5 Mesh Draw / MDC
MDC 是最终提交给渲染 pass 的核心中间层，参考 UE 的 Mesh Draw Command 体系。

职责
从 PrimitiveSceneInfo 中提取可绘制数据
按 mesh batch / material / pass / pipeline state 组织命令
支持静态命令缓存与动态命令构建
为每个 view 生成可执行的 draw commands
为渲染排序、合批、状态复用提供基础
推荐结构
MeshBatch：描述网格批次数据
MeshPass：按渲染阶段划分的命令集合
MeshDrawCommand：单条最终提交命令
MeshPassProcessor：将 batch 转换为 draw command
要求
不能直接从逻辑组件提交到 GPU
不能直接由 RenderGraph pass 手写 draw call
应通过 MDC 层统一进入 pass
3.6 RenderGraph
RenderGraph 是新的帧内执行图，不是现有旧式 render graph 的简单改名，而是独立的图式调度层。

职责
管理 pass 的依赖关系
管理 texture / buffer / attachment 的生命周期
处理资源读写关系和转换
按依赖执行渲染任务
组织前向、延迟、后处理、UI 等阶段
核心特性
Graph Resource
Graph Pass
Dependency Resolve
Resource Transition
Automatic Lifetime Management
使用原则
RenderGraph 只描述“如何执行”
不持有逻辑场景对象
不负责组件生命周期
不负责 mesh 数据组织
3.7 RenderingPipeline
RenderingPipeline 是总调度器，负责统筹整条渲染链路。

职责
接收 frame render request
构建 RenderViewFamily
组织多个 RenderView
执行 view culling
调用 Mesh Draw 构建 MDC
构建 RenderGraph
排列并执行渲染 pass
推荐流程
收集本帧渲染请求
构建 ViewFamily
为每个 View 初始化相机和 viewport
执行可见性裁剪
构建每个 View 的 MDC
通过 RenderGraph 组织渲染 pass
执行图并输出最终结果
4. 推荐调用链路
4.1 逻辑层到场景层
组件状态变化
组件提交变更到 SceneGraph
SceneGraph 更新内部节点和标记 dirty
SceneGraph 同步到 RenderScene
RenderScene 更新 PrimitiveSceneInfo
4.2 场景层到视图层
RenderingPipeline 创建 RenderViewFamily
为 family 添加一个或多个 RenderView
每个 View 计算自己的相机、viewport、frustum
每个 View 从 RenderScene 中执行 culling
4.3 视图层到绘制层
根据可见 primitive 列表构建 MeshBatch
MeshPassProcessor 生成 MeshDrawCommand
为不同 pass 组织 MDC
将 MDC 交给 RenderGraph pass 执行
4.4 绘制层到执行层
RenderGraph 创建渲染资源和 pass
pass 读取和写入图资源
图系统决定执行顺序
输出到 backbuffer 或中间目标
5. 多视口设计要点
5.1 不能把单视图当成单帧
多个视口必须共享同一个 frame context，不能各自独立驱动整条渲染链路。

5.2 共享与独立边界
共享给 ViewFamily 的内容
时间参数
环境参数
Frame 级别的全局资源
一次性共享的中间结果
独立给每个 View 的内容
Camera
Viewport Rect
Frustum
Visibility
Per-view uniform
Per-view post process 参数
5.3 视图级 culling
每个 View 应独立执行裁剪，避免不同视口之间互相污染可见性结果。

6. 数据模型建议
6.1 SceneGraph 节点数据
Node ID
Parent / Children
Transform
Bounds
Dirty Flags
Component Handle
Scene Info Handle
6.2 PrimitiveSceneInfo 数据
Primitive ID
Owner handle
World transform
Visibility flags
Material references
Mesh references
Bounds
LOD info
Instance info
6.3 RenderView 数据
View ID
Camera position / rotation
View matrix
Projection matrix
View-proj matrix
Viewport rect
Frustum planes
Exposure
Post process params
6.4 RenderViewFamily 数据
Family ID
Views list
Frame time
Delta time
Global environment params
Shared render settings
6.5 Mesh Draw 数据
MeshBatch
Material ID
Pipeline state
Pass type
Sort key
Instance count
Draw parameters
7. 渲染阶段建议
建议将渲染阶段拆成以下链路：

Depth Prepass
Base Pass
Deferred Lighting 或 Forward Lighting
Shadow Pass
Translucency Pass
UI Pass
Post Process Pass
如果当前项目主要采用延迟 + 前向混合渲染，则建议：

几何阶段走统一基础 pass
光照阶段通过 deferred 处理主要光照
特殊透明对象、特殊材质、UI 通过 forward 补充
8. 推荐目录结构
建议新增如下目录：

推荐职责分布如下：

render_scene/：场景实体管理
scene_graph/：组件桥接、节点组织、dirty 同步
render_view/：单视图数据与裁剪结果
render_view_family/：多视图管理
mesh_draw/：MeshBatch / MDC / Processor
render_graph/：图资源与 pass 调度
rendering_pipeline/：渲染统筹入口
9. 迁移计划
Phase 1：建立新边界
明确 SceneGraph 的桥接职责
明确 PrimitiveSceneInfo 的内部实体职责
明确 RenderViewFamily 与 RenderView 的区别
新建独立 render_graph 和 mesh_draw 模块
Phase 2：拆分当前渲染逻辑
将场景管理逻辑从渲染执行逻辑中分离
将视口相关逻辑从 pipeline 中抽离到 view / family
将 draw call 生成从 pass 中抽离到 mesh draw 层
Phase 3：接入 RenderGraph
用 graph pass 替换手写的渲染步骤
所有中间资源通过 graph 统一管理
调度顺序完全依赖图关系
Phase 4：接入 MDC
将 primitive 数据转换为 mesh batch
再转换为 mesh draw command
由 pass 消费 MDC，而不是直接消费组件数据
Phase 5：逐步替换旧实现
保留旧逻辑作为过渡
每次只迁移一个功能域
等新链路稳定后，再删除旧实现
10. 关键接口草案
以下为建议的接口方向，具体命名可按项目风格调整。

SceneGraph
RegisterComponent(...)
UnregisterComponent(...)
UpdateComponentTransform(...)
MarkDirty(...)
SyncToRenderScene(...)
CollectVisiblePrimitives(...)
RenderScene
AddPrimitive(...)
RemovePrimitive(...)
UpdatePrimitiveInfo(...)
GetPrimitiveSceneInfo(...)
QueryPrimitivesByView(...)
RenderViewFamily
AddView(...)
GetViews()
SetSharedFrameParams(...)
RenderView
SetCamera(...)
SetViewportRect(...)
UpdateMatrices(...)
BuildFrustum(...)
CullVisiblePrimitives(...)
Mesh Draw
BuildMeshBatches(...)
BuildMeshDrawCommands(...)
SortCommands(...)
SubmitPassCommands(...)
RenderGraph
AddPass(...)
CreateTexture(...)
CreateBuffer(...)
BuildDependencies(...)
Execute(...)
RenderingPipeline
BeginFrame(...)
BuildViewFamily(...)
CullScene(...)
BuildDrawCommands(...)
BuildRenderGraph(...)
Execute(...)
11. 设计约束
11.1 不要让 SceneGraph 变成“万能对象”
SceneGraph 只承担桥接与组织，不承担整个渲染执行生命周期。

11.2 不要让 RenderViewFamily 退化成一个简单数组
它必须持有共享 frame 数据，并成为渲染请求的统一入口。

11.3 不要让 RenderGraph 直接认识逻辑组件
RenderGraph 只面向图资源和 pass，不面向业务实体。

11.4 不要让 draw command 生成散落在各个 pass 内
所有 MDC 构建必须统一进入 mesh draw 层。

12. 最终目标形态
最终系统应达到以下状态：

逻辑组件通过 SceneGraph 进入渲染系统
RenderScene 管理场景级渲染实体
一个 Frame 对应一个 RenderViewFamily
一个 RenderViewFamily 中可以有多个 RenderView
每个 View 独立裁剪和生成可见集
Mesh Draw 层统一生成 MDC
RenderGraph 统一调度 pass 和资源生命周期
RenderingPipeline 只负责全局统筹
这套结构可以让项目在保持清晰职责边界的同时，逐步演进到 UE 风格的渲染组织方式。

13. 后续建议
下一步建议优先完成以下三项：

定义 RenderViewFamily 与 RenderView 的数据结构
重构 SceneGraph，让它承担桥接与组织职责
抽出 mesh_draw 和新的 render_graph 模块
完成这三项后，整个渲染架构就会进入可持续重构状态。