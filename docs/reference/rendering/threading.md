# 渲染多线程模型

Dodoe 的线程架构:游戏逻辑多线程并行,GPU 提交收敛到单线程。本文覆盖线程角色、同步机制、OpenGL 上下文所有权,以及"任意线程创建资源"的时序保证。

## 1. 线程角色总览

```text
主线程(MainThread)
  ├─ 引擎 tick:SystemContext::tickOneFrame → updateTick(游戏逻辑)→ renderTick(提交帧)
  ├─ TaskScheduler worker 池 ── World 系统(SystemGraph 按读写依赖分层并行)
  └─ 调试强制单线程模式下兼做渲染

RenderThread(默认双线程模式启动物理线程)
  └─ renderFrame():场景消费 → RenderGraph 构建与执行 → 命令录制 → 提交 + present

ThreadPool(每个 Renderer 自有,hardware_concurrency)
  └─ RenderGraph 并行执行 pass(仅 D3D12/Vulkan;GL 恒走串行 direct_mode)
```

## 2. 线程模式(render_settings.h)

默认**双线程**(MainThread + RenderThread)。无线程模式枚举;唯一开关是 Debug 构建下的 `RenderSettingsInitInfo::enable_single_thread`(配置字段 `render_settings.enable_single_thread`),`RenderSettings::IsSingleThread()` 为 true 时整条渲染路径收敛到主线程。Release 构建下 `IsSingleThread()` 恒为 false(`#ifdef DODOE_DEBUG_ENABLED` 隔离,编译期消除)。

两模式语义:

| 模式 | 帧构建 | 命令执行 + Present | 主线程阻塞? |
|---|---|---|---|
| 单线程(仅 Debug 开关) | 主线程(`executeFrameOnce` 原地执行) | 同上 | 是(同步) |
| 双线程(默认) | RenderThread | RenderThread | 是(`submitAndWait`) |

## 3. RenderThread(runtime/core/thread/)

### RenderThread(render_thread.h/.cpp)

单帧锁步协议,成员:`m_has_pending_frame`、`m_frame_completed` + mutex/condition_variable。

- `start(spawn_thread)`:双线程 spawn `loop()`,单线程不启动物理线程。
- `submitAndWait()`:submit 后 `m_cv.wait(m_frame_completed)`——主线程阻塞到渲染线程完成本帧(双线程)。
- `executeFrameOnce()`:调用方线程原地执行帧任务(单线程)。
- `loop()`:标记线程名 → `Memory::InitThread()` → **帧内存 epoch 惰性重置** → 等待 pending → 执行 `m_frame_task()`(即 `renderFrame` lambda)→ 置 completed、notify。

双线程模式下 frame_task 是 `renderFrameOnRenderThread`:进入时 `acquireApplicationGraphicsContext()`,退出时 `releaseApplicationGraphicsContext()`(shutdown_task)。

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

## 5. OpenGL 上下文所有权(双线程常驻)

GL 上下文同一时刻只能在一个线程 current。双线程模式下上下文**常驻渲染线程**:首次进入渲染帧时 `acquireContext`,整个运行期不再交接;渲染线程退出时 shutdown_task `releaseContext` 交还主线程做 teardown。

```text
启动:  GfxContext::Create(主线程 current) → setupRenderThreading → releaseContext(清主线程所有权)
运行:  渲染线程 首帧 acquireContext(此后常驻)
       主线程 updateTick/renderTick:只录制命令,零设备调用
       渲染线程 renderFrameOnRenderThread() → renderFrame()(设备调用全部在此线程)
关闭:  渲染线程 stop → shutdown_task → releaseContext → 主线程 acquire → waitForIdle/销毁
```

- 单线程:全程主线程持有,无转移。
- 非 OpenGL 后端:acquire/release 均为 no-op。

## 6. 资源创建:按线程分流

**问题**:资源创建(Texture/Buffer)是设备调用,而调用方分属两类线程。参考 Unity/UE 的模式,按**调用方所在线程**分流:

**两条入口**:

| API | 语义 | 使用方 |
|---|---|---|
| `RenderCommandQueue::CreateTexture/CreateBuffer` | **Request**:创建未就绪 Proxy + 数据内联拷贝 + 入队 RenderCommand;渲染线程 ⑥ 回放时 `initializeGpu` 直接创建 | **游戏线程**(worker 资产加载、主线程)——`mesh.cpp`、`physics_debug.cpp` |
| `GDrawCommandList.createTexture/createBuffer` | **直接创建**:当场 `initializeGpu` + 上传,同帧可用 | **渲染线程**——`gpu_scene.cpp`、`mesh processor`、`imgui_feature` |

约定:游戏侧代码永远不碰 `GDrawCommandList` 的创建接口(它只能在持有上下文的渲染线程调用);渲染侧代码永远不走 request 通道(自己就在渲染线程上,直接建)。

关键性质:

- **同环消化**:⑥ 的 `tryPop` 循环处理 request 时调用方已在渲染线程,数据写入当帧完成。
- **先于消费**:`GpuScene::initialize` 的 `ensureQuadBuffers` 在首帧 ⑥ 前入队,早于 ⑩ 的绘制消费。
- Proxy 的 `isGpuReady()`(atomic,acquire/release)负责 request 模式下的跨线程就绪可见性。

配套防线:

- **执行时守卫**:延迟命令 `execute()` 检查 `isGpuReady()`,未就绪跳过并告警。
- **缓存重试**:`FramebufferCache`/`BindingSetCache` 命中未就绪条目时重建,下帧自然修复。
- **渲染跳过**:`SetGraphicsStateCommand` 执行时跳过未就绪的 framebuffer/binding set(该帧不画,不崩)。
- `GfxRenderScope` 保留为调试断言用途,不再作为行为分支依据。

## 7. 线程间通信通道汇总

| 通道 | 生产者 → 消费者 | 实现 |
|---|---|---|
| 渲染场景命令 | 游戏线程 → RenderThread | `RenderSystem::m_game_command_queue`(`MpmcQueue<RenderCommand, 256>`),`RenderCommandQueue::AddSprite/...` 入队,renderFrame 开头 `tryPop` 批量回放 |
| 资源创建请求 | 游戏线程 → 渲染线程 | 同一命令队列(`RenderCommandType::CreateTexture/CreateBuffer`),`RenderCommandQueue::CreateTexture/CreateBuffer` 入队,⑥ 回放时 `initializeGpu` 直接创建 |
| 延迟数据写入 | 游戏线程/主线程 → 渲染线程 | `GDrawCommandList` 命令流 + `recordCommand` 锁,`detachRecordedCommands()` 移交 |
| pass 并行 | RenderGraph → ThreadPool | `pool.enqueue` + `WaitGroup`(按层级屏障) |
| 相机数据 | CameraSystem → 渲染侧 | `GetCameraRegistry()` 全局通道(不经命令队列) |
| 帧完成 | 渲染线程 → 主线程 | RenderThread condition_variable(`submitAndWait`) |
| GPU 完成 | 驱动 → CPU | `completion_query`(event query),帧槽复用前等待 |

## 8. 已知约束与注意点

1. **GL 后端必须串行提交**:direct_mode 渲染图(pass 内联)+ 上下文常驻渲染线程,游戏侧代码不允许直接调用设备(资源创建走 `GDrawCommandList` 延迟路径)。
2. D3D12/Vulkan 下 pass 线程并发创建资源依赖设备内部线程安全(cutie d3d12 使用锁;D3D12 API 本身允许)。
3. 调试线程问题优先尝试:`World::SetForceSequential(true)`(游戏侧串行)与 Debug 构建下开启 `render_settings.enable_single_thread`,隔离变量。
