# 渲染多线程模型

Dodoe 的线程架构:游戏逻辑多线程并行,GPU 提交收敛到单线程。本文覆盖线程角色、同步机制、OpenGL 上下文所有权,以及"任意线程创建资源"的时序保证。

## 1. 线程角色总览

```text
主线程(MainThread)
  ├─ 引擎 tick:SystemContext::tickOneFrame → updateTick(游戏逻辑)→ renderTick(提交帧)
  ├─ TaskScheduler worker 池 ── World 系统(SystemGraph 按读写依赖分层并行)
  └─ SingleThread 模式下兼做渲染

RenderThread(仅 Dual/TripleThread 启动物理线程)
  └─ renderFrame():场景消费 → RenderGraph 构建与执行 → 命令录制

DrawThread(仅 TripleThread)
  └─ DrawExecutor:命令流回放到后端队列 + present(与 RenderThread 隔 2 帧 in-flight 队列)

ThreadPool(每个 Renderer 自有,hardware_concurrency)
  └─ RenderGraph 并行执行 pass(仅 D3D12/Vulkan;GL 恒走串行 direct_mode)
```

## 2. ThreadingMode(render_settings.h:32)

枚举值:`TripleThread=0, DualThread=1, SingleThread=2`,默认 TripleThread。其他相关默认:api=D3D12、pipeline=Deferred、present=Mailbox。

**关键规则**(render_settings.cpp:17):`api == OpenGL && threading_mode == TripleThread` 时**强制降级为 DualThread**——GL 只有一个上下文,渲染线程与绘制线程无法同时提交。

三模式的语义:

| 模式 | 帧构建 | 命令执行 + Present | 主线程阻塞? |
|---|---|---|---|
| SingleThread | 主线程(`executeFrameOnce` 原地执行) | 同上 | 是(同步) |
| DualThread | RenderThread | RenderThread | 是(`submitAndWait`) |
| TripleThread | RenderThread | **DrawThread** | 否(`submit` 后立即返回,流水线化) |

## 3. RenderThread / DrawThread(runtime/core/thread/)

### RenderThread(render_thread.h/.cpp)

单帧锁步协议,成员:`m_has_pending_frame`、`m_frame_completed` + mutex/condition_variable。

- `start(mode)`:SingleThread **不启动物理线程**,其余 spawn `loop()`。
- `submit()`:置 pending、notify、不等待(TripleThread)。
- `submitAndWait()`:submit 后 `m_cv.wait(m_frame_completed)`——主线程阻塞到渲染线程完成本帧(DualThread)。
- `executeFrameOnce()`:调用方线程原地执行帧任务(SingleThread)。
- `loop()`:标记线程名 → `Memory::InitThread()` → **帧内存 epoch 惰性重置** → 等待 pending → 执行 `m_frame_task()`(即 `renderFrame` lambda)→ 置 completed、notify。

DualThread 模式下 frame_task 是 `renderFrameOnRenderThread`:进入时 `acquireApplicationGraphicsContext()`,退出时 `releaseApplicationGraphicsContext()`(shutdown_task)。

### DrawThread(draw_thread.h/.cpp)

- `SpscQueue<FrameContext, 2>`:单生产者(RenderThread)/单消费者,`kMaxFramesInFlight = 2`。
- `loop()` 持续 pop 帧 → `DrawExecutor::execute(device, gfx, frame_ctx)`:把 RenderThread 录制的 `frame_ctx.command_list` 回放到后端命令列表 → `executeCommandList` → `setEventQuery` → **`presentSwapchainImage`** → `clearGarbage`。
- 即 TripleThread 下 RenderThread 只构建帧,DrawThread 负责提交与上屏,两者错开 1-2 帧。

### 帧内存管理(Memory::InitThread / ResetFrame)

所有工作线程统一模式:

- `thread_local ThreadAllocator{ LinearAllocator frame(64KB), scratch(16KB), last_reset_epoch }`。
- `Memory::ResetFrame()`(每帧 renderFrame 调用):遍历所有已注册 ThreadAllocator 强制 `frame.reset()` 并推进全局 epoch。
- 各线程循环顶部(RenderThread::loop、TaskScheduler::workerLoop、ThreadPool worker)**惰性重置**:比较 `Memory::CurrentFrameEpoch()`,`exchange` 成功者自行 reset——补齐错过 ResetFrame 的线程。
- 线程退出 `Memory::ShutdownThread()`。

## 4. TaskScheduler 与 World 系统并行(runtime/core/async/task_scheduler.h)

- 单例,worker 数 = `hardware_concurrency`;`submit(F, args...)`(fire-and-forget)、`async(...)`(返回 future)、`waitAll()`(哨兵任务)、`parallelFor(begin, end, fn)`(切 chunk 提交 + 主线程自旋等待)。
- **World 系统调度**(world.cpp:169-221 `ExecuteSystemsParallel`):
  1. 系统通过 `getAccess()` 声明 `readsComponents/writesComponents`(entt type_hash)。
  2. `BuildGraphForSystems`(world.cpp:48)按组件读写依赖建边(写→读、写→写),拓扑分层(`TaskGraph::getLevels()`)。
  3. 单节点层在当前线程直接执行;多节点层对每个系统 `TaskScheduler::submit`,主线程自旋 `yield` 等整层完成(**层级屏障**)。
  4. 逃生门:`World::IsForceSequential()`(默认 false)强制全部串行——调试渲染线程问题时常用。
  5. 全部层完成后统一应用世界命令缓冲。

## 5. OpenGL 上下文所有权(DualThread 时序)

GL 上下文同一时刻只能在一个线程 current。引擎用 `OpenGLBackend::acquireContext/releaseContext`(互斥锁 + owner thread::id)在主线程与渲染线程之间**显式转移**:

```text
每帧(DualThread + OpenGL):
  主线程   beginMainThreadFrame() → acquireContext   (上下文 current 于主线程)
  主线程   updateTick():游戏逻辑 + ImGui + GDrawCommandList 录制
  主线程   renderTick() → submitFrame() → releaseContext(交出上下文)
  渲染线程 acquireContext(owner 原子转移)
  渲染线程 renderFrameOnRenderThread() → renderFrame()
  渲染线程 releaseContext
  主线程   submitAndWait() 返回,进入下一帧
```

- SingleThread:全程主线程持有,无转移。
- TripleThread:GL 被强制降级为 DualThread(见第 2 节),DrawThread 只用于可多线程提交的后端。
- 非 OpenGL 后端:acquire/release 均为 no-op。

## 6. GfxRenderScope:资源创建的线程闸门

**问题**:GL 上下文线程绑定 + 单一上下文,而资源创建(纹理/缓冲/framebuffer/binding set)是设备调用——游戏 worker 线程若直接创建 GPU 对象必然崩溃或未定义。

**机制**(gfx_context.h/.cpp):

- `thread_local Bool t_in_render_scope`;RAII 类 `GfxRenderScope` 构造置 true / 析构置 false;静态查询 `GfxContext::inRenderScope()`。
- 进入点共两处:
  1. `RenderSystem::renderFrame()`(render_system.cpp:161)——渲染帧入口,覆盖 RenderThread(或 SingleThread 主线程)整帧;
  2. `RenderGraph::execute` 并行分支的**每个 pass worker lambda 内**(render_graph.cpp:507)——D3D12/Vulkan 下 pass 在 ThreadPool 线程执行(D3D12 设备调用线程安全;GL 不走并行分支)。

**资源创建决策表**(draw_command_list.cpp):

| 调用发生处 | createTexture/createBuffer | createFramebuffer/createBindingSet |
|---|---|---|
| scope 内(渲染帧线程/pass 线程) | 立即 `initializeRHI`,同帧可用 | 依赖资源全部就绪时立即实体化 |
| scope 外(游戏线程,主线程 update 阶段) | 录制 `CreateTextureCommand/CreateBufferCommand`(数据内联拷贝) | 返回未就绪 Proxy |

**跨线程资源时序保证**:

```text
worker 线程(游戏逻辑中加载纹理):
  cmd_list.createTexture(desc, data)   → 只创建 Proxy,录制延迟命令(数据已拷贝)
  下帧渲染线程 renderFrame 开头:
  detachRecordedCommands()             → 锁内移出整条命令流
  execute → CreateTextureCommand::execute
           → initializeRHI + writeTexture  (在渲染线程实体化)
  本帧管线引用该纹理时 isRHIReady()==true
```

配套防线:

- **执行时守卫**:所有延迟命令 `execute()` 检查 `isRHIReady()`,未就绪跳过并告警(跨线程录制顺序差异下不崩溃)。
- **缓存重试**:`FramebufferCache`/`BindingSetCache` 命中未就绪条目时重建,下帧自然修复。
- **渲染跳过**:`SetGraphicsStateCommand` 执行时跳过未就绪的 framebuffer/binding set(该帧不画,不崩)。

## 7. 线程间通信通道汇总

| 通道 | 生产者 → 消费者 | 实现 |
|---|---|---|
| 渲染场景命令 | 游戏线程 → RenderThread | `RenderSystem::m_game_command_queue`(`MpmcQueue<RenderCommand, 256>`),`RenderCommandQueue::AddSprite/...` 入队,renderFrame 开头 `tryPop` 批量回放 |
| 延迟资源命令 | 游戏线程/主线程 → 渲染线程 | `GDrawCommandList` 命令流 + `recordCommand` 锁,`detachRecordedCommands()` 移交 |
| 帧移交 | RenderThread → DrawThread | `SpscQueue<FrameContext, 2>` |
| pass 并行 | RenderGraph → ThreadPool | `pool.enqueue` + `WaitGroup`(按层级屏障) |
| 相机数据 | CameraSystem → 渲染侧 | `GetCameraRegistry()` 全局通道(不经命令队列) |
| 帧完成 | 渲染线程 → 主线程 | RenderThread condition_variable(`submitAndWait`) |
| GPU 完成 | 驱动 → CPU | `completion_query`(event query),帧槽复用前等待 |

## 8. 已知约束与注意点

1. **GL 后端必须串行提交**:direct_mode 渲染图(pass 内联)+ 单上下文转移,任何绕过 `GfxRenderScope` 在 worker 线程触发设备调用的代码都会破坏不变式。
2. D3D12/Vulkan 下 pass 线程并发创建资源依赖设备内部线程安全(cutie d3d12 使用锁;D3D12 API 本身允许)。
3. `RenderThread::m_draw_thread` 成员是死代码(从未赋值)。
4. 调试线程问题优先尝试:`World::SetForceSequential(true)`(游戏侧串行)与把 `ThreadingMode` 降为 SingleThread,隔离变量。
