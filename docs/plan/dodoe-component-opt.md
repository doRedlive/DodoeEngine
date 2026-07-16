性能优化方案：C++ System 自动并行 + C# ECS 存储优化 + C# System 并行

三部分：
- A｜C# ECS 高性能存储：class 组件存储优化（分页 sparse array + 零分配 struct 枚举器）+ 可选 struct 数据组件走 SoA。
- B｜C++ System 自动并行：每个 system 声明组件读/写集，调度器据此推依赖建 DAG，跑在现有 TaskScheduler 上（Task Graph）。
- C｜C# System 并行：CakeSystem 虚方法替代反射调用 + 访问声明 + DAG 分层并行，不引入完整 Job System。

共享基座：TaskGraph（节点+依赖，跑在 TaskScheduler 上）、CommandBuffer（结构变更延迟到同步点）。

---
可复用现有资产
- RenderGraph（render_graph.h/cpp）：Kahn 拓扑排序 + 分层并行执行算法可直接复用；但 Pass/Resource/Context 类型与 GPU 强耦合，CPU TaskGraph 建议写轻量独立实现而非泛化 RenderGraph。
- DrawCommandList（draw_command_list.h/cpp）：侵入式链表 + CRTP Command<T> + 帧分配器 + immediate/deferred 双模式——完美匹配 CommandBuffer 需求。建议抽取 `CommandList<TExecutor>` 泛化基类，DrawCommandList 和 EcsCommandBuffer 共用。
- TaskScheduler / ThreadPool：两者是几乎相同的线程池实现（mutex + queue + threads），建议合并为一个；TaskScheduler 作为 TaskGraph 的 backend executor，本身无依赖调度能力。
- NativeBindings 函数指针表：已用于 component field 访问，可扩展用于暴露 C++ parallelFor/view_iterate 给 C#（备选）。

---
硬约束
1. entt 并发读安全；并发结构变更（create/destroy/emplace/remove）不安全。→ 结构变更走 CommandBuffer，同步点单线程 apply。
2. CakeBehaviour.Update 里调 engine API（Transform、Rigidbody 等）非线程安全。→ CakeBehaviour 保持主线程顺序执行，不并行。
3. class 组件（CakeComponent 子类）无法达到 struct-SoA 的 cache 性能——引用类型的 List<T> 是指针数组、内存离散。高性能路径靠可选 struct 数据组件。
4. 单一 worker 池：C++ system graph 与 C# system graph 各跑各的后端（TaskScheduler / .NET 线程池），渲染线程独立。

现状锚点：
- C++ system 顺序跑：World::onRuntimeUpdate 循环 m_runtime_systems（world.cpp:304）
- C# system 反射调用：ScriptHub.InvokeLifecycle 用 GetMethod("Update").Invoke() 每帧反射（ScriptHub.Lifecycle.cs:9）
- C# 存储：ComponentSet<T> = Dictionary<ulong,int> sparse + List<T> dense（ComponentSet.cs），Query 用 yield（每帧分配）
- C# CakeSystem 是裸类，无虚方法、无访问声明

---
共享基座（先做）
F1. TaskGraph（依赖调度层，跑在 TaskScheduler 上）
- 复用 RenderGraph 的 Kahn 拓扑排序算法思想，写独立轻量实现（CPU TaskGraph 不需要 GPU 资源管理/别名/剔除）
- NodeId addNode(name, work) + addEdge(before, after) + run(sched)
- 图在 system 集合变化时重建（罕见），每帧复用（重置计数器）

F2. CommandBuffer（结构变更延迟）
- 抽取 DrawCommandList 的泛化基座：侵入式链表 + CRTP Command<T> + 帧分配 + immediate/deferred 双模式
- ECS 版本：createEntity/destroyEntity/emplace/remove，apply(Registry&) 单线程执行
- 与 DrawCommandList 共享基础设施，各自定义具体命令类型

---
A. C# ECS 高性能存储
A1. 稀疏集重构：uuid→本地稠密索引 + 分页 sparse array
- 集中实体表（World 持有），热循环走 int[] sparse array，无 hash
- uuid 只在与 native 交互边界出现

A2. 零分配查询：struct 枚举器取代 yield
- ComponentSet<T> 内部改用 T[] _dense（连续，扩容用数组不用 List）
- Query 返回 ref struct 枚举器，无堆分配、无 GC

A3. 可选 struct 数据组件（SoA，高性能路径）
- IComponentData 标记接口 + StructComponentSet<T>（T[] 连续内存）
- 暴露 Span<T> AsSpan() → 用户自己 Parallel.For，无需引擎提供 Job System

---
B. C++ System 自动并行（Task Graph）
B1. SystemAccess 声明（reads/writes/structural）
B2. 冲突规则 → DAG 构建 → 分层并行
B3. World::onRuntimeUpdate 从 for 循环 → 跑 TaskGraph + apply CommandBuffer
B4. 系统内数据并行：parallelFor 分块
B5. 确定性：注册顺序 tie-break + 顺序 fallback 开关

---
C. C# System 并行（CakeSystem 重构 + TaskGraph）

C1. CakeSystem 接口重构（杀反射）
- 改为 abstract class，OnCreate()/OnUpdate()/OnDestroy() 虚方法
- ScriptHub 用直接虚调用替代 GetMethod().Invoke() 反射

C2. 访问声明 + DAG 构建
- CakeSystem.GetAccess() → SystemAccess { Reads, Writes, Structural }
- 冲突规则同 C++ B，Kahn 排序产出分层列表
- 图在 system 注册/热重载时重建，每帧复用

C3. 分层并行执行
- 同层 system 用 Parallel.ForEach 并行（.NET 线程池后端）
- 单 system 直接跑，避免线程开销
- 每层结束 apply C# CommandBuffer

C4. 系统内数据并行
- A2 改造后 ComponentSet<T> 是 T[]，暴露 Span<T>
- 用户自己 Parallel.For，引擎不提供 IJob/IJobParallelFor 抽象层

C5. CakeBehaviour 保持不变
- GameObjectManager.ProcessUpdates 继续主线程顺序 foreach
- 优化点：A1/A2 改进存储效率（数组迭代替代 List<CakeBehaviour>）

---
为什么不做完整 C# Job System
- C# 侧组件分两条路径：Native 组件在 entt（C++ System 已并行迭代），Managed 组件在 ManagedComponentStore
- Managed 组件主要是 CakeComponent 子类（class 引用类型），内存离散，不适合并行
- struct 数据组件（A3）覆盖面取决于用户迁移意愿
- 用户拿到 Span<T> 后自己 Parallel.For 就够了，不需要引擎提供 JobHandle/Schedule 抽象
- 暴露 C++ parallelFor 给 C# 也无意义——C# 的 managed 组件不在 entt 里，C++ 迭代不到

---
实施顺序

Phase 1：F1+F2（TaskGraph + CommandBuffer，复用 DrawCommandList 泛化基座）
         ↓
     ┌───┴───┐
     ▼       ▼
Phase 2A   Phase 2B
C++ B     C# C1 — CakeSystem 虚方法（杀反射）
           C# A1 — ComponentSet 改数组+分页 sparse
           C# A2 — 零分配 struct 枚举器
           C# C2 — C# TaskGraph + 访问声明 + DAG
           C# C3 — C# CommandBuffer
           C# A3 — struct 组件 SoA + Span
         ↓       ↓
     └───┬───┘
         ▼
Phase 3：集成验证 + 性能对比

Phase 4（可选二期）：A4 Archetype/Chunk

2A 和 2B 完全解耦，可两人并行推进。
C++ 和 C# 各一个 TaskGraph 实例，C++ 跑在 TaskScheduler 上，C# 跑在 .NET 线程池上。

---
风险与权衡
- 结构变更安全：并行前必须先落 CommandBuffer
- 确定性：注册顺序 tie-break；access 声明遗漏需 review/断言
- class 组件性能天花板：引用数组无法 SoA；性能关键数据用 struct 组件（A3）
- 小场景并行开销可能 > 收益：给 graph 设「实体数阈值」，低于阈值退顺序执行
- CakeBehaviour 永远主线程：用户在里面调 engine API，天然不能并行

验证 / 度量
1. 性能：instrumentor 采集每 system 耗时、关键路径、帧时间；before/after 对比
2. 零分配：C# 端查询路径 GC 分配 == 0（GC.GetAllocatedBytesForCurrentThread 断言）
3. 正确性/确定性：并行结果 == 顺序结果（逐帧状态哈希对比）
4. 压测：万级实体场景下并行加速比与线程伸缩性
5. CakeSystem 虚调用 vs 反射调用性能对比
