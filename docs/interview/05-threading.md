# 05 多线程架构

对应简历条目：多线程架构。含面试高频题"OpenGL 单线程，你项目双线程，怎么处理的？双线程如何同步游戏和渲染线程？"

## 一句话

> 游戏逻辑多线程（ECS 系统按读写依赖分层并行），GPU 提交收敛到单线程；主/渲染双线程 + 帧内 Pass 并行录制，资源/场景双命令通道加每帧预算控制负载，Tracy 采集性能。

## 线程模型总览

```text
主线程     SystemContext::tickOneFrame → updateTick(游戏逻辑) → renderTick(提交帧)
TaskScheduler worker 池      World 系统（SystemGraph 按读写依赖分层并行）
RenderThread（双线程默认）   renderFrame：场景消费 → RenderGraph → 录制 → 提交 + present
ThreadPool（每个 Renderer 自有）  RenderGraph 并行执行 pass
```

### 游戏侧并行：TaskScheduler + TaskGraph

- 每个系统用 `getAccess()` 声明 `readsComponents/writesComponents`（entt type_hash）；
- `BuildGraphForSystems`（world.cpp:48）按组件读写依赖建边（写→读、写→写），拓扑分层；
- 单节点层当前线程直接执行；多节点层每系统 `TaskScheduler::submit`，主线程自旋 yield 等整层完成（层级屏障）；
- 逃生门：`World::SetForceSequential(true)` 强制串行，调试线程问题用。

### 渲染侧并行：两个粒度

1. **RenderGraph 每 level 并行**：`render_graph.cpp:550-591`，每 Pass 一个线程 + 独立 `DrawCommandList`，`WaitGroup`（wait_group.h:12）等整层，完成后按序 `append` 合并；
2. **网格命令并行构建**：primitive 索引按 64 分块 `parallelFor` + 线程本地命令存储 + `mergeThreadLocal`（见 03）。

## 双线程同步机制（面试重点，按"通信 → 节奏 → GPU"三层讲）

### 1. 通信层：双命令通道 + 无锁队列

- 每帧游戏侧把命令录进 `RenderFramePacket` 的**两条命令流**：`resource_commands`（资源创建）+ `scene_commands`（场景增删改），`render_system.h:49-57`；
- 录制期 `m_record_mutex` 双缓冲：渲染线程在消费上一帧 packet 的同时，游戏线程在填新 packet，互斥只在 swap 瞬间；
- `submitFrame`（render_system.cpp:123）swap 出 packet，push 进 `SpscQueue`（容量 4，单生产者单消费者无锁）；渲染线程 `renderFrame` 里 `tryPop` 回放；
- 资源创建走同一通道：游戏线程只建 Proxy + 数据内联进命令，渲染线程回放时 `initializeGpu` 实体化；`isGpuReady()` 原子标志保证跨线程就绪可见，配套执行时守卫 + 缓存重建兜底"晚一帧就绪"。

### 2. 节奏层：有界流水线（不是死锁步）

- `RenderThread`（render_thread.h:16）`kMaxFramesInFlight = 2`：`submitFrame` 在 condition_variable 上等 `in_flight < 2`，没满立即返回继续游戏逻辑；渲染线程完成一帧递减并 notify；
- 语义：**游戏逻辑最多领先渲染 2 帧**，既不死锁步也不无限堆积；
- 单线程开关 `render_settings.enable_single_thread`，两条路径语义一致，调试线程问题先切单线程隔离变量。

### 3. GPU 层：帧槽 + 延迟删除

- `RenderFrameScheduler` 维护 3 个 `FrameSlot`（`kMaxFramesInFlight = 3`），槽复用前 `waitEventQuery` 等 GPU 真正完成，再处理延迟删除队列——防止资源在 GPU 仍在使用时被销毁；
- 槽内成员：帧 staging 分配器（64MB ring）、瞬态资源池、completion query、本帧命令流。

### 4. 帧内存：epoch 惰性重置

- 每线程 `ThreadAllocator{ frame(64KB), scratch(16KB) }` 线性分配器（thread_allocator.h:12-18）；
- 渲染线程每帧 `Memory::ResetFrame()`（memory.cpp:302）遍历所有已注册分配器强制 reset 并推进全局 `s_frame_epoch`；
- 各 worker 循环顶部**惰性补重置**：比较 epoch，错过的线程自己追平（render_thread.cpp:62-67、thread_pool.h:29-34）。

## "双命令队列 + 任务预算"的准确表述（简历这句话必被追问）

| 简历用词 | 实际机制 | 准确说法 |
|---|---|---|
| 双命令队列 | `RenderFramePacket::resource_commands` + `scene_commands` 两条命令流 | "资源/场景双命令通道，双缓冲录制 + 无锁 SPSC 队列交接" |
| 控制任务预算 | `max_primitive_upserts_per_frame{16}` / `max_sprite_upserts_per_frame{64}`（render_scene.h:23-24），`processPendingPrimitiveUpdates` 每帧扣减，超出回滚到下帧（render_scene.cpp:587） | "每帧同步预算，平滑大场景 GPU 结构变更峰值" |
| 平衡负载 | 上述预算 + 2/3 帧 in-flight 上限 + 分层并行 | 组合拳 |

**注意**：graphics/compute/copy 三条 GPU 队列在 RHI 层存在，`AsyncCompute` pass 标志也有，但**当前没有 Pass 使用异步计算**。有 UE/商业引擎背景的面试官在场时，别把"双命令队列"说成"图形+计算队列并行"。

## OpenGL 与双线程的关系（详见 06）

- 不是"OpenGL 强制引擎单线程"，而是 **GL 下 GPU 提交线程必须持有上下文**；
- 上下文所有权模型：`OpenGLBackend::acquireContext/releaseContext`（opengl_backend.cpp:89-107，mutex + thread::id），渲染线程首帧 acquire 后**常驻**，主线程零设备调用；
- GL 下 RenderGraph 走 direct_mode 串行录制；并行度来自"游戏逻辑与渲染提交重叠"。

## Tracy 性能分析（简历提到，要能说清怎么用）

- 封装：`core/debug/instrumentor.h`，`DO_PROFILE_SCOPE / DO_PROFILE_SCOPE_CATEGORY / DO_PROFILE_FUNCTION / DO_PROFILE_FRAME / DO_PROFILE_THREAD_NAME`；Debug 构建接 `Tracy::TracyClient`（CMake `$<$<CONFIG:Debug>:DODOE_TRACY_ENABLED=1>`），Release 退化为自研 Chrome trace JSON 或空实现；
- 关键埋点：renderFrame 全流程（render_system.cpp 二十余处）、RenderGraph 每 Pass zone（"render-pass" 分类）、RenderFrameScheduler、RenderThread/TaskWorker 线程名、内存分配跟踪（TracyAllocS/TracyFreeS，persistent tier）；
- 典型用法：主线程与渲染线程区间对比找 CPU 瓶颈 → 帧图面板看 Pass 裁剪与屏障数 → RenderDoc/Nsight 看 GPU。

## 高频追问

- **"为什么 2 帧 in-flight 而不是 3？"** → RenderThread 层 2 帧是游戏逻辑领先上限（控制输入延迟），RenderFrameScheduler 的 3 槽是 GPU 资源生命周期（对齐三缓冲），两者粒度不同。
- **"双缓冲 packet 解决什么？"** → 游戏线程录制下一帧命令与渲染线程消费当前帧不互斥，只在 swap 加锁；SPSC 队列免锁。
- **"预算为什么是 16/64？"** → 经验值：单帧结构变更超过预算说明场景在爆发式变化，宁可滚动到下帧也要保帧时间稳定；可配置。
- **"怎么定位是 CPU 还是 GPU 瓶颈？"** → Tracy 看 CPU 区间 + event query 帧完成时间对比；再细分到 Pass zone。
- **"游戏线程怎么知道渲染完？"** → 它不主动等；只在 in-flight 满 2 帧时才被 cv 阻塞，其余时间通过命令队列单向推进。
