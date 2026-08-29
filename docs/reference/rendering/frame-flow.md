# 每帧流程(Frame Flow)

本文按执行顺序拆解一帧从"游戏 tick"到"像素上屏"的完整流程。行号参考 `render_system.cpp`。

## 1. 帧驱动入口

帧循环由 `SystemContext` 驱动(system_context.cpp):

```cpp
tickOneFrame():
    startRuntime()        → beginMainThreadFrame()   // 双线程下主线程拿回 GL 上下文
    updateTick(dt)        → World 系统(可能 TaskScheduler 并行)+ 各引擎模块 update
    renderTick()          → RenderSystem::submitFrame()
```

`RenderSystem::submitFrame()`(render_system.cpp:94):

1. 单线程(Debug 开关):主线程 `executeFrameOnce()` 原地渲染;
2. 双线程(默认):先 `releaseApplicationGraphicsContext()`(交出 GL 上下文),再 `submitAndWait()`(渲染线程执行,主线程阻塞等完成)。

## 2. renderFrame 逐步时序(render_system.cpp:159)

```text
① GfxRenderScope render_scope
② Memory::ResetFrame()
③ 窗口尺寸检查(像素尺寸 ≤0 直接返回)
④ drain 全局延迟资源命令(GDrawCommandList)
⑤ resize 检测与交换链重建
⑥ 消费游戏线程命令队列 → RenderScene
⑦ acquireNextSwapchainImage
⑧ frame_scheduler->beginFrame(image_index) → FrameContext
⑨ scene->flushUpdates(*frame_ctx.command_list)
⑩ buildViewFamily + pipeline->render(每个 view target)
⑪ 命令执行 + present
```

各步细节:

**①② Scope 与帧内存**:标记当前线程为"RHI 提交线程"(资源创建走立即路径);重置帧线性分配器。

**④ drain 延迟资源命令**(176-186):游戏线程/主线程 update 阶段经 `GDrawCommandList` 录制的 `CreateTextureCommand/CreateBufferCommand` 等在此被移出并在渲染线程执行(实体化 + 上传)。这是 worker 线程创建的资源变为 `isGpuReady()` 的时刻——保证本帧管线引用它们时已就绪。

**⑤ resize**(188-212):遍历 `RenderViewManager` 的 target,窗口/像素尺寸不符者 `target->resize()`;有 dirty 则 `waitForIdle` → `retireCompletedFrames`(强制回收 in-flight 帧)→ `recreateSwapchain` → `pipeline->onResize` → `clearGarbage`。

**⑥ 场景消费**(216-220):`while (m_game_command_queue.tryPop(cmd)) applyRenderCommand(*scene, cmd)`——把游戏线程入队的 Add/Remove Primitive/Sprite/Light、SubmitUI 回放为 `RenderScene` 增删改(带脏标记 diff,见 resources.md)。

**⑦⑧ 交换链与帧槽**(222-232):`acquireNextSwapchainImage` 取后台缓冲索引;`m_frame_scheduler->beginFrame(image_index)` 返回 `FrameContext`(见第 3 节)。同时取 `TimeSystem` 的当前时间与 delta 供 view family 使用。

**⑨ 场景刷写**(235):`scene->flushUpdates(*frame_ctx.command_list)`——处理 pending 的 primitive/sprite/light 更新(有每帧 upsert 预算:primitive 16 / sprite 64),同步到 `GpuScene` 的 CPU 镜像数组并按脏区间上传 GPU 缓冲。

**⑩ 视图与管线**(237-270):对每个 `RenderViewTarget`:

- 从相机注册表取 view/proj(editor 相机特判),`viewport.buildViewFamily(...)` 物化为 `RenderViewFamily`(内部做视锥剔除,写 ViewExtension);
- `pipeline->render(family, scene, swapchain_image_index, *frame_ctx.command_list, frame_ctx.staging, frame_ctx.transient_resource_pool)`——RenderGraph 构建 + pass 执行 + 命令录制(详见 render-pipeline.md)。

**⑪ 提交与上屏**(272-284):

- TripleThread:`endFrame(frame_ctx)`(打 event query)→ `draw_thread->submit(std::move(frame_ctx))`——DrawThread 异步执行命令流并 present;
- 本线程执行(主线程或渲染线程)——复用持久 gfx_cmd:`open → frame_ctx.command_list->execute(gfx_cmd) → close → executeCommandList` → `setEventQuery` → `presentSwapchainImage` → `clearGarbage`。

## 3. RenderFrameScheduler:帧槽与 in-flight(render_frame/)

`kMaxFramesInFlight = 3`,环形复用 `FrameSlot`:

| 成员 | 作用 |
|---|---|
| `frame_number` | 槽位帧号 |
| `Scope<FrameStagingAllocator> staging` | 每帧临时上传分配器(staging buffer 映射内存) |
| `RenderGraphTransientPool transient_resource_pool` | 瞬态纹理/缓冲池,帧间复用(见 render-pipeline.md) |
| `completion_query` | GPU event query,标记帧完成 |
| `DrawCommandList command_list` | 本帧命令流(pass 录制目标) |

`beginFrame(image_index)`(render_frame_scheduler.cpp:33):

1. 当前槽若 in-flight:`waitEventQuery(completion_query)`(等 GPU 完成)+ reset;
2. `m_deletion_queue.processCompleted(slot.frame_number)`——延迟删除与 GPU 完成挂钩(安全的资源销毁);
3. `command_list.beginFrame()`(重置命令流)、`transient_resource_pool.releaseAll()`(池化资源标记未用)、`staging->reset()`;
4. 推进槽位,返回 `FrameContext{ command_list, swapchain_image_index, frame_number, completion_query, staging, transient_resource_pool, valid }`。

`endFrame` 打 query 标记完成;`retireCompletedFrames` 用于 resize 时强制回收全部槽。

**帧资源生命周期**:`beginFrame 复用槽位 → 帧内录制/执行 → GPU 完成(query)→ 延迟删除队列消化 → 槽位再次复用`。任何跨帧引用瞬态资源的代码都违反此契约。

## 4. 两模式下的帧重叠

```text
单线程:         [update][renderFrame+submit+present][update][...]
双线程(默认):   主线程 [update]      [等待]
                渲染线程              [renderFrame+submit+present]
                (锁步:每帧同步一次,无重叠)
```

## 5. 帧内时间线示例(双线程 + worker 加载纹理)

```text
t0  主线程 updateTick:SpriteRendererSystem(worker 线程)发现新实体引用未加载纹理
t1  worker:ResourceManager::LoadTexture2D → TextureBlob(stb 解码,worker 上进行)
t2  worker:GDrawCommandList.createTexture(desc, pixels)
      → 创建 Proxy + 录制 CreateTextureCommand(数据已拷入命令)——立即返回
t3  worker:RenderCommandQueue::AddSprite(...) → MPMC 队列
t4  主线程 submitFrame → releaseContext → submitAndWait
t5  渲染线程 acquireContext → renderFrame:
      drain:t2 的命令执行,纹理实体化(isGpuReady = true)
      场景消费:t3 的 Sprite 入 RenderScene
      flushUpdates → GpuScene 上传实例数据
      buildViewFamily(剔除)→ RenderGraph 构建/执行(pass 录制命令)
      执行命令流 → present
t6  主线程被唤醒,下一帧
```

纹理从 worker 请求到 GPU 可用存在**一帧延迟**;实体化前渲染侧对该资源的行为:不画(skip)、绑定 fallback 或由缓存重试,均不阻塞也不崩溃。
