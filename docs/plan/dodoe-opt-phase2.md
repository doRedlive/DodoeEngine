引擎并行化优化 — 第二阶段指导文档

本文档覆盖第一阶段已完成基础设施之后的全部剩余工作，
包括：DrawCommandList 重构、C# ECS 存储优化、C# System 并行化。

---
一、当前基线

已完成资产（第一阶段产物）：

  core/async/task_graph.h / .cpp
    Kahn 拓扑排序 + 分层并行执行，跑在 TaskScheduler 上。
    API: addNode(name, work) → addEdge(before, after) → compile() → execute(scheduler)
    暴露 getLevels() 供调用方自行决定每层的执行策略。

  core/async/task_scheduler.h / .cpp
    从 core/thread/ 迁移至 core/async/。mutex + queue + threads 线程池。
    提供 submit(fn)、parallelFor(begin, end, fn)、waitAll()。

  core/container/command_list.h
    泛化 CommandList<TExecutor> 模板类（header-only）。
    提供：侵入式链表 Command + CRTP CommandImpl<T> + 帧分配 + immediate/deferred 双模式。
    核心 API: enqueue<T>(args...), append(&&), execute(executor), reset()。
    受保护接口: appendCommand, allocate, moveFrom, immediateTarget。

  core/container/containers.h — 已存在，项目通用容器别名（String, DynamicArray, UnorderedMap 等）。

  function/world/world_commands.h
    WorldCommands : CommandList<Registry>。
    ECS 结构变更延迟命令：destroyEntity(Entity)、emplaceComponent<T>(entity, args...)、removeComponent<T>(entity)。
    同步点 apply(Registry&) → execute(registry)。

  function/world/systems/system.h
    SystemAccess { reads, writes, structural } + Builder（readsComponents<Ts...>(), writesComponents<Ts...>()）。
    System 基类新增 virtual getAccess() const → 返回空（默认无冲突）。

  function/world/world.h / .cpp
    World 持有 m_runtime_task_graph、m_simulation_task_graph、m_command_buffer(WorldCommands)。
    BuildGraphForSystems → entt::type_hash 为 key，追踪每类组件的 producer/readers，构建 DAG 边。
    ExecuteSystemsParallel → 分层并行：单系统直接调，多系统 TaskScheduler::submit + yield 等待。
    World::SetForceSequential(bool) — 关闭并行回退到原来顺序 for 循环，用于调试。

  scriptcore/Source/World/System.cs
    CakeSystem { virtual OnCreate(), OnUpdate(), OnDestroy() }。

  scriptcore/Source/World/SystemDispatcher.cs（新建）
    缓存 CakeSystem[] 数组，直接虚调用，消除反射。

  scriptcore/Source/ScriptHub/ScriptHub.Lifecycle.cs
    InvokeSystemOnCreate/Update/Destroy → 委托到 SystemDispatcher。

已验证的设计约束：
  - entt 并发读安全，并发结构变更不安全 → 走 WorldCommands
  - CakeBehaviour 保持主线程顺序
  - C++ 和 C# 各有独立的 TaskGraph 实例，各自的后端线程池

---
二、Phase R1 — DrawCommandList 复用 CommandList<GfxCommandList>

2.1 目标

消除 DrawCommandList 中与 CommandList<TExecutor> 重复的底层代码。
DrawCommandList 继承 CommandList<GfxCommandList>，只保留 GPU 专属逻辑。

2.2 当前 DrawCommandList 结构分析

  文件：engine/src/runtime/function/graphics/draw_command_list.h / .cpp

  DrawCommandList 现有成员可拆为三层：

  第一层 — 通用链表基础设施（与 CommandList<GfxCommandList> 完全同构）：
    struct DrawCommand { m_next, m_size, m_execute(GfxCommandList&), m_destroy }
    struct Command<T> : DrawCommand { CRTP ExecuteCommand/DestroyCommand }
    m_head, m_tail, m_command_count
    void appendCommand(DrawCommand*)      → CommandList::appendCommand
    void* allocate(size, align)          → CommandList::allocate
    void moveFrom(DrawCommandList&&)     → CommandList::moveFrom
    void append(DrawCommandList&&)       → CommandList::append
    void execute(GfxCommandList&)        → CommandList::execute
    void reset()                         → CommandList::reset（还需调 AdvanceFrameEpoch）

  第二层 — immediate/deferred 双模式：
    GfxCommandList* m_immediate_target
    beginImmediateFrame / endImmediateFrame / isImmediate
    → CommandList 已有完全同构的接口

  第三层 — GPU 专属（保留）：
    GfxDeviceHandle m_device
    setDevice, beginFrame, endFrame
    50+ 方法：beginMarker, endMarker, draw, drawIndexed, dispatch, clearTexture*, 
    writeBuffer, writeTexture, copyBuffer, setGraphicsState, setComputeState,
    createTexture, createBuffer, createShader, etc.
    内部 *Command 类型约 20 个

2.3 重构步骤

步骤 1 — 改继承关系
  修改 draw_command_list.h：
    class DrawCommandList : public CommandList<GfxCommandList> { ... };

  删除的成员（由基类接管）：
    - DrawCommand 结构体定义 → 用基类的 Command
    - Command<TDerived> CRTP → 用基类的 CommandImpl<TDerived>
    - m_head, m_tail, m_command_count 声明
    - appendCommand, allocate, moveFrom 实现
    - append, execute, reset 声明和实现
    - beginImmediateFrame, endImmediateFrame, isImmediate 声明和实现

步骤 2 — 迁移内部命令类型
  每个内部 struct 从继承 Command<T> 改为继承 CommandImpl<T>：

  迁移前（OpenCommand 为例）：
    struct OpenCommand final : Command<OpenCommand> {
        OpenCommand() = default;
        void execute(GfxCommandList& command_list) const;
    };

  迁移后：
    struct OpenCommand final : CommandImpl<OpenCommand> {
        OpenCommand() = default;
        void execute(GfxCommandList& command_list) const;
    };
    // ExecuteCommand / DestroyCommand 由 CommandImpl 自动生成，删除手写版本

  需要迁移的命令类型（约 20 个）：
    OpenCommand, CloseCommand, ClearStateCommand, EndMarkerCommand,
    ClearTextureFloatCommand, ClearTextureUIntCommand, ClearDepthStencilTextureCommand,
    CopyBufferCommand, SetTextureStateCommand, SetBufferStateCommand,
    CommitBarriersCommand, SetGraphicsStateByValueCommand, SetGraphicsStateCommand,
    SetComputeStateCommand, DrawPrimitiveCommand, DrawIndexedPrimitiveCommand,
    DispatchCommand, DrawIndirectCommand, DrawIndexedIndirectCommand,
    DispatchIndirectCommand,
    CreateTextureCommand, CreateFramebufferCommand, CreateBindingSetCommand,
    CreateGraphicsPipelineCommand, CreateDescriptorCommand

  保留手写 ExecuteCommand/DestroyCommand 的命令（变长数据，非标砖 CommandImpl 模式）：
    WriteBufferCommand, WriteTextureCommand, PushConstantsCommand,
    BeginMarkerCommand (变长 name), CreateBufferCommand, CreateShaderCommand
    → 这些仍继承基类 Command 手写 Execute/Destroy，但无需继承 CommandImpl

步骤 3 — 清理重复方法
  删除 DrawCommandList 中已有基类实现的方法：
    - append(DrawCommandList&&) → 基类已实现
    - execute(GfxCommandList&) → 基类已实现
    - isEmpty / commandCount → 基类已实现

  reset() 特殊处理：
    基类 reset() 不做 AdvanceFrameEpoch（由调用方决定）。
    DrawCommandList::reset() override：
      void reset() {
          CommandList<GfxCommandList>::reset();
          Memory::AdvanceFrameEpoch();
      }

步骤 4 — enqueue 适配
  基类 enqueue 签名为：
    template<typename TCommand, typename... TArgs>
    TCommand& enqueue(TArgs&&... args);

  所有 GPU API 方法内部的 enqueue<XxxCommand>(args...) 调用保持不变。
  因为 CommandImpl<T> 构造函数从基类 Command 继承，兼容现有参数。

2.4 验证方法
  - 编译通过（模板实例化在编译期验证类型正确性）
  - 运行现有渲染场景，视觉对比 before/after 无差异
  - GPU 帧捕获（RenderDoc）before/after 对比，所有 draw call 参数一致

2.5 风险
  低。纯机械替换，不改变任何 GPU 行为。变长命令（WriteBuffer 等）保留手写 Execute/Destroy，
  不受基类影响。immediate 模式的行为完全由基类接管，逻辑不变。

---
三、Phase 2B — C# ECS 存储优化 + System 并行

所有 C# 文件位于 engine/src/scriptcore/Source/ 下。

3.1 A1 — ComponentSet 稀疏集重构

  目标：消除 Dictionary<ulong,int> 的 hash 查找开销，热循环走 int[] 数组索引。

  现状：
    ComponentSet<T>（ComponentSet.cs）：
      Dictionary<ulong,int> _sparse  // uuid → dense index, O(1) 但有 hash 开销
      List<T> _dense                 // 实际组件数据，引用类型分散

  改动：
    a) _sparse 从 Dictionary<ulong,int> 改为分页 int[][]：
       页大小 = 4096（12 位偏移）。
       sparse[page_index][offset] = dense_index（-1 表示不存在）。
       查找：page = uuid >> 12, offset = uuid & 0xFFF，两次数组索引，无 hash。

    b) _dense 从 List<T> 改为 T[]，手动扩容（×2 策略），消除 List<T> 的内部计数开销。

    c) 实体表集中到 World：
       struct EntityRecord { int sparsePage; int sparseOffset; ulong uuid; }
       实体 Id → EntityRecord 的映射由 World 持有，ComponentSet 只关心 sparse→dense。

  文件：engine/src/scriptcore/Source/World/ComponentSet.cs

  设计细节：
    class ComponentSet<T> {
        int[][] _sparse;       // 分页稀疏数组，每页 4096
        T[] _dense;            // 连续组件数组
        int _count;            // 当前数量
        int _capacity;         // dense 容量

        bool Has(ulong uuid);
        ref T Get(ulong uuid);
        void Add(ulong uuid, T component);
        void Remove(ulong uuid);
    }

  注意：T 是 class（CakeComponent 子类），所以 ref T 返回的是引用（指针），
  不涉及 struct 复制。后续 A3 再处理 struct 组件。

3.2 A2 — 零分配 struct 枚举器

  目标：Query 不产生堆分配，foreach 走 ref struct 枚举器。

  现状：
    查询走 yield return → 编译器生成 IEnumerator<T> 类实例 → 每帧堆分配。

  改动：
    ComponentSet<T> 加 Enumerator 和 Query()：

    public ref struct Enumerator {
        T[] _dense;
        int _count;
        int _index;

        public Enumerator(T[] dense, int count) { ... }
        public bool MoveNext() { while (_index < _count) { _index++; if (_dense[_index-1] != null) return true; } return false; }
        public T Current => _dense[_index - 1];
        public Enumerator GetEnumerator() => this;  // foreach 要求
    }

    public Enumerator Query() => new Enumerator(_dense, _count);

  验证：在查询循环前后调 GC.GetAllocatedBytesForCurrentThread，断言增量为 0。

3.3 C2 — C# CakeSystem 访问声明 + DAG + 分层并行

  目标：CakeSystem 像 C++ System 一样声明读写集，引擎据此建 DAG，
  同层 system 用 Parallel.ForEach 并行。

  步骤 1 — 加 CakeSystemAccess 和 GetAccess()

  文件：engine/src/scriptcore/Source/World/System.cs

  类型设计：
    public readonly struct CakeSystemAccess {
        public Type[] Reads { get; init; }
        public Type[] Writes { get; init; }
        public bool Structural { get; init; }
    }

    public class CakeSystem {
        ...
        public virtual CakeSystemAccess GetAccess() => default;
    }

  使用示例（用户代码）：
    class MoveSystem : CakeSystem {
        public override CakeSystemAccess GetAccess() => new() {
            Reads  = [typeof(TransformComponent)],
            Writes = [typeof(VelocityComponent)],
        };
        public override void OnUpdate() { ... }
    }

  步骤 2 — 构建 CakeTaskGraph

  新建文件：engine/src/scriptcore/Source/World/CakeTaskGraph.cs

  public class CakeTaskGraph {
      // C# 侧的 Kahn 拓扑排序，输出分层列表
      public void Build(CakeSystem[] systems);
      public List<CakeSystem[]> Levels { get; }  // 每层是一组可并行的 system
  }

  构建算法（与 C++ BuildGraphForSystems 同构）：
    1. 收集所有 system 的 GetAccess()
    2. 遍历 system，以 Type 为 key（typeof(T)），追踪 producer 和 readers
    3. 读冲突：producer → 当前 system 加边
    4. 写冲突：producer → 当前，所有 readers → 当前，清空 readers，当前变 producer
    5. Kahn 排序产出 Levels

  注意：C# 侧用 Type（typeof(T)）作为组件类型标识，对应 C++ 的 entt::type_hash。

  步骤 3 — CakeSystemScheduler 执行层

  新建文件：engine/src/scriptcore/Source/World/CakeSystemScheduler.cs

  修改 SystemDispatcher.OnUpdate()：
    不以简单顺序 foreach 调用，改为：
    a) 从 ScriptHub.SystemTypeCache 收集所有 CakeSystem 实例
    b) 如果 system 列表变更（热重载），重建 CakeTaskGraph
    c) 遍历 Levels，每层：
       单 system → 直接调 OnUpdate()
       多 system → Parallel.ForEach(level, sys => sys.OnUpdate())
    d) 应用 CakeCommandBuffer 结构变更（如果有）

3.4 C3 — C# CommandBuffer

  新建文件：engine/src/scriptcore/Source/World/CakeCommandBuffer.cs

  public class CakeCommandBuffer {
      // 参照 C++ WorldCommands 模式，C# 侧的结构变更延迟

      public void DestroyEntity(ulong entityId);
      public void AddComponent<T>(ulong entityId, T component) where T : CakeComponent;
      public void RemoveComponent<T>(ulong entityId) where T : CakeComponent;

      // 同步点：单线程 apply
      public void Apply(World world);
  }

  实现要点：
    - 内部用 List<ICommand> 存储（C# 侧不需要帧分配器优化，GC 管理即可）
    - ICommand { void Execute(World world); }
    - 每个命令类型实现 ICommand
    - Apply 按入队顺序单线程执行

3.5 A3 — struct 数据组件 SoA（可选高性能路径）

  目标：允许 struct 组件走 T[] 连续存储，暴露 Span<T> 给用户自行 Parallel.For。

  新建文件：engine/src/scriptcore/Source/World/StructComponentSet.cs

  public interface IComponentData { }  // 标记接口，struct 组件需实现

  public class StructComponentSet<T> where T : struct, IComponentData {
      T[] _data;
      int _count;

      public Span<T> AsSpan() => _data.AsSpan(0, _count);
      public ref T this[int index] => ref _data[index];
      public int Count => _count;
      // ... Add, Remove, Has 等同 ComponentSet 模式
  }

  用户使用示例：
    struct HealthData : IComponentData { public float Value; }

    var healthSet = world.GetStructSet<HealthData>();
    var span = healthSet.AsSpan();
    Parallel.For(0, span.Length, i => {
        ref var h = ref span[i];
        if (h.Value <= 0) { /* ... */ }
    });

  注意：
    - struct 组件无 GC 分配负担，天然适合并行
    - 引擎不提供 IJob/IJobParallelFor/JobHandle 抽象层
    - 用户拿到 Span<T> 后自行决定是否并行、用什么并行 API

---
四、Phase 3 — 集成验证

4.1 性能度量
  - 用 instrumentor 采集每个 system（C++ 和 C#）的耗时
  - 记录关键路径长度（DAG 中最长的依赖链）
  - before/after 帧时间对比（顺序 vs 并行）

4.2 正确性
  - 确定性验证：同一场景同一输入，多次运行逐帧状态哈希一致
  - 并行 vs 顺序结果对比：toggle SetForceSequential 对比
  - C# 侧：虚调用 vs 反射调用行为一致

4.3 零分配验证
  - 每个查询路径前后 GC.GetAllocatedBytesForCurrentThread 断言 == 0
  - 压测场景运行 1000 帧，GC 触发次数为 0

4.4 压测
  - 万级实体场景（10000+ entities，每种 system 遍历全部）
  - 测量并行加速比（2/4/8 线程）
  - 小场景对比（100 实体）验证并行开销不反噬

4.5 DrawCommandList 重构验证
  - 运行现有渲染场景，RenderDoc 捕获 before/after 对比
  - 所有 draw call 参数、资源绑定、状态切换一致

---
五、Phase 4 — 可选后续

5.1 ThreadPool + TaskScheduler 合并
  core/thread/thread_pool.h 与 core/async/task_scheduler.h 几乎相同实现。
  保留 TaskScheduler，ThreadPool 的调用方迁移至 TaskScheduler，删除 thread_pool.h。

5.2 A4 Archetype/Chunk 存储
  在 A1 基础上进一步优化：按 (ComponentTypeA, ComponentTypeB, ...) 组合分组存储实体，
  同 archetype 的所有组件在连续内存中，迭代时 cache 友好。

5.3 NativeBindings 扩展
  函数指针表扩展 parallelFor/view_iterate 入口，供 C# 调 C++ 的并行迭代能力。
  注意：只对 C++ 原生组件有效，Managed 组件无法走此路径。

---
六、实施顺序

  Phase R1  DrawCommandList 重构            [独立，约 2-3h 机械迁移]
      ↓
  Phase 2B-A1  ComponentSet 稀疏集          [依赖：无]
      ↓
  Phase 2B-A2  零分配枚举器                 [依赖：A1]
      ↓
  Phase 2B-C2  CakeSystem DAG + 并行        [依赖：C1 已完成，SystemDispatcher]
      ↓
  Phase 2B-C3  C# CommandBuffer             [依赖：C2]
      ↓
  Phase 2B-A3  struct 组件 SoA              [依赖：A1/A2]
      ↓
  Phase 3      集成验证 + 性能对比          [依赖：全部完成]

  R1 和 2B 线可并行推进（不同语言，不同文件）。
  A3 为可选，不影响核心功能。
