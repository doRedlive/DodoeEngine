DoDoE 渲染基础设施评审与演进路线图
更新时间：2026-07-17
范围：engine/src/runtime/function/render、function/graphics 及其线程、资源、着色器基础设施。
性质：架构评审和实施路线图；不代表本次已经修改代码。
1. 结论
DoDoE 的渲染架构已经有一条正确且现代化的主干：跨 API RHI、RenderScene、ViewFamily、Mesh Pass、延迟管线、RenderGraph、PSO cache、bindless/GPU-driven 的雏形都已存在。其形态更接近 UE 的渲染线程架构，而非简单的“ECS 直接调用 draw”。
当前最大风险不是缺少某个画面特性，而是基础设施尚未形成完整闭环：命令内存所有权、帧生命周期、RenderGraph 资源编译、GPU Scene 增量同步和 capability 管理之间仍有断点。应先处理这些正确性与可观测性问题，再扩展 async compute、GPU culling、HZB、clustered lighting 等能力。
当前架构（实态）
World / ECS systems
    │ RenderCommandQueue
    ▼
RenderSystem
    │ consume RenderCommand / flush RenderScene
    ▼
RenderScene ──────────────► GpuScene（设计上应增量同步）
    │
    ▼
RenderViewport → RenderViewFamily → RenderView extensions（可见性、draw 数据）
    │
    ▼
RenderPipeline → DeferredRenderer / Only2DRenderer
    │               │ Mesh processors + features
    ▼               ▼
RenderGraph（依赖排序、CPU 并行录制）
    │
    ▼
DrawCommandList → GfxContext / cutie RHI → DX12 / Vulkan / OpenGL
与 UE / Unity 的位置对齐
DoDoE
接近的 UE 概念
接近的 Unity 概念
评价
RenderCommandQueue + RenderThread
game-to-render command / render thread
main thread 到 render loop 的提交边界
方向正确，需要把增量数据做成 frame snapshot
RenderScene、PrimitiveSceneInfo
FScene、FPrimitiveSceneProxy
renderer-visible scene / culling data
已有代理化，适合继续走 SoA / GPU Scene
RenderViewFamily、RenderView
FSceneViewFamily、FSceneView
camera stack / culling view
模型正确，需支持统一多视图 frame graph
MeshPassType + processors
Mesh Draw Commands / pass processors
SRP draw list + renderer list
方向正确，仍有较多 pass 专用拼装
RenderGraph
RDG
RenderGraph
当前是 DAG 调度器，尚不是完整资源编译器
DrawCommandList
RHI command list
command buffer recording
必须先修所有权、arena 与线程隔离
GfxContext + cutie
RHI / device abstraction
GraphicsDevice / native backend
多后端骨架已有，能力模型不足
2. 现有优势：应该保留并强化的设计
1. 渲染场景与游戏世界分离。 ECS 经 RenderCommandQueue 更新 RenderScene，避免渲染阶段直接遍历 World。这是后续多线程、编辑器多视口、GPU Scene 的必要前提。
2. View-family 模型明确。 Viewport 能构建 RenderViewFamily，并能覆盖相机矩阵；这对 Scene/Game 双视图、反射/阴影视图、截图和离屏目标很重要。
3. 管线由 renderer 组合 feature。 DeferredRenderer 组合 BaseScene、Lighting、PostProcess、Sprite、ImGui、Present feature，Only2DRenderer 走更轻路径。这比 pass 注册散落在系统中更可维护。
4. Mesh pass 与 draw command 已经解耦。 MeshPassRelevance、MeshPassProcessor、MeshDrawCommandDispatcher 是可扩展的 draw submission 基础，可承接 sorting、instancing、GPU-driven。
5. 有 RenderGraph 和 PSO cache 的起点。 依赖图、黑板和 graphics pipeline key 都已出现，避免以后从线性 pass 链重构。
6. 后端抽象不是空壳。 GfxContext 覆盖 DX12/Vulkan/OpenGL，swapchain 包装、资源状态和 validation 入口已具备。
3. P0：先修正确性与生命周期
这些项目应在扩展效果或 GPU-driven 前完成。每项都可能造成未定义行为、功能静默失效或设备相关的偶现错误。
P0.1 命令列表的所有权与帧内存模型
现状和证据
- DrawCommandList::append(DrawCommandList&& other) 仅连接 other.m_head/m_tail，但没有清空 other。
- 图的非 immediate 路径为同一 level 创建 pass_command_lists，并在任务完成后 append 到 out_commands；局部数组析构时会再次 reset 这些命令。
- DrawCommandList::reset() 会调用全局 Memory::ResetFrame()；而 Memory 只有一个进程级 s_frame_allocator。
涉及文件：
- function/graphics/draw_command_list.cpp:68-80, 88-102
- function/render/render_graph/render_graph.cpp:216-242
- core/memory/memory.cpp:9, 69-72
风险
- append 后源列表析构可能再次析构/释放已经归属输出列表的命令；
- worker 线程同时从同一 frame allocator 分配或 reset，存在线程安全和寿命竞态；
- triple-thread 下 CPU 可以开始构建下一帧，而 draw thread 仍在消费上一帧的命令存储；
- 任意列表 reset 都可能回收另一个列表仍在使用的命令内存。
目标设计
引入 FrameContext-owned command arena，而不是全局 frame allocator：
FrameSlot[N]（N = frames in flight）
  ├─ FrameFence / completion value
  ├─ UploadArena
  ├─ TransientResourceArena / pool context
  └─ CommandArena
       ├─ Render-thread chunk
       └─ Worker-thread chunks（每 worker 独立）
- DrawCommandList 只拥有一个 command chunk 的链表，不负责重置全局内存；
- append(std::move(other)) 必须是严格 move：输出接管链表，other 置空；
- 单个 FrameContext 完成 GPU fence 前不得复用其 arena；
- 多 worker 使用独立 allocator/chunk，汇总只做链表拼接；
- 资源创建、上传和 draw command 均必须明确属于哪一个 frame slot；
- 只有 frame completion 后由 frame scheduler 统一 reset/recycle。
验收
- AddressSanitizer / PageHeap 下，连续运行多视口和 resize 不出现 UAF、double free；
- TripleThread 下 10,000 帧稳定运行，command 数、内存峰值与 frame slot 可观测；
- 任何 worker 的命令内存不会被其他 worker/上一帧 reset；
- immediate 与 deferred 路径共用资源所有权规则，但不共享可变全局列表。
P0.2 GPU Scene dirty 更新在清空后被消费
现状和证据
RenderScene::rebuildPipelineSceneData() 先更新 CPU scene info，随后执行：
m_pending_primitive_updates.clear();
m_pending_sprite_updates.clear();
// 后面才遍历 m_pending_sprite_updates 更新 m_gpu_scene
因此 GPU Scene 处理循环拿到的是空集合，sprite 的注册、变换/bounds、instance 数据上传都不会执行。
文件：function/render/render_scene/render_scene.cpp:439-519，清空发生在 453-454，GPU Scene 循环从 458 开始。
修复原则
- 在 flush 的开始将 pending updates move 到局部 RenderSceneDelta；CPU Scene 和 GPU Scene 都消费这一个 delta；
- 所有消费者完成后再丢弃 delta；
- 不要让 RenderScene 的可变 pending map 成为多个阶段依赖的隐式输入；
- GPU object handle 要带 generation，避免删除后 slot 被复用造成旧命令命中新对象。
验收
- 添加/删除/移动 sprite 时，GPU object count、dirty range、上传字节数均按预期变化；
- 无修改帧不会提交 GPU Scene 上传；
- 删除再新增同 UUID/slot 时，旧帧不会绘制新对象的数据。
P0.3 GPU-driven capability 永远不会启用
现状和证据
RenderSystem::initialize() 创建 GfxContext 时把 enable_gpu_driven 固定传为 false。GfxContext 只在该字段为真时查询 bindless 与 compute queue 支持，所以 RenderSettings::IsGpuDrivenSupported() 默认始终为 false。
文件：
- function/render/render_system.cpp:32
- function/graphics/gfx_context.cpp:58-62
目标设计：能力、配置、使用策略分离
DeviceCapabilities（后端真实能力）
       +
RenderFeatureSettings（项目/用户是否允许）
       +
PipelineRequirements（当前功能是否需要）
       ↓
ResolvedRenderFeatures（本帧实际启用，含 fallback 原因）
不要把 capability 写进全局静态开关作为唯一真相。至少分别表达：
- hardware/API support；
- requested/disabled by project setting；
- pipeline-ready（descriptor layout、shader variant 是否存在）；
- runtime fallback reason（例如 bindless unavailable）。
验收
- 启动日志打印每个 backend 的 feature matrix；
- DX12/Vulkan 支持设备可显式开启 GPU-driven；不支持的设备稳定走 CPU fallback；
- editor 面板能显示“未启用”的具体原因，而不是只有 bool。
P0.4 明确帧提交、present 与 swapchain resize 的序列
当前 RenderSystem 负责 acquire，render/draw thread 最终 present；resize 会在 render frame 起始处立即 recreateSwapchain()。当 draw thread 仍在提交旧 swapchain image 时，应明确保证其 fence 已完成，否则存在跨线程 resize 生命周期风险。
目标：
- 将 acquire / render submit / present / resize 统一纳入 FrameScheduler；
- resize 进入 pending 状态，停止新的 acquire，等待所有引用旧 swapchain 的 in-flight frame 完成后再重建；
- device lost、minimized window、out-of-date/suboptimal 均有统一状态机；
- RenderViewport 不直接决定 GPU 生命周期，只产生 resize request。
4. P1：把 RenderGraph 升级为资源编译器
4.1 当前能力与缺口
现有 RenderGraph 会记录 Read/Write 访问、建立 producer/reader 依赖、拓扑分 level，并在非 immediate 模式下并行录制同 level pass。这是“CPU DAG scheduler”的良好起点。
但它还没有负责现代 RenderGraph 最关键的资源编译工作：
能力
当前情况
目标
依赖排序
已有 producer/read/write 排序
保留，并增加 validation
资源状态
pass 手写 setTextureState / commitBarriers
graph 根据访问声明生成 transition/UAV barrier
子资源粒度
无
mip、array slice、aspect、buffer range
transient 资源
execute 时逐个 create
按生命周期池化、aliasing、frame budget
pass culling
仅 cull 无访问且非 NeverCull 的 pass
从 exported/present roots 反向可达性剔除
queue 调度
AsyncCompute 仅是 flag
graphics/compute/copy queue batch + fence/wait
raster attachment
隐含在 execute lambda 中
attachment、load/store/clear 显式声明
调试
marker
graph dump、barrier、资源寿命、显存预算
涉及：render_graph_resource.h、render_graph_pass.h、render_graph.cpp、render_graph_resource_registry.cpp。
4.2 建议的资源访问 API
将 Read/Write 两态扩展为显式 usage：
builder.readTexture(input, RGTextureAccess::SampledPixel);
builder.writeColor(output, LoadOp::Clear, StoreOp::Store);
builder.writeDepth(depth, DepthAccess::Write);
builder.readBuffer(instances, RGBufferAccess::Vertex);
builder.writeBuffer(visibleList, RGBufferAccess::UAV);
builder.exportTexture(backbuffer, RGFinalState::Present);
每次访问应包含：
- read/write/read-write；
- pipeline stage；
- RHI resource state；
- texture subresource range 或 buffer range；
- raster attachment load/store/clear；
- 是否跨 queue、是否允许 async compute。
pass execute 函数应只录制 draw/dispatch，不再手写通常的资源 transition。特殊 barrier 可保留低级逃生口，但必须可审计。
4.3 编译阶段建议
Register passes/resources
  → validate（未初始化 read、重复 write、格式/usage 不匹配）
  → build dependencies
  → root-based culling
  → topological order
  → derive resource lifetimes
  → allocate/alias transient resources
  → derive barriers and queue synchronization
  → form command batches
  → execute / submit batches
4.4 实施顺序
1. 先加入资源 state/usage 声明和 debug validation，仍允许 pass 手写 barrier 作为对照；
2. 自动生成同一 graphics queue 的 transitions；
3. 引入 transient resource pool 与 lifetime 统计，不急于 aliasing；
4. 实现 export root 与真正的 pass culling，逐步移除大量 NeverCull；
5. 再实现 compute/copy queue batch 与 async compute；
6. 最后做 aliasing、render pass merge、memory budget pressure。
4.5 验收
- 新 pass 不需要手写常规 state transition；
- validation 能报出“读取未定义资源”“同一 subresource 冲突写入”等错误；
- 连续帧 transient texture/buffer 创建数量趋近稳定；
- dump 可以展示每个资源的 first/last use、物理分配、barrier 和队列归属；
- 同一画面在 validation backend 下无 resource-state warning。
5. P1：渲染线程、场景 delta 与 frame snapshot
当前问题
RenderCommandQueue 已把 Game Thread 与 RenderScene 隔开，但 RenderScene 更新、资源创建、GPU Scene 上传仍会直接使用 GDrawCommandList。全局列表让 Game/Render/Draw 线程间的所有权边界不够清晰。
目标职责划分
执行域
只做什么
不做什么
Game thread
ECS、脚本、生成 immutable RenderSceneDelta
访问 GPU 资源、修改渲染 snapshot
Asset/IO worker
读取、解码、shader compile、产出 upload request
调用 graphics API
Render thread
消费 delta、生成 scene snapshot、构建 graph、录制命令包
等待 GPU 空闲（除 shutdown/recovery）
Submit/Draw thread
提交封闭命令包、present、推进 fence、回收 frame slot
改写 RenderScene
Scene delta 建议
用批量不可变数据替换逐对象命令依赖：
struct RenderSceneDelta {
    FrameNumber source_frame;
    Array<PrimitiveAddOrUpdate> primitives;
    Array<PrimitiveRemove> removed_primitives;
    Array<SpriteAddOrUpdate> sprites;
    Array<SpriteRemove> removed_sprites;
    Array<LightUpdate> lights;
};
初期可继续由现有 RenderCommand 累积生成 delta，不需要一次重写 ECS。收益是：一次消费、可统计、可丢弃过期 transform 更新、可复现捕帧。
帧延迟策略
- 使用明确的 frames_in_flight，默认 2 或 3；
- Game→Render 与 Render→Submit 都使用有界队列，满时选择阻塞、丢弃可合并更新或降低帧数；
- 保持 input/transform 的时间戳，避免 editor camera 在多帧延迟下抖动；
- 所有资源销毁进入 deferred deletion queue，带 submit/fence completion value。
6. P1：资源、上传与 descriptor 基础设施
6.1 资源生命周期
当前 TextureManager::loadTexture() 是同步加载并直接录制创建/上传命令。短期简单，长期会把 IO、解码与渲染线程耦合，且大纹理/模型会造成卡顿。
建议资源状态机：
Unloaded → LoadingIO → Decoding → WaitingForUpload
         → Resident → Evicting / Failed
- IO/解码不碰 Gfx*；
- render thread 把 upload request 放进当前 frame 的 upload ring；
- upload 完成 fence 前资源为 PendingResident，材质采样 fallback texture；
- 纹理至少预留 mip generation、streaming、eviction budget 接口。
6.2 Upload ring
建设 per-frame upload allocator：
- 常量 buffer、动态 instance buffer、纹理初始数据统一从 upload ring 分配；
- 支持对齐规则、copy queue（未来）、fence 回收；
- 不要为每次 writeBuffer 临时创建不可追踪资源；
- 上传字节数、stall、overflow 必须可统计。
6.3 Bindless descriptor table
DescriptorTableManager 已实现纹理 SRV 表和扩容。下一步应补：
- descriptor index 采用 generation/indirection，避免释放后旧 GPU 数据指向新资源；
- 明确 descriptor 写入发生在哪个线程和哪一帧；
- 引入 sampler、buffer SRV/UAV 的扩展规划；
- capacity、碎片、扩容次数和 fallback 使用率的 telemetry；
- 不支持 bindless 时，保留 material binding-set fallback，而不是仅关闭一整条功能。
7. P1：Shader、Material 和 PSO 闭环
7.1 当前状态
ShaderLibrary 按后端文件扩展名硬编码加载一组 shader。它可作为 bootstrap，但会限制材质、变体和热重载的发展。
7.2 目标能力
1. Shader manifest：源文件、entry point、stage、include、permutation domain、目标平台；
2. 反射元数据：CBV/SRV/UAV/sampler、push constant、vertex input，自动生成/校验 binding layout；
3. 材质模板和实例：模板定义 shader/permutation/render state，实例仅保存参数和纹理；
4. PSO cache：内存 cache 保留，增加异步预热、disk cache、shader revision 失效；
5. 热重载：编译失败继续使用旧版本，editor 展示错误；成功后只精准失效相关 PSO；
6. 变体控制：限制 permutation explosion，优先 runtime uniform / specialization constant，按平台收集使用集。
7.3 PSO 的验收指标
- 首帧和切场景的 PSO miss/编译耗时可统计；
- 常用场景预热后不在 render thread 同步编译 PSO；
- shader hot reload 不导致全局 PSO cache 清空；
- binding 变更可由反射报错，而非运行时黑屏。
8. P2：GPU-driven 的正确扩展路径
GPU-driven 不是单一 compute culling pass，而是一条从 Scene 数据布局、可见性、draw grouping 到 indirect arguments 的连续数据流。
推荐阶段
1. CPU-driven baseline：稳定的 CPU frustum culling、按 material/mesh/PSO 排序、instancing；
2. GPU Scene：统一 object metadata、transform、bounds、instance/material data；修复增量上传；
3. GPU frustum culling：输入真实 object count，而不是固定 kMaxObjects；输出 compact visible list；
4. indirect args build：按 mesh/material/pass 分桶，生成有效的 DrawIndexedIndirectArguments；
5. HZB occlusion：引入 depth pyramid、history 与 camera-cut 处理；
6. LOD/meshlet/mesh shader：仅在数据和 profiling 显示值得时推进。
当前实现要避免的误区
- kMaxObjects = 4096 只能是临时容量，不应成为 dispatch object count；
- culling 输出 visible list 后必须被 draw pass 消费，否则只是额外成本；
- indirect args 从 UAV 切换为 indirect argument 时需要 graph 管理的同步；
- GPU-driven 与 CPU-driven 必须结果等价并能通过 debug toggle A/B 比对；
- 不应把“支持 compute queue”直接等同于“应启用 async compute”。
9. 多视图、编辑器和 presentation
现在每个 RenderView 会单独构建并执行 graph。它能工作，但随着 Scene/Game viewport、shadow view、reflection probe、thumbnail、picking 出现，重复资源和跨 view 依赖会变得难以管理。
演进目标：一个 FrameGraph 包含多个 view subgraph。
FrameGraph
  ├─ shared uploads / GPU Scene update
  ├─ shadow views（跨 camera 可共享）
  ├─ Scene viewport subgraph
  ├─ Game viewport subgraph
  ├─ picking / editor overlays
  └─ per-swapchain presentation
原则：
- RenderViewport 定义目标、尺寸、camera source、output，不直接拥有渲染全局状态；
- picking、debug draw、gizmo 都是 feature/pass，不能绕过 graph 直接改 backbuffer；
- 使用 render resolution 与 display resolution 分离，为 dynamic resolution、TAA/FSR 类能力预留位置；
- swapchain 只是一个 imported/external output，editor 离屏 viewport 也应走同一套 graph 契约。
10. 可观测性：UE/Unity 体验差距的主要来源
建议在 P0 完成后尽早加入。没有指标就很难判断 GPU-driven、并行录制或资源池是否真的收益。
必备数据
类别
最小指标
CPU
Game/Render/Submit thread 时间，graph build/compile/record 时间，queue wait
GPU
每 pass timestamp，graphics/compute/copy queue overlap，present wait
Draw
draw/dispatch 数，triangle、instance、state/PSO/descriptor bind 变化数
RenderGraph
pass count、culled pass、barrier count、transient physical allocations、alias 节省
Memory
frame arena、upload ring、transient pool、GPU Scene、descriptor capacity/fragmentation
Shader/PSO
shader compile、PSO hit/miss、预热覆盖率、hot reload 失败
工具输出
- 每帧可导出的 graph JSON/DOT；
- runtime overlay：frame slot、in-flight count、GPU memory、resource upload；
- RenderDoc/PIX marker 命名与 graph pass 一致；
- deterministic capture：保存 scene delta、settings、camera、shader revision，便于复现。
11. 分阶段实施计划
Phase R0：正确性封口（最高优先级）
[] DrawCommandList::append 实现真正的 move 所有权转移；
[] 去除 DrawCommandList::reset() 对全局 Memory::ResetFrame() 的隐式影响；
[] 引入按 frame slot + worker 分隔的 command arena；
[] 修复 GPU Scene 在清空 pending update 后才消费的逻辑；
[] 使 GPU-driven 支持开关可由配置启用，并记录 capability/fallback；
[] resize/present 与 in-flight frame fence 同步；
[] 增加 TSAN/ASAN（或 Windows PageHeap）压力测试场景。
完成标准： triple-thread、多 viewport、频繁 resize 下稳定；GPU Scene 的增量上传可验证；无全局命令内存 reset 竞态。
Phase R1：Frame infrastructure
[] FrameScheduler + N 个 FrameSlot；
[] upload ring、deferred deletion queue、fence completion；
[] RenderSceneDelta / immutable scene snapshot；
[] 资源创建和销毁不再隐式依赖 GDrawCommandList；
[] frame/debug telemetry 基础面板。
完成标准： CPU 可领先 GPU N 帧但不越界复用资源；加载纹理不阻塞渲染主路径。
Phase R2：RenderGraph resource compiler
[] state/stage/subresource access 声明；
[] 自动 graphics queue barrier；
[] export root 与真正 culling；
[] transient pool 和 lifetime dump；
[] graph validation 与 marker/JSON dump。
完成标准： 常规 pass 不再手写 barrier；资源生命周期和峰值显存可解释。
Phase R3：Shader/material/PSO
[] manifest + reflection；
[] material template/instance；
[] shader hot reload；
[] PSO async warmup + disk cache；
[] non-bindless fallback。
完成标准： 新增材质/变体不需要改 ShaderLibrary 的硬编码成员；场景切换没有可见 PSO 编译卡顿。
Phase R4：GPU-driven 与多队列
[] stable GPU Scene；
[] culling → indirect args → draw 的完整数据闭环；
[] HZB/occlusion（可选）；
[] copy/compute queue batching 和真正 async compute；
[] 多 view 统一 FrameGraph。
完成标准： CPU/GPU 路径可 A/B 对比，GPU path 在目标场景的 CPU 提交成本有量化下降，且不会恶化 GPU 时间。
12. 代码约束（后续实现时必须遵守）
1. 不新增又一个全局可变 command list 或全局 frame allocator。
2. setup 阶段声明数据与资源依赖；execute 阶段只录制命令；submit 阶段只提交封闭包。
3. 每个跨线程对象必须写清创建线程、可读线程、销毁条件和 fence 所有者。
4. GPU resource 的销毁不能依赖 CPU ref count 立即发生，必须满足 GPU completion。
5. RenderGraph pass 不允许隐式读写未声明的 transient resource；外部资源必须 import，最终输出必须 export/present。
6. GPU-driven 永远保留可验证的 CPU fallback；先保证等价，再优化性能。
7. 每项性能基础设施改动至少附带一个指标、一个压力场景和一个 fallback 验证路径。
13. 建议的近期切入顺序（按文件）
1. function/graphics/draw_command_list.*、core/memory/memory.*：所有权和 arena；
2. function/render/render_scene/render_scene.cpp：先修 delta 消费顺序；
3. function/render/render_system.*、core/thread/draw_thread.*、function/graphics/gfx_context.*：FrameScheduler 与 resize/fence；
4. function/render/render_graph/*：usage、barrier、resource pool；
5. function/render/framework/*：resource upload、shader/material/PSO；
6. function/render/gpu_driven/*：基于稳定 GPU Scene 完成 cull-to-indirect 闭环。
附录：本评审引用的当前实现位置
- 顶层帧循环和 threading mode：function/render/render_system.cpp
- 延迟 renderer 与 feature 组合：function/render/render_pipeline/deferred_renderer.cpp
- RenderGraph 编译/并行执行：function/render/render_graph/render_graph.cpp
- RenderGraph 资源 registry：function/render/render_graph/render_graph_resource_registry.cpp
- 图访问类型和 pass flags：function/render/render_graph/render_graph_resource.h、render_graph_pass.h
- deferred command list 与全局 frame memory：function/graphics/draw_command_list.cpp、core/memory/memory.cpp
- GPU Scene 同步：function/render/render_scene/render_scene.cpp
- GPU culling 原型：function/render/gpu_driven/gpu_driven_renderer.cpp
- device/backend capability：function/graphics/gfx_context.cpp、function/render/render_settings.h
- shader library、PSO、texture/descriptor manager：function/render/framework/