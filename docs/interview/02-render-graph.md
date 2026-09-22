# 02 RenderGraph 声明式渲染架构

对应简历条目：RenderGraph 声明式渲染架构。

## 一句话

> 基于拓扑排序的帧图渲染系统，自动完成 Pass 裁剪、GPU 资源生命周期管理、屏障推导与分层并行录制；配套 ImGui 帧图实时可视化面板，以及一条固定管线基准渲染器做正确性校验。

## 要解决的问题

手写渲染顺序时，四类错误极易发生且难以排查：

1. 资源状态切换错误（RT ↔ SRV 转换遗漏）；
2. 屏障插入错误（缺、多、粒度粗）；
3. 临时 RT 生命周期错误（提前销毁 / 忘销毁）；
4. Pass 间隐式依赖导致无法并行。

## 架构设计

代码位置：`engine/src/runtime/function/render/render_graph/`

### 两段式契约（核心）

Pass 的 `build()` 是 setup 阶段，**只声明资源读写，不碰 GPU**：

- `createTransientTexture/Buffer`、`importTexture/Buffer/BackBuffer`；
- `readTexture/readBuffer`、`writeColor/writeDepth`（同时登记附件槽与 load/clear 语义）、`writeUav`；
- `exportTexture(handle, final_state)` 标记存活根；
- `blackboard().set/get<Type>` 跨 Pass 传产物。

execute 阶段是 `addPass<TParameters>(name, flags, setup, execute)` 里的 lambda：参数结构体按值捕获，是 setup 与 execute 之间传递图句柄的通道；execute 里 `ctx.resolveTexture(handle)` 把图句柄换成真实 Gfx 句柄，再建 binding set / PSO、录命令。

**依赖分析只认 setup 期登记的 access**——execute 里私下动用未登记资源不受屏障与依赖保护，这是硬性契约（面试强调这点）。

### 编译五阶段（`RenderGraph::compile()`，render_graph.cpp）

| 阶段 | 做什么 |
|---|---|
| `buildDependencyGraph` | 按资源维护 writer/reader，建 RAW/WAW/WAR 边；边只连"最新 writer"，写时清空 reader 列表，防边数爆炸 |
| `validateAccesses` | 未初始化读、"写而未读未导出"、UAV 双写 → assert |
| `cullUnreachablePasses` | 从 exported 资源 + 后缓冲反向标记可达，不可达 Pass 裁剪（`NeverCull` 豁免）；没人消费的生产链整条裁掉 |
| `deriveBarriers` | 按访问序模拟资源状态机，为每个 Pass 生成前置 `pre_barriers`，支持子资源范围 |
| `topologicalSort` | Kahn 分层；层内可并行、层间串行；成环 assert |

### 执行（`RenderGraph::execute()`，render_graph.cpp 约 394 行）

1. `RenderGraphResourceResolver` 一次性实体化全部资源：Transient 从瞬态池 acquire、Imported 直接用句柄、BackBuffer 取交换链图。
2. 逐 level：为每个 Pass 分配独立 `DrawCommandList`，`ThreadPool::enqueue` + `WaitGroup` 并行录制（render_graph.cpp:550-591）；整层完成按序 `out_commands.append()` O(1) 拼接。
3. 每 Pass 录制序列：pre-barriers → 附件状态/clear/framebuffer → beginMarker → pass.execute → endMarker。
4. **OpenGL 后端退化为 direct_mode 串行**（单上下文不允许并行录制，见 06）。

### 瞬态资源池 `RenderGraphTransientPool`

跨帧常驻；`acquireTexture/acquireBuffer` 按 desc 全字段精确匹配复用，帧末 `releaseAll` 只清 in_use 不销毁——解决"每帧建/销毁一堆中间 RT"的分配开销。desc 变化（如 resize）自然 miss 新建。

### 与渲染管线集成

- `IRenderPass` 用 `getProducedKeys()/getConsumedKeys()` 声明 blackboard 依赖，`BaseRenderer::bakePasses` 按 `RenderPhase` 排序并校验"先生产后消费"；
- `RenderGraphImportRegistry` 是"外部 → 图"通道（GBuffer/ShadowMap/ImGui 字体纹理等），Feature 在 `registerGraphImports` 里 publish，freeze 后 pass build 期 import 进图；
- **每个 view 一张独立图**，共享同一个 out_commands 与瞬态池（`renderer.cpp` `buildOrderedPasses`）。

## 调试与可视化（简历里单独写了，要能展开）

- **ImGui 帧图面板**：`service/debug/render_graph_panel.cpp`，基于 imnodes 的节点图。每次 `graph.compile()` 后由 `RenderGraphDebug::publish` 双缓冲快照（`renderer.cpp:123`），面板展示：Pass 节点（被裁灰色、Compute 蓝、Copy 绿）、资源节点（exported 绿 / imported 紫）、writer→资源→reader 连边、pass tooltip（flags/level/barrier 数/访问列表）、资源表、自动刷新与布局重置。
- **离线导出**：`RenderGraph::dumpToJSON / dumpToDOT`（render_graph.cpp:605/634），Graphviz 有向图用于帧分析。
- **基准渲染器**：`service/reference/baseline_renderer.*`，一条完全绕开 RenderGraph 的手写路径（裸 cutie 命令列表 + 手动 setTextureState/commitBarriers），pass 顺序 GBuffer → Shadow → Lighting → Sky → Sprite → Outline → TAA → PostProcess → Present，与主路径共享同一套 shader 与光照代码。选择开关是 `RenderSettings::enable_baseline_renderer`（app_config.json），**启动时确定，不是运行时热切换**。

## 亮点与难点（讲功力用）

- 屏障是"每 Pass 前置推导"，粒度到子资源 range，不做全局排序；
- 裁剪的根只有 exported 资源与后缓冲——中间结果无人消费则整条生产链被裁，这是裁剪的价值来源；
- 并行边界清晰：层内并行录制、层间严格串行，跨层依赖经资源访问隐式表达，天然安全；
- 图级 blackboard（每 view 独立）与冻结式 ImportRegistry，避免每帧导入集合漂移。

## 高频追问

- **怎么保证屏障正确？** → setup 声明是唯一依据；execute 未登记的资源不受保护，用契约 + validate 阶段 assert 兜底。
- **为什么 OpenGL 不并行？** → GL 单上下文 + 立即式命令列表，多线程录命令会破坏上下文状态；有 direct_mode 串行分支（见 05/06）。
- **Pass 裁剪依据？** → 从导出资源反向可达标记，NeverCull 豁免。
- **跨帧复用瞬态资源行不行？** → 不行，池在 `releaseAll` 后句柄仍复用，跨帧持有是契约违规；resize 时需要 reset 池防膨胀。
- **AsyncCompute 用了吗？** → 标志位与能力探测存在，但当前没有 Pass 使用异步计算，诚实回答"留了接口未启用"。
