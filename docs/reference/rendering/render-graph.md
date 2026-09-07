# RenderGraph 体系

本文覆盖 Dodoe 的帧渲染图(RDG)体系:资源声明与访问模型、Pass 两段式声明(setup/execute)、编译期依赖分析/裁剪/屏障推导/分层排序、执行期并行录制与资源实体化,以及瞬态资源池与渲染管线的集成方式。

代码位置:`engine/src/runtime/function/render/render_graph/`;上层消费者(Renderer/Feature/Pass)见 render-pipeline.md,本文聚焦图本身。

## 1. 总览

```text
每帧、每个 RenderView 一张图:
  IRenderPass::build(graph, context)          ← 各 Pass 声明式注册(setup 阶段只记访问,不碰 GPU)
      │  create/import 资源 + read/writeColor/writeDepth/writeUav/exportTexture + blackboard
      ▼
  RenderGraph::compile()
      ① buildDependencyGraph   写→读 / 写→写 / 读→写 边,资源 writer/reader 记录
      ② validateAccesses       未初始化读、写而不读不导出、UAV 双写 冲突检测
      ③ cullUnreachablePasses  从导出资源/后缓冲反向标记可达,其余剔除(NeverCull 例外)
      ④ deriveBarriers         按序追踪资源状态,生成每 pass 的 pre-barriers
      ⑤ topologicalSort        Kahn 分层;层内并行,层间串行;成环 assert
      ▼
  RenderGraph::execute(ThreadPool, context, out_commands)
      ResourceResolver 实体化资源(transient pool / imported / backbuffer)
      逐 level:层内每 pass 一个线程 + 独立 DrawCommandList 录制
        (barriers → attachments/framebuffer/clear → beginMarker → pass.execute → endMarker)
      层 WaitGroup 等待后按序 append 到 out_commands
```

## 2. 资源模型(render_graph_resource.h)

### 2.1 句柄与记录

- `RenderGraphTextureHandle` / `RenderGraphBufferHandle`:仅含 `UInt32 index` 的轻句柄,生命周期只在本帧图内,执行期经 `RenderGraphResourceResolver` 解析为 `GfxTextureHandle` / `GfxBufferHandle`;
- `RenderGraphResourceRecord`:资源登记表条目 —— `type`(Texture/Buffer)、`source`(Transient / ImportedTexture / ImportedBuffer / ImportedBackBuffer)、`name`、transient 描述(`RenderGraphTextureDesc/BufferDesc`,内嵌 `GfxTextureDesc/GfxBufferDesc`)或导入句柄,以及编译产物:`writer_passes`、`reader_passes`、`first/last_pass_index`、`is_exported` + `export_final_state`。

### 2.2 访问描述

| 概念 | 取值 | 说明 |
|---|---|---|
| `RenderGraphAccessType` | Read / Write / ReadWrite | ReadWrite 即 UAV 语义 |
| `RenderGraphPipelineStage` | VertexShader / PixelShader / ComputeShader / Copy / RenderTarget / DepthStencil | 决定目标资源状态 |
| `RenderGraphSubresourceRange` | base_mip / mip_count / base_array_layer / array_layer_count | 子资源级访问 |
| `RenderGraphPassResourceAccess` | resource_index + access_type + stage + subresource | Pass 侧的最小访问单元 |

访问 → 资源状态的映射(`accessToRequiredState`,render_graph.cpp:20):ReadWrite → UnorderedAccess;Read → Copy 阶段为 CopySource,其余 ShaderResource;Write → RenderTarget / DepthWrite / CopyDest / ShaderResource。

### 2.3 渲染目标附件

- `LoadOp`(Load/Clear/DontCare)+ `StoreOp`(Store/DontCare)+ `clear_color`;深度侧 depth/stencil 各自一对 Load/Store + clear_depth/clear_stencil;
- `RenderGraphColorAttachment` / `RenderGraphDepthStencilAttachment`:句柄 + load/store 语义;
- Pass 最多 8 个 color 槽(`kRenderGraphMaxColorAttachments`),`RenderGraphRenderTargetBindingSlots` 聚合 color + depth;
- 便捷构造:`MakeRenderTarget2D` / `MakeDepthTarget2D`(自动 `isRenderTarget` + automaticStateTracking,渲染目标默认跟踪回 ShaderResource 状态)。

## 3. Pass:声明与执行(render_graph_pass.h)

### 3.1 RenderGraphPass

图中的节点:

- `name` + `flags`:`Raster / Compute / Copy / NeverCull / AsyncCompute`(位标志,`HasAnyFlags` 判断);
- `m_accesses`:setup 期登记的全部资源访问(依赖分析的唯一依据);
- `m_color_slots` / `m_depth_slot`:writeColor/writeDepth 时同步登记的附件槽(执行期据此建 framebuffer、执行 clear);
- `m_pre_barriers`:编译期 `deriveBarriers` 写入的自动屏障;
- `m_execute_function`:`std::function<void(const RenderGraphPassContext&, DrawCommandList&)>`,执行期回调;
- `m_subgraph_index`:所属 view 子图(仅作标注/统计)。

### 3.2 RenderGraphPassContext(执行期上下文)

两层结构:

- `RenderGraphExecuteContext`(整图一次):view_family / scene / view、`GfxContext`、`SharedRenderService`、`FrameStagingAllocator`、`RenderGraphTransientPool`、view_index、swapchain_image_index;
- `RenderGraphPassContext`(每 pass):包装上述上下文,并提供
  - `resolveTexture(handle)` / `resolveBuffer(handle)` → 经 resolver 把图内句柄换成真实 `Gfx*Handle`;
  - `getShaderLibrary()` / `getPipelineStateCache()` / `getTextureManager()`(经 SharedRenderService 转发,render_graph_pass.cpp);
  - `setFramebuffer` / `getRenderTargetSignature()`(execute 期由图框架填好,供 PSO 解析匹配 RT 签名)。

### 3.3 RenderGraphPassBuilder(setup 期 API)

| 方法 | 登记的访问 | 附加效果 |
|---|---|---|
| `createTransientTexture/Buffer(desc, name)` | — | 注册瞬态资源,返回句柄 |
| `importTexture/Buffer(handle, name)` | — | 导入已有 GPU 资源 |
| `importBackBuffer(name)` | — | 导入交换链图像(按 swapchain_image_index 解析) |
| `readTexture(handle, stage, subresource)` / `readBuffer` / `read` | Read | 产生 writer→本 pass 的依赖边 |
| `writeColor(handle, attachment)` | Write@RenderTarget | 登记 color 槽(load/clear 语义) |
| `writeDepth(handle, attachment)` | Write@DepthStencil | 登记 depth 槽 |
| `writeUav(handle, stage)` | ReadWrite | UAV 语义,参与冲突校验 |
| `writeBuffer(handle, stage)` / `write` | Write | — |
| `exportTexture(handle, final_state)` | — | 标记 `is_exported`,防裁剪 + 决定末态 |
| `blackboard()` | — | 跨 pass 传递数据(§5) |

## 4. RenderGraphBuilder 与每帧图的组装(render_graph_builder.h/.cpp)

`RenderGraphBuilder` 持有一个 `RenderGraph` + `RenderGraphBlackboard` + 当前子图索引:

- 资源注册 `createTexture/createBuffer/importTexture/importBuffer/importBackBuffer` 直接转发给图;
- `beginViewSubgraph(name)` / `endViewSubgraph()`:之后的 pass 标记子图归属(`m_subgraph_names` 记名);
- `addPass<TParameters>(name, flags, setup, execute)`(模板,render_graph_builder.h:34):创建 pass → 记子图 → 构造 `TParameters{}` 并执行 `setup(RenderGraphPassBuilder&, TParameters&)` → 把 parameters **按值捕获**进 execute 闭包 → 入图。参数结构体是 setup 与 execute 之间传递图资源句柄的通道;
- `addPass(IRenderPass&, RenderPassBuildContext&)`:调用 `render_pass.build(*this, context)`,把管线侧 pass 对象接入(§8);
- `exportTexture / compile / execute / reset`:转发图;`reset` 同时清 blackboard。

## 5. Blackboard:跨 Pass 数据流(render_graph_blackboard.h)

`TypedKeyValueStore` 的薄封装,按类型键存取:

```cpp
// 键定义(类型即键):Value 可以是单句柄或结构体
struct SceneHdrKey { using Value = RenderGraphTextureHandle; };
struct SceneTexturesKey { using Value = SceneTextures; };   // albedo/normal/position/material/depth + instance buffer

// 生产者(setup 期):blackboard().set<SceneColorKey>(parameters.output);
// 消费者(setup 期):const auto* hdr = blackboard().get<SceneHdrKey>();
```

典型键在 `render_pipeline/passes/render_pass_blackboard_keys.h`(SceneTextures / ShadowMap / SceneHdr / ToneMapped / SceneColor / FxaaColor / SpriteColor / ImGuiColor)。GBuffer 等结构由生产 pass 写入整套 SceneTextures,后续 lighting/post/present 依次读取。注意 blackboard 是 **图级** 的:每个 view 的图独立,同帧跨 view 不共享。

## 6. 编译:RenderGraph::compile(render_graph.cpp)

按固定顺序执行五个阶段;compile 后禁止再 addPass/addResource(assert)。

### 6.1 buildDependencyGraph

单遍扫描 pass 序列,对每个 access 维护每个资源的 `writer_passes` / `reader_passes`:

- **Read**:若有 writer,加边 `最新 writer → 本 pass`(RAW);登记为 reader;
- **Write / ReadWrite**:先 `最新 writer → 本 pass`(WAW),再对所有现存 `reader → 本 pass`(WAR),然后**清空 reader 列表**、把自己登记为最新 writer。

这保证了同帧内对同一资源的先后访问全部串行化。

### 6.2 validateAccesses

- **未初始化读**:非导入资源有 reader 无 writer → assert;
- **写而不读**:非导入资源有 writer 无 reader 且未 export → 报错 + assert(浪费的写入);
- **UAV 冲突**:两个 pass 对同一资源都是 ReadWrite → assert(ReadWrite 访问必须唯一)。

### 6.3 cullUnreachablePasses

从"根资源"(exported 或 ImportedBackBuffer)出发反向传播:

1. 根资源的 writer/reader 标记为可达;可达 pass 的其他访问资源连带标记(写依赖与读依赖各按方向传播);
2. 再沿依赖边正向补标(被可达 pass 依赖的 pass 也不可裁);
3. 不在可达集且无 `NeverCull` 标志的 pass → `m_culled_passes[i] = true`。

零访问的 pass 在 ① 阶段已直接剔除(除非 NeverCull)。

### 6.4 deriveBarriers

按 pass 顺序模拟资源状态机:

- 导入资源(含后缓冲)初始状态为 Common,瞬态资源初始 Unknown;
- 每个 pass 的每个 access 计算 `accessToRequiredState`;当前状态 ≠ 所需且当前 ≠ Unknown 时生成一条 `RenderGraphBarrier{resource, from, to, subresource}`,挂在 pass 的 `pre_barriers`;随后更新当前状态。

即:屏障是"每个 pass 前置"的,不跨 pass 全局排序;同 pass 内多个访问各自触发。

### 6.5 topologicalSort

Kahn 算法分层:入度 0 的 pass 为第一层,剥掉后入度归零的为下一层……被剔除的 pass 不进入 `m_levels`(但仍参与入度削减)。层 == 可并行的最大 pass 集合;若中途 current_level 为空说明存在环,assert。

编译产物:`m_levels`(层级 × pass 索引)、`m_culled_passes`、每 pass 的 `pre_barriers`;`m_compiled = true`。

## 7. 执行:RenderGraph::execute(render_graph.cpp:397)

### 7.1 资源实体化:RenderGraphResourceResolver

execute 开头构造 resolver,一次性为**全部**资源分配真实 GPU 对象(render_graph_resource_resolver.cpp):

| source | 纹理解析 |
|---|---|
| Transient | `transient_pool->acquireTexture(desc, cmd)`(无池则 `cmd.createTexture`) |
| ImportedTexture | 直接用导入句柄 |
| ImportedBackBuffer | `gfx_context.getSwapchainTextures()[swapchain_image_index]` |

Buffer 同理(acquireBuffer / 导入句柄)。`getTexture/getBuffer` 就是 O(1) 数组下标解析。由于 Proxy + 惰性实体化机制(threading.md),`createTexture` 只是录命令,真正的 GPU 对象在提交线程创建。

### 7.2 瞬态资源池:RenderGraphTransientPool(render_graph_transient_pool.h/.cpp)

跨帧常驻的内存池,给"每帧建/销毁一堆中间 RT"免单:

- `acquireTexture/acquireBuffer`:线性扫描空闲槽,**全字段 desc 精确匹配**(纹理:尺寸/数组/mip/sample/格式/维度;缓冲:大小/stride/格式/全部用途标志)则复用,否则 `cmd.createTexture/createBuffer` 新建并入池;
- `releaseAll()`:全部 `in_use` 清零(每帧 execute 结束后由上层调用,句柄保留到下帧复用);
- `reset()`:彻底清空(关闭/设备重建时)。

### 7.3 每 Pass 执行

对 `m_levels` 逐层处理,层内按模式分流:

- **direct_mode(OpenGL 后端,`gfx_context->getOpenGLBackend() != nullptr`)**:串行,直接录到 `out_commands`(GL 单上下文不允许并行录制,见 threading.md);
- **默认模式**:为层内每个 pass 分配一个独立 `DrawCommandList`,`ThreadPool::enqueue` 并行执行,`WaitGroup` 等待整层完成后按 pass 顺序 `append` 合并到 `out_commands`。

每个 pass 的录制序列(两种模式一致):

```text
① pre-barriers:setTextureState/setBufferState(from→to)→ commitBarriers
② setupPassAttachments:
     color/depth 槽资源置 RT/DepthWrite 状态 → commitBarriers
     LoadOp::Clear 的槽 → clearTextureFloat / clearDepthStencilTexture
     有附件 → cmd.createFramebuffer(GfxFramebufferDesc),塞进 pass_context
     特例:引用 ImportedBackBuffer 的槽 → 直接用 gfx_context.getSwapchainFramebuffer,
           且断言"后缓冲必须单独作为唯一附件、clear 必须由 pass 自己显式做"
③ beginMarker(pass name) → pass->execute(pass_context, cmd_list) → endMarker
```

异步计算(`AsyncCompute` 标志)当前仅作标注,尚未有独立队列分流。

## 8. 与渲染管线的集成(render_pipeline/)

### 8.1 Pass 的收集与排序(IRenderPass / BaseRenderer)

- `IRenderPass`(render_pass.h):`getPhase()` 决定静态排序;`getProducedKeys()/getConsumedKeys()` 声明 blackboard 依赖(`TypeList` 的 typeid hash);`build(RenderGraphBuilder&, const RenderPassBuildContext&)` 每帧被调用,把自己的图内容注册进图;
- `BaseRenderer::bakePasses`(renderer.cpp:37):启动期 Feature → `PassCollector` 收集 pass,按 `RenderPhase` 排序,并 `validateBlackboard` 校验"先生产后消费";
- `RenderPassBuildContext`:view、`RenderGraphImportRegistry*`、gfx_context、shared_render_service、scene。

### 8.2 图级导入:RenderGraphImportRegistry(render_graph_import_registry.h)

与 blackboard 相对的"**外部 → 图**"通道:Feature 在 `registerGraphImports(registry, view)` 中 `publish<Key>(value)`(GBuffer/ShadowMap 的 `RenderTargetHandle*`、ImGui 字体纹理等,见 render_graph_import_keys.h),之后 `freeze()`;pass 的 build 期用 `find<Key>()` / `require<Key>()` 读取并 `importTexture/importBuffer` 进图。publish 后冻结、重复 publish 断言,保证每帧导入集合稳定。

### 8.3 每帧流程(BaseRenderer::buildOrderedPasses,renderer.cpp:65)

```text
for 每个 view:
    各 Feature registerGraphImports → registry.freeze()
    RenderGraphBuilder graph
    for 有序 pass:pass->build(graph, build_ctx)      ← 全部 setup,无 GPU 工作
    graph.compile()
for 每个 view:
    填 RenderGraphExecuteContext(view/gfx_context/services/池/swapchain index)
    graph.execute(thread_pool, context, out_commands)  ← 录制合并到主命令流
```

即:**每个 view 一张独立图**,共享同一个 out_commands 与 transient pool。

### 8.4 典型 Pass 写法(render_post_process_pass.cpp)

```cpp
struct PostProcessPassParameters {                       // setup↔execute 的参数通道
    RenderGraphTextureHandle input{}, output{};
};

void PostProcessPass::build(RenderGraphBuilder& graph, const RenderPassBuildContext& context) {
    graph.addPass<PostProcessPassParameters>(
        "PostProcessPass", RenderGraphPassFlags::Raster,
        // setup:只登记依赖与资源,不碰 GPU
        [&context](RenderGraphPassBuilder& b, PostProcessPassParameters& p) {
            const auto* hdr = b.blackboard().get<SceneHdrKey>();
            p.input = b.read(*hdr);
            p.output = b.writeColor(b.createTransientTexture(
                MakeSwapchainRT2D(...), "PostProcessOutput"), {LoadOp::Clear, ...});
            b.blackboard().set<SceneColorKey>(p.output);   // 交给下游
        },
        // execute:resolve 句柄 → 建 binding set/PSO → 录制
        [shader_library](const PostProcessPassParameters& p, const RenderGraphPassContext& ctx, DrawCommandList& cmd) {
            shader_params.input.value = p.input;           // 图句柄
            auto sets = ShaderBindingReflector<...>::createBindingSets(
                cmd, layouts, shader_params,
                [&](auto h) { return ctx.resolveTexture(h); },   // 图句柄 → Gfx 句柄
                [&](auto h) { return ctx.resolveBuffer(h); });
            auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                desc, ctx.getRenderTargetSignature(), cmd);      // RT 签名匹配 PSO
            cmd.setGraphicsState(ctx.getFramebuffer(), pipeline, sets, viewport);
            cmd.draw(...);
        });
}
```

## 9. 调试与可视化

- `dumpToJSON()`:全部 pass(name / culled / async / accesses / barrier 数)+ 全部资源(exported / imported / first/last pass),适合帧分析;
- `dumpToDOT()`:Graphviz 有向图,writer→reader 边标注资源名,被裁 pass 灰色节点。

## 10. 设计要点与约束

- **setup/execute 分离是硬性契约**:资源依赖只认 setup 期登记的 access,execute 里私下动用未登记资源不会有屏障与依赖保护;
- **参数按值捕获**:`addPass<TParameters>` 把参数结构拷进闭包,execute 不可再改图结构;
- **依赖边按"最新 writer"建**:同资源连续写只连最后一环(WAW 链),reader 列表在写入时清空(WAR),避免边数爆炸;
- **屏障粒度**:状态推断按访问序精确推导,支持子资源范围;附件的额外状态切换(setupPassAttachments 内 setTextureState)与 clear 在 barrier 提交之后进行;
- **裁剪的根**:只有 exported 资源与后缓冲是"活"的——中间结果若无人消费,整条生产链都会被裁掉;`NeverCull` 用于有副作用必须执行的 pass;
- **并行边界**:层内并行录制、层间严格串行,保证跨层依赖(经资源访问隐式表达)天然安全;OpenGL 退化为全串行 direct 模式;
- **资源生命周期**:transient 资源由池按 desc 复用,池在帧间 `releaseAll` 而非销毁——desc 变化(如 resize)会自然 miss 并新建,旧槽闲置,需要在 resize 时 `reset` 池避免膨胀。

## 11. 相关文档

- [render-pipeline.md](render-pipeline.md):Renderer/Feature/Pass 体系与 GpuScene,本文 §8 的上层背景。
- [threading.md](threading.md):双线程模型、DrawCommandList 命令流与 GfxRenderScope,execute 并行录制的基础。
- [rhi.md](rhi.md):GfxTexture/GfxBuffer Proxy、屏障与状态追踪的 RHI 侧实现。
