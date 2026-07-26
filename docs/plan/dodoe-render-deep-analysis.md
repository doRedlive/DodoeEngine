# DoDoE 渲染架构深度分析报告

> 基于 `dodoe-render-architecture-roadmap.md` 的逐项代码验证、任务拆解与技术设计补充
> 代码库快照：2026-07-18，分支 `do-dev`

---

## 代码风格约定

实施时必须遵守以下代码风格：

1. **namespace 内缩进**：`namespace dodoe { ... }` 内部使用 4 空格缩进（tab）
2. **注释规则**：
   - 只保留 `// do@Redlive` 文件头注释和 `// namespace dodoe` 闭合注释
   - **不写**任何其他注释（包括行内注释、函数注释、块注释等）
3. **命名风格**：与现有代码保持一致（`PascalCase` 类型、`camelCase` 变量/函数、`m_` 成员前缀）

## 文档追踪约定

每次有代码进展时必须实时更新本文档：

1. 将对应任务的状态从 `⬜ 待实施` 改为 `✅ 完成`
2. 填入完成日期和简要备注（涉及文件 + 关键改动点）
3. 如果有新发现或与原计划有偏差，在备注中记录
4. 每完成一轮 Phase 的最后一个任务时，在下方追加一轮小结

---

## 实施进度追踪

| # | 任务 | Phase | 状态 | 日期 | 备注 |
|---|------|-------|------|------|------|
| R0.2a | 修复 delta 消费顺序（move 到局部 delta） | R0 | ✅ 完成 | 2026-07-18 | `render_scene.cpp:rebuildPipelineSceneData()` |
| R0.2b | 补充 primitive GPU scene 更新 | R0 | ✅ 完成 | 2026-07-18 | 同上，与 R0.2a 一起实施 |
| R0.2c | GPU object handle 加 generation | R0 | ✅ 完成 | 2026-07-18 | SlotMap 已有 generation，为 GpuScene update* 方法加 m_objects.get() 验证 |
| R0.1a | 移除 reset() 中 AdvanceFrameEpoch | R0 | ✅ 完成 | 2026-07-18 | 从 CommandList::reset() 移除，改到 RenderSystem::renderFrame() 帧边界 |
| R0.3a-c | Capability 分层模型 | R0 | ✅ 完成 | 2026-07-18 | DeviceCapabilities + RenderFeatureSettings + ResolvedRenderFeatures，ResolveFeatures() 在 RenderSettings |
| R0.4a-c | FrameScheduler 最小版 | R0 | ✅ 完成 | 2026-07-18 | 新建 `core/frame/frame_scheduler.h/.cpp`，Managed 模式，集成到 RenderSystem |
| R0.5a | 修复 RenderThread 假异步 | R0 | ✅ 完成 | 2026-07-18 | 新增 `submit()` 非阻塞方法，TripleThread 走非阻塞路径 |
| R0.5b | SpscQueue 无锁化 | R0 | ✅ 完成 | 2026-07-18 | atomic head/tail + cacheline padding，移除 mutex 保护 |
| R0.6a | 连接 GpuCulling | R0 | ✅ 完成 | 2026-07-18 | DeferredRenderer 集成 GpuCulling，使用动态 object_count |
| R0.6b | MeshDrawCommandDispatcher barrier 优化 | R0 | ✅ 完成 | 2026-07-18 | 批量写入 shader data，单次 barrier 对替代 per-draw barrier |
| R1.1 | FrameScheduler 完整版（fence + per-slot arena + 延迟回收） | R1 | ✅ 完成 | 2026-07-18 | kMaxFramesInFlight=3，EventQuery per-slot GPU fence，deferred deletion queue |
| R1.2 | UploadRing（per-frame ring buffer allocator） | R1 | ✅ 完成 | 2026-07-18 | 新建 `core/frame/upload_ring.h/.cpp`，persistent mapped buffer + bump allocator，集成到 FrameSlot + FrameContext |
| R1.3 | DeferredDeletionQueue（独立组件） | R1 | ✅ 完成 | 2026-07-18 | 新建 `core/frame/deferred_deletion.h/.cpp`，FrameScheduler 内部改用 DeferredDeletionQueue |
| R1.4 | RenderSceneDelta 批量不可变结构 | R1 | ✅ 完成 | 2026-07-18 | 新建 `render_scene_delta.h`，`rebuildPipelineSceneData()` 改用 RenderSceneDelta，GPU scene 同步提取为 syncPrimitiveGpuScene/syncSpriteGpuScene |
| R1.5 | TextureManager 迁移至 UploadRing | R1 | ✅ 完成 | 2026-07-18 | createTexture/loadTexture/loadCubemapTexture 加 UploadRing* 参数；createFallbackTexture 移除 GDdrawCommandList；cubemap 用 DrawCommandList& 替代专用 cmd list |
| R1.6 | Frame telemetry 基础 | R1 | ✅ 完成 | 2026-07-18 | 新建 `core/frame/frame_telemetry.h/.cpp`，FrameTelemetry 结构 + FrameTelemetryCollector（256帧环形历史），集成到 FrameScheduler |
| R1.7 | 消除 GDrawCommandList 全局变量 | R1 | ✅ Phase1 | 2026-07-18 | Phase 1: `thread_local` 替代全局，新增 `GetThreadDrawCommandList()` 访问器；Phase 2-3（显式参数传递）延后到各模块渐进迁移 |
| — | File Move: framework → shader/pipeline/material | R3 | ✅ 完成 | 2026-07-18 | shader_library/shader_parameter/descriptor_table_manager/global_samplers → shader/；pipeline_state_cache/pso_key → pipeline/；material → material/；更新全部 include 引用 |
| R2.1 | 扩展 RenderGraphAccessType（ReadWrite/UAV + subresource） | R2 | ✅ 完成 | 2026-07-18 | `render_graph_resource.h`: ReadWrite + PipelineStage + SubresourceRange + AttachmentInfo |
| R2.2 | RenderGraphPassBuilder 语义化 API | R2 | ✅ 完成 | 2026-07-18 | `render_graph_pass.h/.cpp`: readTexture/writeColor/writeDepth/writeUav/readBuffer/writeBuffer/exportTexture |
| R2.3 | 编译期 validation（未初始化 read / 重复 write） | R2 | ✅ 完成 | 2026-07-18 | `render_graph.cpp`: validateAccesses() — 未初始化读/写未读/UAV 冲突检测 |
| R2.4 | 自动 barrier 生成（access 声明 → transition） | R2 | ✅ 完成 | 2026-07-18 | `render_graph.cpp`: deriveBarriers() + accessToRequiredState()，execute() 中自动应用 pre-barriers |
| R2.5 | Transient resource pool（lifetime + 池化） | R2 | ✅ 完成 | 2026-07-18 | `render_graph_resource_resolver.h/.cpp`: TransientResourcePool，desc 匹配复用，releaseAll 每帧归还 |
| R2.6 | Export root + 反向可达性 pass culling | R2 | ✅ 完成 | 2026-07-18 | `render_graph.cpp`: cullUnreachablePasses() — 从 export/backbuffer 反向 BFS，不可达 pass 剔除 |
| R2.7 | Graph dump（JSON/DOT 导出） | R2 | ✅ 完成 | 2026-07-18 | `render_graph.cpp`: dumpToJSON() + dumpToDOT()，pass/resource/barrier 完整信息导出 |
| R2.8 | Transient aliasing + memory budget（延后） | R2 | 🔵 延后 | — | 待 profiling 数据支撑 |
| R2.9 | Async compute + multi-queue（延后） | R2 | 🔵 延后 | — | 待 R2.4 完成 |
| R3.1 | Shader manifest 文件格式 + parser | R3 | ✅ 完成 | 2026-07-18 | 见 R3.1 改动摘要 |
| R3.2 | Shader 反射（SPIR-V/DXIL → CBV/SRV/UAV/sampler） | R3 | ✅ 完成 | 2026-07-18 | `shader/shader_reflection.h/.cpp`: ShaderReflectionData + ShaderReflector，DXIL(D3DReflect) + SPIR-V(自解析) 双路径 |
| R3.3 | 基于反射自动生成 binding layout | R3 | ✅ 完成 | 2026-07-18 | `shader/binding_layout_generator.h/.cpp`: BindingLayoutGenerator，多 stage 合并 + 冲突检测；ShaderLibrary 集成反射缓存 |
| R3.4 | Material 模板/实例系统 | R3 | ✅ 完成 | 2026-07-18 | `material/material_system.h/.cpp`: MaterialTemplate + MaterialInstance + MaterialSystem，参数解析 + CB 数据构建 |
| R3.5 | PSO disk cache | R3 | ✅ 完成 | 2026-07-18 | `pipeline/pso_disk_cache.h/.cpp`: PsoDiskCache，二进制文件格式 + shader hash 失效 + 增量刷盘 |
| R3.6 | Shader 热重载 | R3 | ✅ 完成 | 2026-07-18 | `shader/shader_hot_reload.h/.cpp`: ShaderHotReload，文件时间戳轮询 + 变更检测 |
| R3.7 | Non-bindless fallback 路径 | R3 | ✅ 完成 | 2026-07-18 | `shader/binding_set_allocator.h/.cpp`: BindingSetAllocator，传统 binding-set 路径，与 bindless descriptor heap 互补 |
| R4.1 | GPU Scene 增量同步加固 | R4 | ✅ 完成 | 2026-07-18 | LightGpuData + DirtyRange bitmask + GpuSceneStats + applyDelta + syncLightGpuScene |
| R4.2 | GPU frustum culling（动态 count + compact list） | R4 | ✅ 完成 | 2026-07-18 | GpuVisibleStats + readback buffer + getObjectCount |
| R4.3 | Indirect args 构建（分桶 + prefix sum） | R4 | ✅ 完成 | 2026-07-18 | BucketKey/BucketCount/IndirectArgs 结构 + bucket_count/fill CS + manifest 条目 |
| R4.4 | Draw pass 消费 indirect args（ExecuteIndirect） | R4 | ✅ 完成 | 2026-07-18 | buildGpuDrivenDrawCommands + GpuScene buffer 绑定 + drawIndexedIndirect |
| R4.5 | CPU/GPU 路径 A/B 对比 + 等价验证 | R4 | ✅ 完成 | 2026-07-18 | CullingPath 枚举（CpuOnly/GpuOnly/CpuThenGpuVerify）+ RenderSettings 集成 |
| R4.6 | HZB occlusion（可选） | R4 | 🔵 延后 | — | 待 frustum culling 成为瓶颈 |
| R4.7 | 多 view 统一 FrameGraph | R4 | ✅ 完成 | 2026-07-18 | RenderGraph 子图支持 + beginViewSubgraph/endViewSubgraph + addSubgraph |
| R4.8 | Async compute batching（延后） | R4 | 🔵 延后 | — | 待 R2.9 完成 |

### R1.1 改动摘要

**修改文件**：[frame_scheduler.h](frame_scheduler.h) / [frame_scheduler.cpp](frame_scheduler.cpp), [frame_context.h](frame_context.h), [draw_executor.h](draw_executor.h) / [draw_executor.cpp](draw_executor.cpp), [render_system.cpp](render_system.cpp)

- `kMaxFramesInFlight`：2 → 3，增加 CPU-GPU 并行度
- `FrameSlot` 新增 `EventQueryHandle completion_query`：per-slot GPU fence
- `FrameContext` 新增 `EventQueryHandle completion_query`：传递给 draw thread
- `beginFrame()`：复用 slot 前 `waitEventQuery` + `resetEventQuery` 阻塞直到 GPU 完成，随后 `processDeferredDeletions()`
- `deferDeleteFunc()`：延迟释放回调，GPU fence 完成后执行
- `processDeferredDeletions()`：清理已完成帧的延迟释放条目
- `getInFlightCount()`：查询当前 in-flight 帧数
- `DrawExecutor::execute()`：`executeCommandList` 后调用 `setEventQuery` 插入 GPU 信号点
- `render_system.cpp`：TripleThread 路径传递 `completion_query`；Dual/SingleThread 路径在 `ImmediateFrameScope` 后 `setEventQuery`

### R1.2 改动摘要

**新建文件**：[core/frame/upload_ring.h](upload_ring.h) / [upload_ring.cpp](upload_ring.cpp)

- `UploadRing` 类：per-frame ring buffer allocator，默认 64 MB
- 使用 persistent mapped buffer（`CpuAccessMode::Write` + `mapBuffer`），CPU 直接写入映射指针
- `allocate(size, alignment)` 返回 `Allocation{buffer, offset, size, mapped_data}`，bump allocator 推进 head
- `reset()` 回收整环（head 归零），对齐 FrameScheduler 的 per-slot 复用模型
- `getStallCount()` / `getOverflowCount()` 统计接口，配合 R1.6 telemetry

**修改文件**：[frame_scheduler.h](frame_scheduler.h) / [frame_scheduler.cpp](frame_scheduler.cpp), [frame_context.h](frame_context.h), [render_system.cpp](render_system.cpp)

- `FrameSlot` 新增 `UploadRing upload_ring` 成员
- `FrameScheduler::initialize()` 为每个 slot 初始化 upload_ring，`shutdown()` 对应释放
- `FrameScheduler::beginFrame()` 在复用 slot 时调用 `upload_ring.reset()`
- `FrameContext` 新增 `UploadRing* upload_ring` 指针，供 render pass 访问
- `RenderSystem::renderFrame()` TripleThread 路径传递 `&frame_slot.upload_ring` 到 FrameContext

### R0.2a/b 改动摘要

**文件**：[render_scene.cpp:417-521](render_scene.cpp:417)

**改动前**：
- `m_pending_*_updates` 在 CPU Phase 后被 clear
- GPU Phase 循环迭代 `m_pending_*_updates`（此时已为空）→ **死代码**
- 仅 sprite 有 GPU scene 更新，primitive 无

**改动后**：
- 开头将 `m_pending_primitive_updates` 和 `m_pending_sprite_updates` move 到 `primitive_delta` / `sprite_delta` 局部变量
- CPU Phase 消费局部 delta
- GPU Phase 消费**同一个**局部 delta → **不再为空**
- 新增 primitive GPU scene 更新循环（transform、bounds、PrimitiveGpuData）

### R0.4a-c 改动摘要

**新建文件**：[core/frame/frame_scheduler.h](frame_scheduler.h) / [frame_scheduler.cpp](frame_scheduler.cpp)

- 使用 `Managed<FrameScheduler, FrameSchedulerCreateInfo>` 模式
- `beginFrame()` 执行 pending resize + 获取 swapchain image + 返回 FrameSlot&
- `requestResize()` / `tryExecutePendingResize()` 延迟 resize 到安全的帧边界
- `RenderSystem` 中集成：`FrameScheduler::Create()` 初始化、`beginFrame()` 替换手动 acquire
- `frame_context.h` 从 `function/render/` 移入 `core/frame/`，新增 `frame_number` 字段

### R0.5a/b 改动摘要

**文件**：[render_thread.h](render_thread.h) / [render_thread.cpp](render_thread.cpp), [spsc_queue.h](spsc_queue.h), [system_context.cpp](system_context.cpp)

- `RenderThread::submit()` 新增非阻塞方法：只发信号，不等待完成
- `system_context.cpp:renderTick()` 改为 switch：TripleThread → `submit()`，DualThread → `submitAndWait()`，SingleThread → `executeFrameOnce()`
- `SpscQueue` 重写为 `atomic<Size_t>` head/tail + cacheline padding（`alignas(64)`）
- push/pop 阻塞等待改用 condition_variable 轮询（100μs），tryPush/tryPop 纯无锁

### R0.6a/b 改动摘要

**文件**：[gpu_driven_renderer.h](gpu_driven_renderer.h) / [gpu_driven_renderer.cpp](gpu_driven_renderer.cpp), [deferred_renderer.h](deferred_renderer.h) / [deferred_renderer.cpp](deferred_renderer.cpp), [mesh_draw_command_dispatcher.cpp](mesh_draw_command_dispatcher.cpp)

- `GpuCulling::executeCulling()` 新增 `object_count` 参数，buffer 按实际对象数动态扩容
- `DeferredRenderer` 集成 `Scope<GpuCulling>`，在 `render()` 中 CPU 粗剔后、mesh draw 前调用
- `MeshDrawCommandDispatcher` 改为 `BatchWritePassShaderData()` 批量写入，一次 barrier 对（CopyDest → 批量写入 → ConstantBuffer）替代 per-draw N 次 barrier

---

---

---

## 3. Phase R1-R4：详细任务拆解

### 3.1 Phase R1：Frame Infrastructure

> **已实施：R1.1  FrameScheduler 完整版** ✅ 2026-07-18 — 见实施进度追踪表

---

#### Task R1.2：实现 UploadRing（per-frame ring buffer allocator）

- 新建文件：`core/frame/upload_ring.h` / `upload_ring.cpp`
- 依赖：R1.1（FrameScheduler 提供 per-slot fence 与回收时机）

**目标**：将常量化 buffer、动态 instance buffer、纹理初始数据等上传操作统一从 UploadRing 分配 staging 内存，替代当前每个 draw call 临时 `createBuffer()` + `writeBuffer()` 的模式。

**设计要点**（详见 4.5 节）：
1. 每个 `FrameSlot` 持有一个 `UploadRing` 实例，大小默认 64 MB
2. 使用 persistent mapped buffer（`GfxBufferHandle` + `mapped_data` 指针），CPU 直接写入
3. `allocate(size, alignment)` 返回 `{buffer, offset, mapped_ptr}`，head 指针推进
4. 空间不足时：flush 当前已录制命令 → 推进 fence → `recycle()` 回收 → 重试
5. `markUsed(alloc, fence_value)` 记录分配对应的 fence
6. `recycle(completed_fence)` 推进 tail，释放已完成 fence 的空间

**与 FrameScheduler 的集成点**：
```cpp
// FrameScheduler::beginFrame() 中：
slot.upload_ring.reset();  // 回收上一轮已完成 fence 的空间

// FrameScheduler::submit() 中：
slot.upload_ring.markAllUsed(submit_fence);  // 本帧所有分配标记 fence
```

**验收**：
- 每帧 upload bytes、stall count、overflow count 可观测（配合 R1.6 telemetry）
- 大纹理上传不再在 render thread 上分配临时 buffer
- 连续 10000 帧无 upload ring 相关的 OOM 或数据损坏
- PIX/RenderDoc 中 staging buffer 数量从 per-draw 降低为固定 per-slot

---

#### Task R1.3：实现 DeferredDeletionQueue

- 新建文件：`core/frame/deferred_deletion.h` / `deferred_deletion.cpp`
- 依赖：R1.1（FrameScheduler 提供 fence completion 通知）

> **注意**：R1.1 的 `FrameScheduler` 已内置 `deferDeleteFunc()` 和 `processDeferredDeletions()`。R1.3 将其提取为独立可复用的组件，供非 FrameScheduler 上下文（如 asset manager）使用。

**目标**：GPU 资源的销毁不能依赖 CPU ref count 立即发生，必须入队等待 GPU fence 完成。

**设计**：
```cpp
class DeferredDeletionQueue {
    struct Entry {
        std::function<void()> deleter;
        UInt64 fence_value;
    };
    DynamicArray<Entry> m_queue{};

public:
    template <typename T>
    void enqueue(Scope<T> resource, UInt64 fence_value) {
        m_queue.push_back({
            [res = std::move(resource)]() mutable { res.reset(); },
            fence_value
        });
    }

    void processCompleted(UInt64 last_completed_fence) {
        // 移除并执行所有 fence_value <= last_completed_fence 的条目
        auto it = std::remove_if(m_queue.begin(), m_queue.end(),
            [&](const Entry& e) {
                if (e.fence_value <= last_completed_fence) {
                    e.deleter();
                    return true;
                }
                return false;
            });
        m_queue.erase(it, m_queue.end());
    }

    Size_t pendingCount() const;
};
```

**验收**：
- ASAN 下连续 resize + 加载/卸载纹理无 UAF
- pending count 不会无限增长（fence 正常推进）
- DX12/Vulkan debug layer 无 "destroying resource still in use" 警告

---

#### Task R1.4：实现 RenderSceneDelta 批量不可变结构

- 文件：`render_scene/render_scene_delta.h` / `.cpp`，修改 `render_scene.h`、`render_command.h`
- 依赖：无（可与 R1.1 并行）

**目标**：替换当前 per-UUID `HashMap<UUID, Bitmask>` dirty map，引入批量不可变的 delta 结构，一次生成、一次消费。

**设计**：详见 4.2 节 `RenderSceneDelta` + `RenderSceneDeltaBuilder`。

**实施步骤**：
1. 新建 `render_scene_delta.h`，定义 `RenderSceneDelta` 和 `RenderSceneDeltaBuilder`
2. 修改 `RenderScene`：
   - 将 `m_pending_primitive_updates` / `m_pending_sprite_updates` 改为 `RenderSceneDeltaBuilder m_delta_builder`
   - `rebuildPipelineSceneData()` 改为接收 `const RenderSceneDelta& delta` 参数
3. 修改 `RenderSystem`：在帧边界调用 `m_delta_builder.build(frame_number)`，产出 delta 后传递给 render thread
4. 修改 `RenderCommand` 处理路径：`addOrUpdatePrimitive()` 等操作写入 `delta_builder`
5. 逐步移除旧的 per-UUID dirty map（保留兼容过渡期）

**优势**：
- 一次消费 → 不存在 R0.2 的 clear 时序 bug
- 连续内存 → cache friendly 遍历
- 可丢弃重复 transform 更新 → 同一个 primitive 移动多次只保留最后一次
- 可序列化 → 便于帧复现（deterministic capture）

**验收**：
- `RenderScene::rebuildPipelineSceneData()` 不再直接访问 mutable pending map
- CPU 和 GPU 阶段消费同一个 const delta 引用
- 同一帧内多次 update primitive transform，最终 delta 只保留最后一次
- delta 结构可通过 JSON dump 输出（调试用）

---

#### Task R1.5：TextureManager 迁移至 UploadRing

- 文件：`render/framework/texture_manager.cpp` / `.h`
- 依赖：R1.2（UploadRing 可用）

**当前问题**：`loadTexture()` 调用 `createTexture()` → 同步读文件 → `GDrawCommandList.createTexture()` + `writeTexture()` → 阻塞执行。所有操作耦合在调用线程上，且直接写入全局 command list。

**实施步骤**：
1. 将 `loadTexture()` 拆分为三个阶段：
   - **IO 阶段**（可在 worker 线程）：读文件、解码像素数据到 CPU buffer
   - **Upload 阶段**（render thread）：从 `FrameContext.upload_ring` 分配 staging 空间、复制像素、录制 `writeTexture` 命令
   - **Transition 阶段**：fence 完成后资源标记为 Resident
2. `TextureManager` 新增方法：
   ```cpp
   struct TextureUploadRequest {
       GfxTextureHandle texture;
       const UInt8* pixel_data;
       UInt64 data_size;
       UInt32 mip_levels;
       GfxTextureFormat format;
   };
   void enqueueUpload(const TextureUploadRequest& request);
   void processUploads(DrawCommandList& cmd_list, UploadRing& ring);
   ```
3. 移除 `TextureManager` 中对 `GDrawCommandList` 的直接引用，改为接收 `DrawCommandList&` 参数
4. Cubemap 加载不再创建专用 command list，改为使用当前帧的 command list

**验收**：
- `TextureManager` 不包含 `GDrawCommandList` 引用
- 纹理加载路径的 GPU command 录制与 draw command 在同一 command list
- PIX 中纹理上传命令归属于正确的 pass marker

---

#### Task R1.6：Frame Telemetry 基础

- 新建文件：`core/frame/render_telemetry.h` / `render_telemetry.cpp`
- 依赖：R1.1（FrameScheduler 提供帧号、in-flight count 等）

**目标**：提供每帧可查询/可导出的性能指标，为后续优化提供量化依据。

**最小指标集**：

| 类别 | 指标 | 来源 |
|------|------|------|
| Frame | frame_number, delta_time_ms, in_flight_count | FrameScheduler |
| CPU | game_thread_ms, render_thread_ms, draw_thread_ms | RenderSystem |
| GPU | gpu_frame_ms（total GPU time） | EventQuery / timestamp |
| Upload | upload_bytes, stall_count, overflow_count | UploadRing |
| Memory | frame_arena_used_mb, frame_arena_peak_mb | FrameSlot arena |
| Draw | draw_call_count, dispatch_count, barrier_count | DrawCommandList 统计 |
| Resource | transient_texture_count, transient_buffer_count | RenderGraph |
| Deletion | pending_deletion_count | DeferredDeletionQueue |

**数据结构**：
```cpp
struct FrameTelemetry {
    UInt64 frame_number;
    Float delta_time_ms;

    // CPU
    Float game_thread_ms;
    Float render_thread_ms;
    Float draw_thread_ms;

    // Upload
    UInt64 upload_bytes;
    UInt32 upload_stall_count;
    UInt32 upload_overflow_count;

    // Memory
    Float frame_arena_used_mb;
    Float frame_arena_peak_mb;

    // Draw
    UInt32 draw_call_count;
    UInt32 dispatch_count;
    UInt32 barrier_count;

    // Deletion
    UInt32 pending_deletion_count;

    // 导出
    String toJSON() const;
};

class FrameTelemetryCollector {
    RingBuffer<FrameTelemetry, 256> m_history{};
public:
    void record(const FrameTelemetry& telemetry);
    const FrameTelemetry& current() const;
    const FrameTelemetry& previous(UInt32 frames_ago = 1) const;
    DynamicArray<FrameTelemetry> lastN(UInt32 n) const;
};
```

**验收**：
- Editor overlay 可显示 frame time、in-flight count、upload bytes
- 帧数据可导出为 JSON（供 CI 性能回归对比）
- 不影响 release 构建性能（telemetry 可用宏开关）

---

#### Task R1.7：消除 GDrawCommandList 全局变量

- 文件：全部 `render/` 目录引用处（约 30+ 引用点）
- 依赖：R0.1b（设计方案已完成）、R1.1（FrameContext 承载 command list）

**目标**：彻底移除 `GDrawCommandList` 全局变量，所有 command 录制通过 `FrameContext` 或显式参数传入的 `DrawCommandList&` 完成。

**迁移步骤**：
1. **Phase 1 — 隐式传递**：将 `GDrawCommandList` 声明改为 `thread_local`（过渡方案，降低风险）
   ```cpp
   // draw_command_list.cpp
   thread_local DrawCommandList t_draw_command_list{};
   DrawCommandList& getThreadDrawCommandList() { return t_draw_command_list; }
   ```
2. **Phase 2 — 显式参数**（逐个模块迁移）：
   - `GpuScene::flushUpdates()` 改为 `flushUpdates(DrawCommandList& cmd_list)`
   - `TextureManager::createTexture()` 改为接收 `DrawCommandList&`
   - 各 RenderFeature 的 `registerPass()` 改为接收 `DrawCommandList&`
   - `ImmediateFrameScope` 改为持有 `DrawCommandList` 并传入
3. **Phase 3 — 最终清理**：
   - 删除 `draw_command_list.cpp` 中的全局/thread_local 变量
   - 所有录制路径统一从 `FrameContext::command_list` 获取

**涉及的关键文件**（按引用频率排序）：
- `render_scene.cpp` — GPU Scene flush
- `texture_manager.cpp` — 纹理创建/上传
- `deferred_renderer.cpp` — pass 录制
- `mesh_draw_command_dispatcher.cpp` — draw command 录制
- `render_graph.cpp` — graph execute 分发
- 所有 `render_feature/*.cpp` — 各 feature pass

**验收**：
- 搜索 `GDrawCommandList` 全代码库零匹配
- 三线程模式下无 command list 竞态
- 每个 draw call 归属于明确的 `FrameContext`

---

**R1 完成标准**：
- CPU 可领先 GPU N 帧但不越界复用资源
- 加载纹理不阻塞渲染主路径
- 每帧 upload bytes / arena usage / in-flight count 可观测
- 全局 `GDrawCommandList` 变量已消除

---

### 3.2 Phase R2：RenderGraph Resource Compiler

#### Task R2.1：扩展 RenderGraphAccessType 与子资源范围

- 文件：[render_graph_resource.h](render_graph_resource.h)
- 依赖：无

**目标**：将当前仅有的 `Read` / `Write` 两态扩展为完整的访问语义描述。

**改动**（详见 4.3.1 节设计）：
1. 枚举新增 `ReadWrite`（UAV 访问）
2. 新增 `RenderGraphPipelineStage` 枚举：`VertexShader`, `PixelShader`, `ComputeShader`, `Copy`, `RenderTarget`, `DepthStencil`
3. 新增 `RenderGraphSubresourceRange` 结构：`base_mip`, `mip_count`, `base_array_layer`, `array_layer_count`
4. 新增 `RenderGraphAccessInfo` 结构：聚合 `access_type`, `stage`, `required_state`, `subresource`
5. 新增 `RenderGraphAttachmentInfo` 结构：`LoadOp`, `StoreOp`, `clear_color`
6. `LoadOp` / `StoreOp` 枚举：`Load`, `Clear`, `DontCare` / `Store`, `DontCare`

**向后兼容**：保留旧 API（`read()` / `write()`）标记为 `[[deprecated]]`，内部转为新 API。

**验收**：
- 新 pass 可以使用 `readTexture()` / `writeColor()` / `writeDepth()` / `writeUav()` API
- `subresource` 默认为全部（不填 = 整资源）

---

#### Task R2.2：RenderGraphPassBuilder 语义化 API

- 文件：[render_graph_pass.h](render_graph_pass.h)
- 依赖：R2.1

**目标**：提供显式语义的 pass builder API，替代当前模糊的 `read()` / `write()`。

**新增方法**（详见 4.3.2 节设计）：
```cpp
// 纹理访问
RenderGraphTextureHandle readTexture(handle, stage, subresource = {});
RenderGraphTextureHandle writeColor(handle, attachment = {});
RenderGraphTextureHandle writeDepth(handle, attachment = {});
RenderGraphTextureHandle writeUav(handle, stage);

// Buffer 访问
RenderGraphBufferHandle readBuffer(handle, stage);
RenderGraphBufferHandle writeBuffer(handle, stage);

// 最终输出标记
void exportTexture(handle, final_state);
```

**验收**：
- 现有 pass（DeferredRenderer features）至少有一个迁移到新 API 作为示例
- 新旧 API 共存期，旧 API 编译产生 deprecation warning

---

#### Task R2.3：编译期 Validation

- 文件：[render_graph.cpp](render_graph.cpp) `compile()` 方法
- 依赖：R2.1

**目标**：在 graph 编译阶段检测资源使用错误，避免运行时黑屏。

**Validation 规则**：

| 规则 | 检测内容 | 错误级别 |
|------|---------|---------|
| 未初始化读取 | 资源在某 pass 被 Read 但之前无 Write/Import | Error |
| 重复写入 | 同一 subresource 被两个 pass 同时 Write（无依赖关系时） | Error |
| 格式不匹配 | Import 资源的实际格式与声明格式不一致 | Error |
| 未使用资源 | 资源被创建但从未被任何 pass 访问 | Warning |
| Write 后未读 | 资源仅被 Write 但从未被 Read（可能是未 export 的输出） | Warning |
| UAV 冲突 | 两个 pass 同时对同一资源 ReadWrite | Error |
| Attachment 冲突 | 两个 pass 同时 writeColor/writeDepth 同一 attachment | Error |

**实现**：
```cpp
void RenderGraph::validateAccesses() {
    for (auto& pass : m_passes) {
        for (auto& access : pass.accesses) {
            auto& resource = m_resources[access.resource_index];
            // 遍历此 resource 的访问历史，检查冲突
            for (auto& prev_access : resource.access_history) {
                checkConflict(prev_access, access);
            }
            resource.access_history.push_back(access);
        }
    }
}
```

**验收**：
- 人为制造 "read before write" → compile() 报 Error 且不执行
- 人为制造 "duplicate write" → compile() 报 Error 且不执行
- 正常场景 compile() 不产生 false positive

---

#### Task R2.4：自动 Barrier 生成

- 文件：[render_graph.cpp](render_graph.cpp)
- 依赖：R2.2（声明了完整 access）、R2.3（validation 通过）

**目标**：根据 pass 的访问声明自动推导并插入 resource state transition barrier，消除手写 `setTextureState` / `commitBarriers`。

**算法**（详见 4.3.4 节）：
1. 维护每个 resource 的 `current_state` 追踪
2. 按拓扑序遍历每个 pass：
   - 对每个 access，计算 `required_state = accessToRequiredState(access)`
   - 如果 `current_state != required_state` → 在此 pass 前插入 transition
   - 更新 `current_state = required_state`
3. 相邻 pass 间的相同 transition 自动合并（只发一次 barrier）
4. 同一 pass 内的多个 pre-barrier 合并为一次 `commitBarriers()`

**access → RHI state 映射**：
```cpp
GfxResourceStates accessToState(RenderGraphAccessType access, RenderGraphPipelineStage stage) {
    switch (access) {
        case Read:
            switch (stage) {
                case VertexShader:   return GfxResourceStates::ShaderResource;
                case PixelShader:    return GfxResourceStates::ShaderResource;
                case ComputeShader:  return GfxResourceStates::ShaderResource;
                case Copy:           return GfxResourceStates::CopySource;
                default: break;
            }
        case Write:
            switch (stage) {
                case RenderTarget:   return GfxResourceStates::RenderTarget;
                case DepthStencil:   return GfxResourceStates::DepthWrite;
                case Copy:           return GfxResourceStates::CopyDest;
                default: break;
            }
        case ReadWrite:             return GfxResourceStates::UnorderedAccess;
    }
}
```

**保留逃生口**：pass 可以在 execute 中插入自定义 barrier（标记为 `ManualBarrier`），与自动 barrier 共存但会在 validation 中产生 warning。

**验收**：
- 至少 2 个现有 pass（如 BasePass + LightingPass）移除了手写 barrier 且渲染结果不变
- PIX/RenderDoc 中 resource state transition 数量与手写版本在同一量级（无退化）
- 跨 pass 的 transition 正确合并

---

#### Task R2.5：Transient Resource Pool

- 文件：[render_graph_resource_resolver.cpp](render_graph_resource_resolver.cpp) / `.h`
- 依赖：R2.3（validation 保证 lifetime 正确）

**目标**：根据资源在 graph 中的 first_pass / last_pass 推导 lifetime，实现池化复用，减少每帧 `createTexture` / `createBuffer` 调用。

**实现步骤**：
1. **Lifetime 推导**（在 `compile()` 中）：
   ```cpp
   struct ResourceLifetime {
       Size_t first_pass;  // 首次被访问的 pass index
       Size_t last_pass;   // 最后被访问的 pass index
       Bool is_exported;   // 是否为 export/present 资源
   };
   ```
   遍历所有 pass 的 accesses，更新每个 resource 的 first/last。

2. **Pool 分配**：
   ```cpp
   class TransientResourcePool {
       struct PooledTexture {
           GfxTextureHandle texture;
           ResourceLifetime lifetime;  // 当前占用者
           GfxTextureDesc desc;
       };
       DynamicArray<PooledTexture> m_pool{};
   public:
       GfxTextureHandle acquire(const GfxTextureDesc& desc, ResourceLifetime lifetime);
       void release(Size_t completed_pass);
   };
   ```
   `acquire()` 先搜索池中 desc 匹配且 lifetime 不重叠的空闲资源；无匹配则创建新资源。
   `release()` 在 pass index 超过资源的 last_pass 后将其归还池中。

3. **不需要 aliasing**（R2.8 延后）：此阶段只需池化，不实现内存 aliasing。

**验收**：
- 连续帧的 `createTexture` 调用次数稳定（不再随帧数线性增长）
- 同一帧内两个不重叠的 pass 可以复用同一个纹理资源
- lifetime 不重叠的 pool 分配不产生 validation error

---

#### Task R2.6：Export Root + Pass Culling

- 文件：[render_graph.cpp](render_graph.cpp) `compile()` 方法
- 依赖：R2.3

**目标**：从 exported/present 资源反向 BFS 标记可达 pass，剔除不可达子树。

**当前状态**：`cullUnreachablePasses()` 仅剔除没有任何资源访问且非 `NeverCull` 的 pass。无法剔除"有资源访问但对最终输出无贡献"的 pass 链。

**实现**：
```cpp
void RenderGraph::cullUnreachablePasses() {
    // 1. 收集所有 exported resources（通过 exportTexture 标记）
    HashSet<UInt32> reachable_resources{};
    for (auto& res : m_resources) {
        if (res.is_exported || res.is_backbuffer) {
            reachable_resources.insert(res.index);
        }
    }

    // 2. 反向 BFS：从 reachable resources 找到所有 producer pass
    HashSet<UInt32> reachable_passes{};
    DynamicArray<UInt32> queue(reachable_resources.begin(), reachable_resources.end());
    while (!queue.empty()) {
        auto res_idx = queue.back(); queue.pop_back();
        for (auto& pass : m_passes) {
            if (reachable_passes.contains(pass.index)) continue;
            // 如果 pass 写入了此资源，pass 是 reachable
            if (pass.writesResource(res_idx)) {
                reachable_passes.insert(pass.index);
                // pass 读取的所有资源也是 reachable（继续 BFS）
                for (auto& read_idx : pass.readResources()) {
                    if (!reachable_resources.contains(read_idx)) {
                        reachable_resources.insert(read_idx);
                        queue.push_back(read_idx);
                    }
                }
            }
        }
    }

    // 3. 标记不可达 pass 为 culled
    for (auto& pass : m_passes) {
        if (pass.flags & NeverCull) continue;
        m_culled_passes[pass.index] = !reachable_passes.contains(pass.index);
    }
}
```

**验收**：
- Debug UI 关闭时，debug draw pass 被自动剔除
- 某个 intermediate pass 的输出不被任何后续 pass 读取 → pass 被剔除
- culled pass 数量和原因可通过 graph dump（R2.7）查看

---

#### Task R2.7：Graph Dump（JSON/DOT 导出）

- 文件：[render_graph.cpp](render_graph.cpp)
- 依赖：R2.4（barrier）、R2.5（lifetime）

**目标**：导出每帧的 graph 结构、barrier、资源 lifetime、显存使用情况，便于离线分析和 CI 回归对比。

**JSON 导出结构**：
```json
{
  "frame": 1234,
  "passes": [
    {
      "name": "GBufferPass",
      "index": 0,
      "level": 0,
      "culled": false,
      "async_compute": false,
      "accesses": [
        {"resource": "GBufferA", "type": "Write", "stage": "RenderTarget"},
        {"resource": "GBufferB", "type": "Write", "stage": "RenderTarget"},
        {"resource": "DepthBuffer", "type": "Write", "stage": "DepthStencil"}
      ],
      "barriers_pre": [
        {"resource": "GBufferA", "from": "Unknown", "to": "RenderTarget"}
      ],
      "duration_us": 1234.5
    }
  ],
  "resources": [
    {
      "name": "GBufferA",
      "type": "texture",
      "format": "RGBA8_UNORM",
      "size": [1920, 1080],
      "first_pass": 0,
      "last_pass": 1,
      "is_transient": true,
      "is_exported": false
    }
  ],
  "stats": {
    "total_passes": 12,
    "culled_passes": 1,
    "total_barriers": 18,
    "transient_textures": 8,
    "transient_buffers": 3,
    "estimated_vram_mb": 156.4
  }
}
```

**DOT 导出**：生成 Graphviz DOT 格式，pass 为节点、依赖为边、资源名标注在边上。

**验收**：
- 按快捷键可导出当前帧的 graph JSON 文件
- DOT 文件可用 `dot -Tpng graph.dot -o graph.png` 渲染
- CI 可对比两帧的 barrier 数量差异

---

#### Task R2.8（延后）：Transient Aliasing + Memory Budget

- 依赖：R2.5

**目标**：在 R2.5 池化基础上，进一步实现内存 aliasing（多个 lifetime 不重叠的资源共享同一物理内存块）。

**不在 R2 必做范围内**。当前先实现池化（R2.5），aliasing 在有显存压力数据支撑后再推进。

---

#### Task R2.9（延后）：Async Compute + Multi-Queue

- 依赖：R2.4

**目标**：将独立的 compute pass 提交到 compute queue，与 graphics queue 并行执行。

**不在 R2 必做范围内**。当前 `AsyncCompute` flag 存在但未被消费。R2.4 的 barrier 推导为跨 queue fence/wait 预留了接口。

---

**R2 完成标准**：
- 常规 pass 不再手写 `setTextureState` / `commitBarriers`（至少 80% pass 覆盖）
- Validation 能报出 "未初始化读取"、"冲突写入" 等错误
- Transient 资源创建数趋近稳定（连续帧不增长）
- Graph dump 可展示 barrier、lifetime、pass culling 信息

---

### R2 完成小结（2026-07-18）

**涉及文件**：

| 文件 | 改动类型 | 关键内容 |
|------|---------|---------|
| [render_graph_resource.h](render_graph_resource.h) | 扩展 | 新增 `ReadWrite`、`RenderGraphPipelineStage`、`RenderGraphSubresourceRange`、`LoadOp/StoreOp`、`RenderGraphAttachmentInfo`、`RenderGraphAccessInfo`、`RenderGraphBarrier`；扩展 `RenderGraphPassResourceAccess`、`RenderGraphResourceRecord` |
| [render_graph_pass.h](render_graph_pass.h) | 扩展 | `RenderGraphPass` 新增 `m_pre_barriers` + `setAutoBarriers/getPreBarriers`；`RenderGraphPassBuilder` 新增 `readTexture/writeColor/writeDepth/writeUav/readBuffer/writeBuffer/exportTexture` 语义化 API |
| [render_graph_builder.h](render_graph_builder.h) | 扩展 | 新增 `exportTexture()` |
| [render_graph_builder.cpp](render_graph_builder.cpp) | 扩展 | 实现所有新 Builder API + `exportTexture` |
| [render_graph.h](render_graph.h) | 扩展 | 新增 `TransientResourcePool` 成员 + `validateAccesses/deriveBarriers/cullUnreachablePasses/dumpToJSON/dumpToDOT` |
| [render_graph.cpp](render_graph.cpp) | 重构 | `accessToRequiredState()` 映射函数；`validateAccesses()` 3 条规则；`deriveBarriers()` per-resource 状态追踪；`cullUnreachablePasses()` 反向 BFS；`execute()` 自动应用 barrier + pool release；`dumpToJSON/dumpToDOT` |
| [render_graph_resource_resolver.h](render_graph_resource_resolver.h) | 扩展 | 新增 `TransientResourcePool` 类 |
| [render_graph_resource_resolver.cpp](render_graph_resource_resolver.cpp) | 扩展 | Pool acquire/release 实现 + `initialize()` 集成 |

**R2 完成标准达成**：
- ✅ 语义化 API 可声明 Read/Write/ReadWrite + 管线阶段 + 子资源范围
- ✅ 编译期验证：未初始化读 → Error，写未读 → Warning，UAV 冲突 → Error
- ✅ 自动 barrier：根据 access 声明推导 transition state，execute 中自动注入
- ✅ Transient pool：同 desc 资源跨帧复用，池满自动扩容
- ✅ Pass culling：从 export/backbuffer 反向 BFS，不可达子树全剔除
- ✅ Graph dump：JSON（pass/resource/barrier 完整信息）+ DOT（Graphviz 可视化）
- ✅ 旧 API（`read/write`）保持兼容，新 API 共存

---

### R3.1 改动摘要（2026-07-18）

**新建文件**：[render/shader/shader_manifest.h](shader_manifest.h) / [shader_manifest.cpp](shader_manifest.cpp), [engine/res/shaders/shader_manifest.json](shader_manifest.json)

- `ShaderManifestEntry` 结构：name + source + entry_point + stage（GfxShaderType）+ platforms
- `ShaderManifest` 类：`loadFromFile()` 解析 JSON，`find()` 按名查找，`getEntries()` 遍历，`StageToExtension()` stage→文件扩展名映射
- `ReadShaderFile()` 从 [core/utils/common.h](common.h) 移入 `shader_manifest.h`，改用 `FileSystem::GetEngineResPath()` 解析完整路径，使用 `DynamicArray<Char>` 替代 `std::vector<char>`
- Manifest JSON 路径改为相对于 `engine/res/`（`"shaders/shader_manifest.json"`），shader binary 路径同样（`"shaders/bin/"`）
- 包含所有 21 个现有 shader + pick_pass / point_light_shadow_pass（共 26 个条目）

**修改文件**：[shader_library.h](shader_library.h) / [shader_library.cpp](shader_library.cpp), [common.h](common.h), [mesh_pass_processor.cpp](mesh_pass_processor.cpp)

- `ShaderLibrary`：21 个成员变量替换为 `UnorderedMap<String, GfxShaderHandle> m_shaders` + `ShaderManifest m_manifest`
- 21 个 getter 改为 map 查找（返回 `GfxShaderHandle` 值而非引用），完全向后兼容
- 新增 `findShader(name)` 公开查询接口，新代码可按名获取 shader 而无需新增 getter
- 新增 `getManifest()` 公开 manifest 访问
- `initialize()` 改为遍历 manifest entries 循环加载，消除所有硬编码
- `reset()` 简化为 `m_shaders.clear()`
- [common.h] 移除 `ReadShaderFile`，已在 `shader_manifest.h` 中用 `FileSystem::GetEngineResPath()` 重写
- [mesh_pass_processor.cpp] include 从 `common.h` 改为 `shader_manifest.h`

**目录结构调整**（本次新建）：
```
render/shader/           # 🆕 Shader 基础设施目录
├── shader_manifest.h    # Manifest 解析 + ReadShaderFile
└── shader_manifest.cpp  # JSON 解析实现
```

> **后续**：原有 `render/framework/` 中的 shader 相关文件（`shader_library`、`shader_parameter`、`pipeline_state_cache`、`pso_key`、`descriptor_table_manager`、`global_samplers`）将在 File Move 轮次统一迁入 `render/shader/`，当前先在原地修改以控制 include 变更范围。

---

### 3.3 Phase R3：Shader / Material / PSO

#### Task R3.1：Shader Manifest 文件格式 + Parser ✅ 2026-07-18

- **实际文件**：[render/shader/shader_manifest.h](shader_manifest.h) / [shader_manifest.cpp](shader_manifest.cpp), [engine/res/shaders/shader_manifest.json](shader_manifest.json)
- 依赖：无

**实施内容**：

1. **`render/shader/shader_manifest.h`** — 数据结构 + 解析器 + `ReadShaderFile`：
   - `ShaderManifestEntry`：name, source（file stem）, entry_point, stage（`GfxShaderType`）, platforms
   - `ShaderManifest`：`loadFromFile()` JSON 解析（nlohmann/json），`find()` 按名查找，`getEntries()` 遍历，`StageToExtension()` stage→文件扩展名映射
   - `ReadShaderFile()` 内联函数：从 `core/utils/common.h` 移入，改用 `FileSystem::GetEngineResPath()` 解析完整路径，使用 `DynamicArray<char>` 返回

2. **`render/shader/shader_manifest.cpp`** — 实现：
   - manifest 路径通过 `FileSystem::GetEngineResPath()` 解析
   - `ParseStage()` 映射：vertex→Vertex, pixel→Pixel, compute→Compute, geometry→Geometry, hull→Hull, domain→Domain
   - `StageToExtension()` 映射：Vertex→.vert, Pixel→.frag, Compute→.comp, Geometry→.geom, Hull→.hull, Domain→.domain

3. **`engine/res/shaders/shader_manifest.json`** — 26 个 shader entries（21 个现有 + pick_pass 2 个 + point_light_shadow_pass 3 个），精简 JSON 数组格式

4. **`shader_library.h` 修改**：
   - 21 个 `GfxShaderHandle` 成员变量 → `UnorderedMap<String, GfxShaderHandle> m_shaders`
   - 新增 `ShaderManifest m_manifest` 成员
   - 新增 `findShader(name)` 公开接口 + `getManifest()` 获取 manifest
   - 21 个旧 getter 改为 inline map 查找（返回 `GfxShaderHandle` 值），完全向后兼容

5. **`shader_library.cpp` 修改**：
   - `initialize()`：加载 manifest → 遍历 entries → 构造路径（`FileSystem::GetEngineResPath()` + `"shaders/bin/"`）→ 加载字节码 → 创建 shader
   - `reset()`：从逐成员置空简化为 `m_shaders.clear()`

6. **`core/utils/common.h`**：移除 `ReadShaderFile` 函数，已由 `shader_manifest.h` 提供

7. **`mesh_draw/mesh_pass_processor.cpp`**：include 从 `common.h` 改为 `render/shader/shader_manifest.h`

**验收**：
- ✅ 新增 shader 只需在 manifest JSON 中添加条目，无需修改 C++ 代码
- ✅ JSON 解析错误有明确路径报错（nlohmann/json 异常捕获）
- ✅ 21 个旧 getter 保持兼容，内部通过 map 查找
- ✅ `findShader(name)` 开放新代码按名获取 shader
- ✅ `ReadShaderFile` 使用 `FileSystem::GetEngineResPath()` 统一路径解析

---

#### Task R3.2：Shader 反射（SPIR-V/DXIL 元数据提取）

- 新建文件：`render/framework/shader_reflection.h` / `shader_reflection.cpp`
- 依赖：R3.1（manifest 提供源文件信息）

**目标**：从编译后的 shader 字节码中自动提取 CBV/SRV/UAV/sampler、push constant、vertex input layout，替代当前手写的 `ShaderParameter` 模板。

**技术路线**：
- DX12：使用 D3DReflect API 从 DXIL 提取反射数据
- Vulkan：使用 SPIRV-Cross 从 SPIR-V 提取反射数据
- 统一为引擎内部格式

**反射输出结构**：
```cpp
struct ShaderReflectionData {
    String shader_name;
    GfxShaderStage stage;

    struct ConstantBufferBinding {
        String name;
        UInt32 slot;
        UInt32 size;
        DynamicArray<ConstantVariable> variables;
    };
    DynamicArray<ConstantBufferBinding> constant_buffers;

    struct TextureBinding {
        String name;
        UInt32 slot;
        GfxDescriptorType type;  // SRV / UAV
        GfxTextureDimension dimension;
    };
    DynamicArray<TextureBinding> textures;

    struct SamplerBinding {
        String name;
        UInt32 slot;
    };
    DynamicArray<SamplerBinding> samplers;

    struct VertexInputAttribute {
        String semantic_name;
        UInt32 semantic_index;
        GfxFormat format;
        UInt32 location;
    };
    DynamicArray<VertexInputAttribute> vertex_inputs;

    Bool uses_push_constants{false};
    UInt32 push_constant_size{0};
};
```

**验收**：
- GBuffer shader 的手写 `ShaderParameter` 与反射结果一致
- 为 hand-authored 的 binding 和反射不匹配报 Warning
- SPIR-V 和 DXIL 双路径输出等价

---

#### Task R3.3：自动生成 Binding Layout

- 文件：`render/framework/binding_layout_generator.h` / `.cpp`
- 依赖：R3.2（反射数据可用）

**目标**：基于反射元数据自动生成 `GfxBindingLayoutHandle`，替代手写 binding layout 定义。

**生成逻辑**：
```cpp
GfxBindingLayoutHandle generateBindingLayout(
    GfxDeviceHandle device,
    const ShaderReflectionData& vertex_reflection,
    const ShaderReflectionData& pixel_reflection)
{
    GfxBindingLayoutDesc layout_desc{};

    // 合并 VS + PS 的 CBV
    HashMap<UInt32, GfxBindingSlot> cbv_slots{};
    auto mergeCBV = [&](const ShaderReflectionData& refl) {
        for (auto& cb : refl.constant_buffers) {
            if (!cbv_slots.contains(cb.slot)) {
                cbv_slots[cb.slot] = {
                    cb.slot, cb.slot,
                    GfxDescriptorType::ConstantBuffer,
                    cb.stage
                };
            } else {
                cbv_slots[cb.slot].stage |= cb.stage;
            }
        }
    };
    mergeCBV(vertex_reflection);
    mergeCBV(pixel_reflection);
    // ... SRV、UAV、Sampler 同理

    return device->createBindingLayout(layout_desc);
}
```

**验收**：
- 至少 3 个 shader 的 binding layout 由反射生成
- 反射生成的手动定义版本行为一致
- 反射发现手动定义不匹配时编译期报错

---

#### Task R3.4：Material 模板/实例系统

- 文件：`render/framework/material.h` 重写
- 依赖：R3.1（shader manifest）、R3.3（自动 binding layout）

**当前问题**：`Material` 是简单 POD struct（仅有 diffuse/albedo 等固定属性），无模板/实例分离、无 permutation 控制。

**目标设计**：

```cpp
// 材质模板 — 定义"这是什么类型的材质"，跨实例共享
struct MaterialTemplate {
    String name;
    String shader_name;              // manifest 中的 shader 名
    HashMap<String, UInt32> permutation_values;  // 变体选择
    GfxRasterizerState rasterizer_state;
    GfxDepthStencilState depth_stencil_state;
    GfxBlendState blend_state;

    // 参数元数据（编辑器可见）
    struct ParameterMeta {
        String name;
        String display_name;
        MaterialParameterType type;  // Float, Float2, Float3, Float4, Color, Texture
        Float default_value[4];
        Float min_value[4];
        Float max_value[4];
    };
    DynamicArray<ParameterMeta> parameters;
};

// 材质实例 — 实际使用的材质，只存参数值
struct MaterialInstance {
        UInt32 template_index;  // 指向 MaterialTemplate
        HashMap<String, Float[4]> float_params{};
        HashMap<String, GfxTextureHandle> texture_params{};

        // 运行时生成的唯一 key（用于 PSO cache lookup）
        GraphicsPipelineCacheKey computePipelineKey() const;
};
```

**与现有代码的兼容**：现有 `Material` POD 作为默认模板的实例参数存在，不破坏现有渲染路径。

**验收**：
- 可以用同一个 shader 模板 + 不同参数创建两个材质实例
- 材质实例切换不触发 PSO 重编译（仅更新 constant buffer 内容）
- 编辑器可显示材质的可调参数列表

---

#### Task R3.5：PSO Disk Cache

- 文件：`render/framework/pipeline_state_cache.cpp` / `.h`
- 依赖：R3.3（binding layout 稳定）

**当前问题**：[pipeline_state_cache.h](pipeline_state_cache.h) 仅在内存中使用 `UnorderedMap` 做缓存，进程重启后全部丢失，导致每帧开头有 PSO 编译卡顿。

**实现**：
1. 定义磁盘缓存文件格式：
   ```
   [Header: magic, version, entry_count]
   [Entry 0: key_hash, data_size, data]
   [Entry 1: key_hash, data_size, data]
   ...
   ```
2. 启动时异步加载磁盘缓存到内存（不阻塞首帧）
3. `resolveGraphicsPipeline()` 先查内存 → 未命中时编译 → 编译完成后追加到磁盘缓存队列
4. 合入缓存：每 N 帧或 shutdown 时将新增条目写入磁盘文件
5. Shader revision 变化时的失效策略：
   - 缓存文件头部包含所有 shader 源文件的 hash
   - 加载缓存时比对 shader hash，不匹配则丢弃整个缓存

**关键数据结构**：
```cpp
class PipelineStateCache {
    UnorderedMap<GraphicsPipelineCacheKey, GfxGraphicsPipelineHandle, ...> m_memory_cache{};
    String m_disk_cache_path{};
    DynamicArray<DiskCacheEntry> m_pending_writes{};

public:
    Bool loadDiskCache(const String& path);
    void flushDiskCache();
    void setShaderRevision(const HashMap<String, UInt64>& shader_hashes);
};
```

**验收**：
- 第二次启动同场景时 `resolveGraphicsPipeline()` 无同步编译
- 修改 shader 源文件后缓存自动失效
- 磁盘缓存文件大小合理（< 50 MB for 50 PSO）

---

#### Task R3.6：Shader 热重载

- 新建文件：`render/shader/shader_hot_reload.h` / `.cpp`
- 依赖：R3.1（manifest）、R3.5（PSO disk cache）

**目标**：编辑器运行时修改 shader 文件后，自动重编译并精准失效相关 PSO，不导致全局 PSO cache 清空。

**工作流**：
1. **文件监控**：使用 `FileWatcher`（或编辑器通知）检测 `.hlsl` / `.glsl` 文件变化
2. **重编译**：调用 `GfxDevice::compileShader()` 重新编译变更的 shader
3. **编译失败处理**：保留旧版本 shader，在 editor console 显示编译错误
4. **PSO 精准失效**：仅失效引用了此 shader 的 PSO cache 条目（通过 shader handle 反向查找）
5. **编译成功**：替换 shader handle，失效对应 PSO，在下一帧自动重建

**实现**：
```cpp
class ShaderHotReload {
    struct LoadedShader {
        GfxShaderHandle handle;
        String source_path;
        UInt64 last_write_time;
        HashSet<GraphicsPipelineCacheKey> dependent_psos;
    };
    HashMap<String, LoadedShader> m_loaded_shaders{};
    PipelineStateCache* m_pso_cache{nullptr};

public:
    void registerShader(const String& name, GfxShaderHandle handle, const String& source_path);
    void checkForChanges();  // 每帧在 editor mode 调用
    Bool hasPendingReload() const;
};
```

**验收**：
- 修改 GBuffer pixel shader → 仅 GBuffer PSO 失效（Lighting/Sprite pass 不受影响）
- 引入编译错误的 shader → 画面不变（使用旧版本），console 显示编译错误
- 修正错误 → 画面更新为新 shader
- 非 editor 构建中 hot reload 代码完全编译剔除

---

#### Task R3.7：Non-Bindless Fallback 路径

- 文件：`render/framework/descriptor_table_manager.cpp` / 各 shader 的 binding 代码
- 依赖：R3.3（binding layout 可自动生成）

**目标**：在不支持 bindless 的设备上（如部分 OpenGL 实现、低端 DX12 设备），走传统的 material binding-set 路径，而非完全关闭渲染功能。

**实现策略**：
1. `ResolvedRenderFeatures.bindless_active` 决定走 bindless 还是 binding-set 路径
2. Bindless 路径：使用 `DescriptorTableManager` 的 descriptor heap + shader 中 `DescriptorHeap[]` 声明
3. Binding-set 路径：每个 material instance 拥有独立的 `GfxBindingSetHandle`，在 draw 时显式绑定
4. Shader 需提供两个 variant：
   - `#define BINDLESS 1` → 走 descriptor heap
   - `#define BINDLESS 0` → 走传统 texture slot
5. `ShaderManifest` 中标记哪些 shader 支持 bindless fallback

**验收**：
- 在 bindless_supported=false 的设备上正常渲染（非黑屏）
- Bindless/Binding-set 路径的渲染结果逐像素对比一致
- 切换路径不需要修改 material asset 数据

---

**R3 完成标准**：
- 新增材质不需要改 `ShaderLibrary` 硬编码成员
- 场景切换无可见 PSO 编译卡顿（disk cache 预热）
- Shader hot reload 不导致全局 PSO cache 清空
- 不支持 bindless 的设备有 graceful fallback

---

### R3 完成小结（2026-07-18）

**新建文件**：

| 文件 | 改动类型 | 关键内容 |
|------|---------|---------|
| [shader/shader_reflection.h](shader_reflection.h) | 新建 | ShaderReflectionData 完整数据结构 + ShaderReflector 静态反射器 + ShaderResourceKind 枚举 |
| [shader/shader_reflection.cpp](shader_reflection.cpp) | 新建 | DXIL 反射（D3DReflect API）+ SPIR-V 反射（自解析字节码）+ ValidateAgainstLayout |
| [shader/binding_layout_generator.h](binding_layout_generator.h) | 新建 | BindingLayoutGenerator：Multi-stage 合并、冲突检测 |
| [shader/binding_layout_generator.cpp](binding_layout_generator.cpp) | 新建 | MergeReflection + Generate + CreateLayout（device/cmd_list 双路径） |
| [shader/shader_hot_reload.h](shader_hot_reload.h) | 新建 | ShaderHotReload：TrackedShader + 时间戳轮询 |
| [shader/shader_hot_reload.cpp](shader_hot_reload.cpp) | 新建 | RegisterShader + PollChanges + 文件变更检测 |
| [shader/binding_set_allocator.h](binding_set_allocator.h) | 新建 | BindingSetAllocator：传统 binding-set 缓存，bindless=off 回退路径 |
| [shader/binding_set_allocator.cpp](binding_set_allocator.cpp) | 新建 | GetOrCreate + 缓存 hash/equal |
| [material/material_system.h](material_system.h) | 新建 | MaterialParamType/Value/Def + MaterialTemplateDesc + MaterialInstanceDesc + MaterialSystem |
| [material/material_system.cpp](material_system.cpp) | 新建 | RegisterTemplate/CreateInstance/GetResolvedParams/BuildConstantBufferData |
| [pipeline/pso_disk_cache.h](pso_disk_cache.h) | 新建 | PsoDiskCache：二进制文件头 + shader hash 失效 |
| [pipeline/pso_disk_cache.cpp](pso_disk_cache.cpp) | 新建 | BeginLoad/Find/Insert/FlushToDisk/InvalidateAll |

**迁移文件**（framework → 新目录）：

| 原路径 | 新路径 |
|--------|--------|
| `framework/shader_library.h/.cpp` | `shader/shader_library.h/.cpp` |
| `framework/shader_parameter.h` | `shader/shader_parameter.h` |
| `framework/descriptor_table_manager.h/.cpp` | `shader/descriptor_table_manager.h/.cpp` |
| `framework/global_samplers.h/.cpp` | `shader/global_samplers.h/.cpp` |
| `framework/pipeline_state_cache.h/.cpp` | `pipeline/pipeline_state_cache.h/.cpp` |
| `framework/pso_key.h/.cpp` | `pipeline/pso_key.h/.cpp` |
| `framework/material.h` | `material/material.h` |

**ShaderLibrary 增强**：[shader/shader_library.h](shader_library.h) / [shader_library.cpp](shader_library.cpp)
- 新增 `m_reflections` 成员：shader 加载后自动反射并缓存
- 新增 `GetReflection(name)` 公开接口

**R3 完成标准达成**：
- ✅ 新增 shader 只需 JSON manifest + 反射自动推导 binding layout（无需手写 ShaderParameter）
- ✅ PSO disk cache 支持二进制持久化 + shader hash 自动失效
- ✅ Shader 热重载基础框架就绪（文件变更检测 + 时间戳比较）
- ✅ Non-bindless fallback：`BindingSetAllocator` 提供传统 binding-set 路径
- ✅ Material 模板/实例系统：参数元数据 + 默认值 + 覆写 + CB 数据构建
- ✅ 目录结构清理：shader/（11 个文件）、material/（3 个文件）、pipeline/（5 个文件）

---

### 3.4 Phase R4：GPU-Driven 与多队列

#### Task R4.1：修复并加固 GPU Scene 增量同步

- 文件：[gpu_scene.h](gpu_scene.h) / [gpu_scene.cpp](gpu_scene.cpp)，[render_scene.cpp](render_scene.cpp)
- 依赖：R0.2（已修复 delta 消费时序）、R1.4（RenderSceneDelta）

**目标**：在 R0.2 修复基础上，将 GPU Scene 的增量同步从"依赖 RenderScene 内部 dirty map"升级为"消费标准化的 RenderSceneDelta"，确保所有对象类型的 GPU 数据上传路径完整。

**当前已修复**：R0.2a/b 已修复 clear 时序并补充了 primitive GPU scene 更新。

**R4.1 增量工作**：
1. `GpuScene::applyDelta(const RenderSceneDelta& delta)` — 统一的增量应用入口
2. 为所有对象类型补齐 GPU 数据更新：
   - Primitive: transform ✅, bounds ✅, PrimitiveGpuData ✅（R0.2b 已完成）
   - Sprite: transform ✅, bounds ✅, SpriteInstanceGpuData ✅（R0.2a 已完成）
   - Light: ❌ GpuScene 中暂无 Light 数据结构 → R4.1 新增 `updateLightGpuData()`
3. `DirtyRange` 机制增强：从简单的 start/end 改为 bitmask 追踪，支持稀疏更新
4. `flushUpdates()` 添加 stats 输出（dirty ranges、上传字节数）→ 配合 R1.6 telemetry

**验收**：
- 添加/删除/修改任意 scene entity → GPU buffer 内容可验证（通过 RenderDoc buffer inspect）
- Dirty range 统计准确（零修改帧无 GPU 上传）
- GPU Scene 的内存占用可查询

---

#### Task R4.2：GPU Frustum Culling — 动态 Object Count + 输出 Compact List

- 文件：[gpu_driven/gpu_driven_renderer.cpp](gpu_driven_renderer.cpp)
- 依赖：R4.1（GPU Scene 数据可靠）、R0.6a（GpuCulling 已可被调用）

**当前问题**（R0.6a 已部分修复 — 加入 `object_count` 参数和动态扩容）：
- `executeCulling()` 的 dispatch 使用实际 `object_count` ✅
- Buffer 按实际数量动态扩容 ✅
- 但 culling 输出仅生成 `visible_objects_buffer`（可见对象索引列表），**未生成 indirect draw args**
- `m_indirect_args_buffer` 已创建但从未填充

**R4.2 工作**：
1. 确认 `GpuCulling::executeCulling()` 在 GPU-driven 路径中被正确调用（R0.6a 已完成）
2. Compute shader 输出 compact visible list（当前已有），验证其格式：
   ```hlsl
   // 输出: g_visible_objects[index] = object_index
   // g_visible_count = 可见对象数
   ```
3. 在 CPU 端 readback `visible_count`（用于 dispatch indirect args 的数量，可选 debug 验证）
4. 将 visible list buffer 传递到下一阶段（indirect args 构建）

**验收**：
- `visible_count` ≤ `object_count`（不可能超过）
- Frustum 外对象 100% 不在 visible list 中
- Frustum 内对象 100% 在 visible list 中（无 false negative）
- 与 CPU frustum culling 结果一致

---

#### Task R4.3：Indirect Args 构建（按 Mesh/Material/Pass 分桶）

- 文件：[gpu_driven/gpu_driven_renderer.cpp](gpu_driven_renderer.cpp)，新增 compute shader
- 依赖：R4.2（visible list 可用）

**目标**：从 compact visible list 出发，按 mesh_id + material_id + pass_type 分桶，为每桶生成 `DrawIndexedIndirectArguments`。

**两阶段 Compute Pipeline**：

**Phase 1 — Count Pass**（统计每桶的 instance count）：
```hlsl
// 输入: visible_list[], object_meta[]
// 输出: bucket_counts[] (初始化为 0)
[numthreads(64, 1, 1)]
void CountCS(uint3 dtid : SV_DispatchThreadID) {
    uint visible_idx = dtid.x;
    if (visible_idx >= visible_count) return;
    uint object_idx = visible_list[visible_idx];
    ObjectMeta meta = object_meta[object_idx];
    uint bucket = getBucketIndex(meta.mesh_id, meta.material_id, meta.pass_type);
    InterlockedAdd(bucket_counts[bucket].instance_count, 1);
}
```

**Phase 2 — Fill Pass**（填充 per-instance data + 生成 indirect args）：
```hlsl
// 输入: visible_list[], object_meta[], prefix_sum(bucket_counts[])
// 输出: indirect_args[], instance_data[]
[numthreads(64, 1, 1)]
void FillCS(uint3 dtid : SV_DispatchThreadID) {
    uint visible_idx = dtid.x;
    if (visible_idx >= visible_count) return;
    uint object_idx = visible_list[visible_idx];
    ObjectMeta meta = object_meta[object_idx];
    uint bucket = getBucketIndex(meta.mesh_id, meta.material_id, meta.pass_type);
    uint slot;
    InterlockedAdd(bucket_counts[bucket].current_offset, 1, slot);
    uint write_idx = bucket_offsets[bucket] + slot;
    instance_data[write_idx] = object_transforms[object_idx];
}
// 最后生成 DrawIndexedIndirectArguments:
// IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation
```

**实现注意事项**：
- 分桶 key：`(mesh_id << 16) | (material_id << 8) | pass_type`
- Indirect args buffer 存储格式：`{index_count, instance_count, start_index, base_vertex, start_instance}[]`
- 需要在 dispatch fill pass 前做一次 prefix sum 计算 bucket offsets（可用 `InterlockedAdd` 或专用 scan shader）

**验收**：
- 生成的 indirect args 数量 = 实际需要绘制的 bucket 数
- Instance count 之和 = visible object count
- 各 bucket 的 mesh/material 参数与 CPU 路径一致

---

#### Task R4.4：Draw Pass 消费 Indirect Args

- 文件：mesh draw pass + RenderGraph
- 依赖：R4.3（indirect args 已填充）

**目标**：Draw pass 通过 `ExecuteIndirect` 消费 GPU 生成的 indirect args，完成 GPU-driven 的最后一环。

**改动点**：
1. 新增 `GfxCommandList::executeIndirect()` 包装（或扩展 `DrawCommandList`）
2. Draw pass 不再遍历 CPU visible list，改为：
   ```cpp
   cmd_list.setBufferState(indirect_args_buffer, GfxResourceStates::IndirectArgument);
   cmd_list.executeIndirect(indirect_args_buffer, args_offset, max_command_count);
   ```
3. 绑定 GPU Scene 的资源为 SRV（transform/instance buffer），而非 CPU 上的 `PrimitiveSceneInfo`
4. 确保 mesh pass processor 的 draw command 构建逻辑与 GPU-driven 路径兼容

**过渡策略**：
- 通过 `ResolvedRenderFeatures.gpu_driven_active` 在 CPU/GPU 路径间切换
- 两路径绘制结果逐像素对比（debug toggle）

**验收**：
- GPU-driven 路径可正常绘制场景（至少与 CPU 路径视觉等价）
- PIX 中 confirm 使用了 `ExecuteIndirect`
- CPU 提交开销（Game/Render thread time）有可量化的下降

---

#### Task R4.5：CPU/GPU 路径 A/B 对比 + 结果等价验证

- 文件：`render/framework/render_debug.h` / RenderSettings 扩展
- 依赖：R4.4（GPU path 可绘制）

**目标**：提供 debug toggle 在 CPU 和 GPU 路径间切换，并自动验证结果等价。

**实现**：
1. RenderSettings 新增：
   ```cpp
   enum class CullingPath {
       CpuOnly = 0,
       GpuOnly,
       CpuThenGpuVerify,  // 两者都执行，GPU 结果与 CPU 比对
   };
   CullingPath culling_path{CullingPath::CpuOnly};  // 默认 CPU（安全）
   ```
2. `CpuThenGpuVerify` 模式下：
   - 同时运行 CPU culling 和 GPU culling
   - Readback GPU visible count 和 visible list 到 CPU
   - 比对两者（允许顺序不同，但元素集合必须相同）
   - 不一致时：日志打印差异（额外/缺失的对象 UUID）
3. 可视化调试：
   - 在 debug overlay 显示 visible_count（CPU vs GPU）
   - 用不同颜色高亮 CPU-only / GPU-only / 两者共有的对象 bounding box

**验收**：
- CPU/GPU visible list 比对 0 差异
- 切换路径不影响画面像素（逐像素 diff 为 0）
- 比较开销在 release 构建中完全剔除

---

#### Task R4.6（可选）：HZB Occlusion Culling

- 文件：[gpu_driven/gpu_driven_renderer.cpp](gpu_driven_gpu_driven_renderer.cpp)，新增 HZB compute shader
- 依赖：R4.2（frustum culling 稳定）

**目标**：在 frustum culling 之后、indirect args 构建之前，通过 Hierarchical Z-Buffer 进一步剔除被遮挡的对象。

**实现步骤**：
1. 从上一帧的 depth buffer 构建 depth pyramid（多级 mip chain，每级取 2×2 区域的最小/最大深度）
2. Occlusion pass：
   - 输入：frustum-visible list
   - 对每个对象的 AABB，在 HZB 中查询最合适的 mip level
   - 如果 AABB 的最近深度 > HZB 采样深度 → 被遮挡 → 从 visible list 移除
3. Camera cut / 场景突变检测：比较当前帧和上一帧的 view-projection 矩阵，差异过大时跳过 HZB
4. 使用上一帧 HZB 进行当前帧 culling（可能有 1 帧延迟的 false negative，但无 false positive — 被遮挡的对象可能仍被渲染一帧，但不会被错误剔除）

**注意**：此任务标记为"可选"，仅在 frustum culling 成为瓶颈（GPU time > CPU time）或场景有大量遮挡时推进。

**验收**：
- 面向墙壁时，墙后对象不可见且不产生 draw call
- Camera cut 时无闪烁（HZB 自动禁用一帧）
- HZB 构建 + 采样的 GPU 时间 < 被节省的 draw pass GPU 时间

---

#### Task R4.7：多 View 统一 FrameGraph

- 文件：[render_graph/](render_graph/) 目录，[render_view.h](render_view.h)，[renderer.cpp](renderer.cpp)
- 依赖：R2 完成（RG resource compiler）

**目标**：将当前"每个 view 独立构建和执行 graph"改为"一个 FrameGraph 包含多个 view subgraph"，实现跨 view 资源共享。

**当前问题**（详见 1.10 节）：
```cpp
for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
    RenderGraphBuilder graph(...);  // 每个 view 独立 graph
    // ...
    graph.compile();
    graph.execute(pool, context, out_commands);  // 独立执行
}
```
导致 GPU Scene upload、shadow map 等被每个 view 重复执行。

**目标结构**（详见 4.7 节设计）：
```
FrameGraph (一个)
  ├─ Shared Upload Pass Group
  │   ├─ GPU Scene update（所有 view 共享）
  │   └─ Upload ring flush
  ├─ Shadow Pass Group（所有 shadow-casting light）
  ├─ View Subgraph: Scene Viewport
  │   ├─ GBuffer → Lighting → PostProcess
  ├─ View Subgraph: Game Viewport
  │   └─ (同上)
  └─ Presentation Pass Group
      ├─ ImGui pass (per swapchain)
      └─ Present
```

**实施步骤**：
1. 在 `RenderGraph` 中引入 subgraph 概念：
   ```cpp
   RenderGraph::addSubgraph(const String& name, RenderView* view);
   ```
2. `RenderGraphBuilder` 支持标记 pass 所属 subgraph
3. `compile()` 统一编译：跨 subgraph 的依赖排序、barrier 推导、transient pool
4. `execute()` 中 subgraph 内的 pass 仍然可以并行录制（只要 level 相同且 subgraph 间无依赖）
5. 移除 `renderer.cpp` 中的 per-view 循环

**验收**：
- GPU Scene upload 每帧只执行一次（而非 N 次 = view 数量）
- Shadow map 在所有 view 之间共享
- 多个 view 的 RG dump 显示在同一个 graph 中
- 任一 view 的 pass 错误不影响其他 view

---

#### Task R4.8（延后）：Async Compute — Copy/Compute Queue Batching

- 文件：RenderGraph + GfxContext queue management
- 依赖：R2.9（RG 支持 async compute）、R4.7（多 view 统一 graph）

**目标**：将独立的 compute pass（如 GPU culling、HZB 构建、post-process compute）提交到 compute queue，与 graphics queue 的绘制并行执行。

**不在 R4 必做范围内**。需要先有 R2.9 的 RG async compute 基础 + R4.7 的统一 graph，再根据 profiling 数据决定哪些 pass 适合 async。

**验收**（延后）：
- GPU timeline 上 compute queue 与 graphics queue 有重叠
- 总帧 GPU time 下降（而非仅 graphics queue time 下降但 compute queue 新增等待）

---

**R4 完成标准**：
- CPU/GPU 路径可 A/B 对比，结果等价
- GPU path 的 CPU 提交成本（render thread time）有量化下降
- GPU 时间不恶化（GPU-driven 的 compute 开销 ≤ 被节省的 CPU draw call 录制开销）
- 多 view 场景的资源上传不再重复

---

## 4. 剩余任务技术设计参考（R3.2-R4.7）

> 以下为尚未实施任务的原始技术设计，实施时需结合实际代码调整。

### 4.7 多 View 统一 FrameGraph 设计

```cpp
// 目标：一个 FrameGraph 包含多个 view subgraph

// 当前问题：每个 view 独立调用 buildViewFamily() + pipeline->render()
// 导致：重复的 GPU Scene upload、shadow map、resource 无法跨 view 共享

// 目标结构：
//
// FrameGraph (一个)
//   ├─ Pass Group: Shared Uploads
//   │   ├─ GPU Scene update（所有 view 共享）
//   │   └─ Upload ring flush
//   ├─ Pass Group: Shadow Views（所有 shadow-casting light）
//   │   ├─ Shadow pass (light 0)
//   │   └─ Shadow pass (light 1)
//   ├─ Subgraph: Scene Viewport
//   │   ├─ Base pass (GBuffer)
//   │   ├─ Lighting pass
//   │   ├─ Sprite pass
//   │   └─ Post process
//   ├─ Subgraph: Game Viewport
//   │   └─ ... (同上)
//   ├─ Subgraph: Editor Overlay
//   │   ├─ Gizmo pass
//   │   └─ Debug draw pass
//   └─ Pass Group: Presentation
//       ├─ ImGui pass (per swapchain)
//       └─ Present (backbuffer transition)
//

struct FrameGraphViewSubgraph {
    RenderView* view;
    DynamicArray<UInt32> pass_indices;  // 此 subgraph 拥有的 pass
};

class MultiViewFrameGraph {
    RenderGraph m_graph{};
    DynamicArray<FrameGraphViewSubgraph> m_subgraphs{};

    DynamicArray<UInt32> m_shared_pass_indices{};  // 不属于任何 view 的 pass

public:
    void addSharedPass(...);    // GPU Scene、shadow、upload
    void addViewSubgraph(RenderView* view, ...);  // 每个 view 的渲染
    void addPresentationPasses(...);  // ImGui、present

    void compile();  // 跨 subgraph 的统一编译
    void execute(ThreadPool& pool, DrawCommandList& out);
};
```

---

### 4.8 Shader Manifest + 反射管线设计

```cpp
// shader_manifest.h

namespace dodoe {

// ---- Manifest 解析产物 ----

struct PermutationDomain {
    String name;                     // e.g. "SKINNED", "USE_NORMAL_MAP"
    DynamicArray<UInt32> values;     // e.g. [0, 1]
    UInt32 default_value;
};

struct ShaderSourceDesc {
    String name;                     // 逻辑名称，如 "GBufferVS"
    String source_path;              // 文件路径，如 "shaders/gbuffer_pass_vs.hlsl"
    String entry_point;              // "main" / "VSMain" 等
    GfxShaderStage stage;
    DynamicArray<String> platforms;  // ["dx12", "vulkan"]
    DynamicArray<PermutationDomain> permutations;
    DynamicArray<String> include_dirs;
    HashMap<String, String> defines; // 全局宏定义
};

struct BindingLayoutDesc {
    String name;                     // 与 shader 关联的 layout 名
    DynamicArray<BindingSpaceDesc> spaces;
};

struct BindingSpaceDesc {
    UInt32 space;
    DynamicArray<BindingSlotDesc> slots;
};

struct BindingSlotDesc {
    UInt32 slot;
    GfxDescriptorType type;          // CBV / SRV / UAV / Sampler
    UInt32 count;                    // 数组长度，1 = 非数组
    GfxShaderStage visibility;       // 哪些 stage 可见
};

// ---- Manifest 文件 ----

class ShaderManifest {
    DynamicArray<ShaderSourceDesc> m_shaders{};
    HashMap<String, BindingLayoutDesc> m_binding_layouts{};
    HashMap<String, UInt64> m_shader_source_hashes{}; // 用于 PSO cache 失效

public:
    Bool loadFromJSON(const String& json_path);
    Bool loadFromDirectory(const String& shader_dir);  // 自动扫描

    const ShaderSourceDesc* findShader(const String& name) const;
    const BindingLayoutDesc* findBindingLayout(const String& name) const;

    DynamicArray<const ShaderSourceDesc*> getShadersByStage(GfxShaderStage stage) const;
    DynamicArray<String> getAllShaderNames() const;

    // 用于 PSO cache 版本管理
    UInt64 computeGlobalShaderHash() const;

    // 热重载支持
    Bool reloadShader(const String& name, const String& new_source_path);
};

// ---- 反射结果 ----

struct ShaderReflection {
    String shader_name;
    GfxShaderStage stage;
    UInt32 push_constant_size{0};

    struct CbVariable {
        String name;
        UInt32 offset;
        UInt32 size;
    };
    struct CbBinding {
        String name;
        UInt32 slot;
        UInt32 size;
        DynamicArray<CbVariable> variables;
    };
    DynamicArray<CbBinding> constant_buffers;

    struct TextureBinding {
        String name;
        UInt32 slot;
        GfxDescriptorType type;
        UInt32 array_size{1};
    };
    DynamicArray<TextureBinding> textures;

    struct SamplerBinding {
        String name;
        UInt32 slot;
    };
    DynamicArray<SamplerBinding> samplers;

    struct VertexInput {
        String semantic;
        UInt32 semantic_index;
        GfxFormat format;
        UInt32 location;
    };
    DynamicArray<VertexInput> vertex_inputs;
};

// ---- 反射器（平台相关实现） ----

class ShaderReflector {
public:
    static ShaderReflection reflectDXIL(const DynamicArray<UInt8>& bytecode);
    static ShaderReflection reflectSPIRV(const DynamicArray<UInt32>& spirv);
    static ShaderReflection reflectCross(const DynamicArray<UInt8>& bytecode, GfxBackend backend);

    // 自动校验反射数据与 manifest 声明的 binding layout 是否一致
    static Bool validateAgainstLayout(
        const ShaderReflection& reflection,
        const BindingLayoutDesc& layout,
        String& out_error);
};

} // namespace dodoe
```

**数据流**：
```
ShaderManifest (JSON)
  → ShaderSourceDesc (C++ struct)
  → GfxDevice::compileShader(source_path, entry, stage, permutations)
  → GfxShaderHandle
  → ShaderReflector::reflectDXIL/SPIRV(bytecode)
  → ShaderReflection
  → BindingLayoutGenerator::generate(reflection)
  → GfxBindingLayoutHandle
  → PipelineStateCache::resolveGraphicsPipeline(layout, ...)
```

---

### 4.9 Material 模板/实例系统设计

```cpp
// material_system.h

namespace dodoe {

enum class MaterialParamType : UInt8 {
    Float = 0,
    Float2,
    Float3,
    Float4,
    Color3,    // HDR color, 存储为 float3
    Color4,    // HDR color + alpha
    Texture2D,
    TextureCube,
    Int,
    Bool,
};

union MaterialParamValue {
    Float f[4];
    Int32 i[4];
    GfxTextureHandle texture;
};

struct MaterialParamDef {
    String name;
    String display_name;             // 编辑器显示名
    MaterialParamType type;
    MaterialParamValue default_value;
    MaterialParamValue min_value;    // 编辑器 slider 范围
    MaterialParamValue max_value;
};

// ---- 材质模板 ----

struct MaterialTemplateDesc {
    String name;
    String shader_name;
    GfxShaderStage shader_stage;     // 主要 stage（vertex/pixel/compute）

    // 渲染状态（模板级默认值，实例可部分覆写）
    GfxRasterizerDesc rasterizer{};
    GfxDepthStencilDesc depth_stencil{};
    GfxBlendDesc blend{};

    // 参数元数据
    DynamicArray<MaterialParamDef> param_defs{};

    // 变体域
    HashMap<String, UInt32> permutation_defaults{};
};

// ---- 材质实例 ----

struct MaterialInstanceDesc {
    String name;
    String template_name;            // 引用 MaterialTemplate

    // 参数覆写（仅覆写与默认值不同的参数）
    HashMap<String, MaterialParamValue> param_overrides{};

    // 变体选择
    HashMap<String, UInt32> permutation_overrides{};

    // 渲染状态覆写（可选）
    Optional<GfxRasterizerDesc> rasterizer_override{};
    Optional<GfxBlendDesc> blend_override{};
};

// ---- 管理器 ----

class MaterialSystem {
    HashMap<String, MaterialTemplateDesc> m_templates{};
    HashMap<String, MaterialInstanceDesc> m_instances{};
    ShaderManifest* m_shader_manifest{nullptr};

public:
    // 模板管理
    Bool registerTemplate(const MaterialTemplateDesc& desc);
    const MaterialTemplateDesc* findTemplate(const String& name) const;

    // 实例管理
    Bool createInstance(const MaterialInstanceDesc& desc);
    const MaterialInstanceDesc* findInstance(const String& name) const;
    void setInstanceParam(const String& instance_name,
                          const String& param_name,
                          MaterialParamValue value);

    // 运行时查询
    void getResolvedParams(
        const String& instance_name,
        HashMap<String, MaterialParamValue>& out_params) const;

    // 构建 constant buffer 数据（从实例参数）
    Bool buildConstantBufferData(
        const String& instance_name,
        const ShaderReflection& reflection,
        DynamicArray<UInt8>& out_cb_data) const;
};

} // namespace dodoe
```

**与 PSO cache 的集成**：
- 材质实例的 render state + shader permutation 组合 → `GraphicsPipelineCacheKey`
- 材质实例参数更新不改变 PSO key，只更新 constant buffer 内容
- 材质模板切换 → 新的 PSO key，触发 cache lookup

---

### 4.10 PSO Disk Cache + 热重载详细设计

```cpp
// pso_disk_cache.h

namespace dodoe {

// ---- Disk Cache 文件格式 ----

struct PsoCacheFileHeader {
    static constexpr UInt32 kMagic = 0x50534F43;  // "PSOC"
    static constexpr UInt32 kVersion = 1;

    UInt32 magic{kMagic};
    UInt32 version{kVersion};
    UInt64 global_shader_hash{0};     // manifest 中所有 shader 源文件的组合 hash
    UInt64 entry_count{0};
    UInt64 data_offset{0};            // entry table 后的数据起始偏移
    // 保留 48 bytes for future
    UInt8 reserved[48]{};
};

struct PsoCacheEntryHeader {
    UInt64 key_hash{0};               // GraphicsPipelineCacheKey 的 hash
    UInt64 data_size{0};
    // 后面紧跟 data_size 字节的序列化 PSO 数据
};

// ---- Cache Manager ----

class PsoDiskCache {
    String m_cache_path{};
    UnorderedMap<UInt64, DynamicArray<UInt8>> m_cache_data{};  // key_hash → pso blob
    DynamicArray<std::pair<UInt64, DynamicArray<UInt8>>> m_pending_writes{};
    UInt64 m_global_shader_hash{0};
    Bool m_loaded{false};

public:
    // 启动时异步加载（不阻塞首帧）
    void beginAsyncLoad(const String& cache_path, UInt64 expected_shader_hash);
    Bool isLoaded() const { return m_loaded; }

    // 查找（仅从已加载的 cache 中查找）
    DynamicArray<UInt8>* find(UInt64 key_hash);

    // 插入（编译完成后调用，暂存到 pending_writes）
    void insert(UInt64 key_hash, const DynamicArray<UInt8>& pso_blob);

    // 将 pending_writes 刷入磁盘（每 N 帧或 shutdown 时）
    void flushToDisk();

    // Shader 变更时全部失效
    void invalidateAll();
};

// ---- Shader 热重载 ----

class ShaderHotReloadSystem {
    struct TrackedShader {
        GfxShaderHandle handle;
        String source_path;
        UInt64 last_write_time;
        HashSet<UInt64> dependent_pso_keys;  // 引用此 shader 的 PSO key
    };

    HashMap<String, TrackedShader> m_tracked{};
    ShaderManifest* m_manifest{nullptr};
    PipelineStateCache* m_pso_cache{nullptr};
    PsoDiskCache* m_disk_cache{nullptr};

    // 文件监控
    struct FileWatchHandle {
        String directory;
        std::function<void(const String& path)> on_changed;
    };
    DynamicArray<FileWatchHandle> m_watches{};

public:
    void registerShader(const String& name,
                        GfxShaderHandle handle,
                        const String& source_path);

    void registerPsoDependency(const String& shader_name, UInt64 pso_key);

    // 每帧在 editor 模式调用
    void pollChanges();

    // 热重载流程:
    // 1. pollChanges() 检测到文件变化
    // 2. 调用 GfxDevice::compileShader() 重新编译
    // 3. 如果编译失败:
    //    - 保留旧 GfxShaderHandle（画面不变）
    //    - 在 editor console 输出错误
    // 4. 如果编译成功:
    //    - 替换 GfxShaderHandle（新 frame 开始使用）
    //    - 遍历 dependent_pso_keys，从 pso_cache 中移除
    //    - 从 disk_cache 中移除
    //    - 下一帧 resolveGraphicsPipeline() 时自动重建
};

} // namespace dodoe
```

---

### 4.11 GPU-Driven 完整数据流设计

```
                  ┌─────────────────────────────────────────────────┐
                  │              GPU Scene (persistent)              │
                  │  ┌───────────┐ ┌───────────┐ ┌───────────────┐  │
                  │  │ Transforms│ │  Bounds   │ │ Instance Data │  │
                  │  │  Buffer   │ │  Buffer   │ │   Buffers     │  │
                  │  └─────┬─────┘ └─────┬─────┘ └───────┬───────┘  │
                  │        │             │               │          │
                  │  ┌─────┴─────────────┴───────────────┴───────┐  │
                  │  │          Object Meta Buffer               │  │
                  │  │  {mesh_id, material_id, visibility_flags} │  │
                  │  └─────────────────┬─────────────────────────┘  │
                  └────────────────────┼────────────────────────────┘
                                       │
                  ┌────────────────────┼────────────────────────────┐
                  │  Phase 1:          │                            │
                  │  Frustum Culling   ▼                            │
                  │  ┌──────────────────────────────────────────┐   │
                  │  │  Compute: gpu_culling_pass_cs            │   │
                  │  │  Input:  transforms, bounds, meta, VP    │   │
                  │  │  Output: visible_list[], visible_count   │   │
                  │  └──────────────────┬───────────────────────┘   │
                  └─────────────────────┼───────────────────────────┘
                                        │ visible_list
                  ┌─────────────────────┼───────────────────────────┐
                  │  Phase 2:           │                           │
                  │  Bucket Count       ▼                           │
                  │  ┌──────────────────────────────────────────┐   │
                  │  │  Compute: bucket_count_cs                │   │
                  │  │  Input:  visible_list[], object_meta[]   │   │
                  │  │  Output: bucket_counts[] + prefix_sum    │   │
                  │  └──────────────────┬───────────────────────┘   │
                  └─────────────────────┼───────────────────────────┘
                                        │ bucket_offsets
                  ┌─────────────────────┼───────────────────────────┐
                  │  Phase 3:           │                           │
                  │  Instance Fill      ▼                           │
                  │  + Indirect Args    ┌──────────────────────────┐│
                  │  ┌──────────────────┤  Compute: bucket_fill_cs ││
                  │  │  Input:          │  visible_list[],         ││
                  │  │  object_meta[],  │  transforms[],           ││
                  │  │  bucket_offsets[]│  bucket_counts[]         ││
                  │  │  Output:         │  instance_data[],        ││
                  │  │                  │  indirect_args[]         ││
                  │  └──────────────────┴──────────────────────────┘│
                  └─────────────────────┬───────────────────────────┘
                                        │ indirect_args
                  ┌─────────────────────┼───────────────────────────┐
                  │  Phase 4:           │                           │
                  │  ExecuteIndirect    ▼                           │
                  │  ┌──────────────────────────────────────────┐   │
                  │  │  Graphics: ExecuteIndirect(               │   │
                  │  │    indirect_args,                         │   │
                  │  │    max_command_count                      │   │
                  │  │  )                                        │   │
                  │  │  SRV: instance_data[], transforms[],      │   │
                  │  │       material data[]                     │   │
                  │  └──────────────────────────────────────────┘   │
                  └─────────────────────────────────────────────────┘
```

**关键同步点**：
1. Phase 1→2：仅需 UAV barrier（`visible_list` 的写后读）
2. Phase 2→3：仅需 UAV barrier（`bucket_counts` 的写后读）
3. Phase 3→4：UAV barrier + indirect argument buffer transition（`UnorderedAccess` → `IndirectArgument`）

**Buffer 生命周期**：
- `visible_list_buffer`：per-frame transient（每帧重新分配），大小 = `max_objects * sizeof(UInt32)`
- `visible_count_buffer`：per-frame transient，大小 = `sizeof(UInt32)`
- `bucket_counts_buffer`：per-frame transient，大小 = `max_buckets * sizeof(BucketCountEntry)`
- `instance_data_buffer`：per-frame transient，大小 = `max_objects * sizeof(InstanceData)`
- `indirect_args_buffer`：per-frame transient，大小 = `max_buckets * sizeof(DrawIndexedIndirectArguments)`

所有 transient buffer 通过 R2.5 的 transient resource pool 管理。

---

---

## 附录 A：已验证的代码文件清单

| 文件 | 行数 | 关键发现 |
|------|------|---------|
| `core/container/command_list.h` | 153 | append 已修复，reset 仍调 AdvanceFrameEpoch |
| `core/memory/memory.h` / `.cpp` | 107 / 217 | 已有 ThreadAllocator 系统，无 per-frame-slot |
| `graphics/draw_command_list.h` / `.cpp` | 491 / 906 | 全局 GDdrawCommandList，ImmediateFrameScope 直接 present |
| `graphics/draw_executor.cpp` | ~30 | 执行后直接 present + clearGarbage |
| `render/render_system.h` / `.cpp` | 65 / 188 | enable_gpu_driven 硬编码 false，无 FrameScheduler |
| `render/render_scene/render_scene.cpp` | 524 | **P0.2 BUG：** clear 在 GPU scene 循环之前 |
| `render/render_graph/render_graph.h` / `.cpp` | 40 / 256 | DAG 调度器，非资源编译器 |
| `render/render_graph/render_graph_resource.h` | 104 | 仅 Read/Write 访问类型 |
| `render/render_graph/render_graph_pass.h` | 131 | AsyncCompute flag 未使用 |
| `render/gpu_driven/gpu_scene.h` / `.cpp` | 115 / 335 | DirtyRange + ensure 模式 ✓，但增量同步因 P0.2 而断开 |
| `render/render_pipeline/deferred_renderer.h` / `.cpp` | 37 / 267 | Feature 组合清晰，barrier 完全手写 |
| `render/render_pipeline/render_feature/render_feature.h` | 26 | IRenderFeature 接口简洁（只有 registerPass） |
| `render/framework/texture_manager` | — | 同步加载，直接写 GDdrawCommandList |
| `render/framework/shader_library` | — | 21 个硬编码 shader，现已改用 manifest 驱动加载 |
| `render/framework/pipeline_state_cache` | — | 内存 cache 仅，无 disk cache |
| `render/framework/descriptor_table_manager` | — | Texture SRV 表，无 generation/多类型 |
| `core/frame/frame_context.h` | 15 | 已有 frame_number，arena 待 R1 |

## 附录 B：与 Roadmap 文档的差异汇总

| Roadmap 判断 | 验证结果 | 修复状态 |
|-------------|---------|---------|
| append 不清空 other | ✅ 已修复（验证前） | — |
| reset 调 ResetFrame | ⚠️ 部分修复 | ✅ R0.1a：移除 AdvanceFrameEpoch，改到帧边界 |
| 只有一个 s_frame_allocator | ⚠️ 已改进 | ⬜ per-frame-slot 隔离待 R1.2 UploadRing |
| GPU Scene clear 在 consume 前 | ❌ 确认 BUG | ✅ R0.2a/b 已修复 |
| GPU-driven 永远 false | ❌ 确认 BUG | ✅ R0.3a-c 已修复 |
| 无 FrameScheduler | ❌ 确认 | ✅ R1.1 已实现（kMaxFramesInFlight=3 + GPU fence） |
| RenderGraph 无资源编译 | ❌ 确认 | ✅ R2 已实施（AccessType 扩展 + Barrier 推导 + Validation + Transient Pool + Pass Culling + Graph Dump） |
| 无 RenderSceneDelta | ❌ 确认 | ✅ R1.4 已实施 |

## 附录 C：R1-R4 目标文件清单

| 文件（新建/修改） | 行数（预估） | 对应任务 | 关键改动 |
|------|------|---------|---------|
| `core/frame/frame_scheduler.h/.cpp` | ~200 / ~300 | R1.1 ✅ | FrameSlot + fence + deferred deletion |
| `core/frame/upload_ring.h/.cpp` | ~120 / ~250 | R1.2 | Ring buffer allocator + persistent map |
| `core/frame/deferred_deletion.h/.cpp` | ~60 / ~100 | R1.3 | 独立 deletion queue 组件 |
| `render/render_scene/render_scene_delta.h/.cpp` | ~100 / ~150 | R1.4 | RenderSceneDelta + DeltaBuilder |
| `render/framework/texture_manager.cpp` | ~400 → 修改 | R1.5 | 异步加载 + UploadRing 集成 |
| `core/frame/render_telemetry.h/.cpp` | ~80 / ~120 | R1.6 | FrameTelemetry + Collector |
| `graphics/draw_command_list.cpp` | ~900 → 修改 | R1.7 | 移除全局变量，改为参数传递 |
| `render/render_graph/render_graph_resource.h` | ~104 → ~200 | R2.1 | AccessType/Stage/Subresource/Attachment |
| `render/render_graph/render_graph_pass.h` | ~131 → ~250 | R2.2 | 语义化 readTexture/writeColor 等 API |
| `render/render_graph/render_graph.cpp` | ~256 → ~600 | R2.3-2.4 | Validation + barrier 推导 |
| `render/render_graph/render_graph_resource_resolver.cpp` | ~修改 | R2.5 | Transient pool + lifetime |
| `render/shader/shader_manifest.h/.cpp` | ~90 / ~95 | R3.1 ✅ | JSON manifest parser + ReadShaderFile |
| `render/shader/shader_reflection.h/.cpp` | ~100 / ~300 | R3.2 | DXIL/SPIRV 反射 |
| `render/shader/binding_layout_generator.h/.cpp` | ~60 / ~150 | R3.3 | 反射 → binding layout |
| `render/framework/material.h` | ~80 → ~250 | R3.4 | 模板/实例分离 |
| `render/framework/pipeline_state_cache.h/.cpp` | ~40 → ~200 | R3.5 | Disk cache + shader revision |
| `render/framework/shader_hot_reload.h/.cpp` | ~80 / ~200 | R3.6 | File watcher + 精准 PSO 失效 |
| `render/framework/descriptor_table_manager.h/.cpp` | ~60 → ~180 | R3.7 | Binding-set fallback + 多类型 |
| `render/gpu_driven/gpu_scene.h/.cpp` | ~115 → ~200 | R4.1 | applyDelta + dirty 增强 |
| `render/gpu_driven/gpu_driven_renderer.h/.cpp` | ~55 → ~250 | R4.2-4.3 | Culling + indirect args compute |
| `render/render_graph/` 目录 | ~修改 | R4.7 | 多 view 统一 graph |
