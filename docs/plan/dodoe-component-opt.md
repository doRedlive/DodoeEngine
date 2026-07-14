性能优化方案：C# ECS 存储 + 系统并行（C++ Task Graph / C# Job System）
三部分：
- A｜C# ECS 高性能存储：class 组件存储优化（分页 sparse array + 零分配 struct 枚举器）+ 可选 struct 数据组件走 SoA。
- B｜C++ System 自动并行：每个 system 声明组件读/写集，调度器据此推依赖建 DAG，跑在现有 TaskScheduler 上（Task Graph）。
- C｜C# 侧并行：Unity 式——MonoBehaviour.Update 留主线程，另提供显式 Job System（IJob/IJobParallelFor）做 opt-in 并行。
共享基座：TaskGraph（节点+依赖，跑在 TaskScheduler 上）、CommandBuffer（结构变更延迟到同步点）。本文档与「组件系统重构」「热重载」解耦，但 A 的 struct-SoA 组件正是 C 并行 Job 的安全载荷（见 §C）。

---
硬约束（决定所有设计）
1. entt 并发读安全；并发结构变更（create/destroy/emplace/remove）不安全。→ 结构变更必须走 CommandBuffer，在单线程同步点统一 apply。
2. 跨线程调 InternalCall 打 entt/物理非线程安全。→ C# 并行 Job 只能碰值数据 / struct 组件数组，不能在 worker 线程做任意 engine 调用。
3. class 组件（MonoBehaviour/用户 class）无法达到 struct-SoA 的 cache 性能——引用类型的 List<T> 是指针数组、内存离散。故高性能路径靠可选的 struct 数据组件，与 class 组件并存。
4. 单一 worker 池：C++ system graph 与 C# Job 共享同一 TaskScheduler，避免线程超订（渲染已有独立 render/draw 线程，render_thread.cpp）。
现状锚点：
- C++ system 顺序跑：World::onRuntimeUpdate 循环 m_runtime_systems（world.cpp:304），System::update(Registry&,dt)（system.h）。
- 线程池：TaskScheduler（parallelFor/async/submit/waitAll，task_scheduler.h），无依赖调度。
- 渲染已有 DAG：render_graph（render_graph.h）——Task Graph 是其 CPU-task 类比。
- C# 存储：ComponentSet<T> = Dictionary<ulong,int> sparse + List<T> dense（ComponentSet.cs），Query 用 yield（每帧分配）。

---
共享基座（先做）
F1. TaskGraph（依赖调度层，跑在 TaskScheduler 上）
现有 TaskScheduler 只有 fire-and-forget + waitAll，缺按依赖调度。新增薄封装：
class TaskGraph {
public:
    using NodeId = uint32_t;
    NodeId addNode(std::string name, std::function<void()> work);
    void   addEdge(NodeId before, NodeId after);   // before 完成后 after 方可运行
    void   run(TaskScheduler& sched);              // 计数器式派发，阻塞至全部完成
private:
    struct Node { std::function<void()> work; std::vector<NodeId> successors;
                  std::atomic<int> remaining_deps; int initial_deps; };
    // run(): 入度=0 的节点 submit；节点完成回调里对后继 remaining_deps-- ，减到 0 再 submit；
    //        用 latch/atomic 计数等待全部完成。
};
- 图结构只在 system 集合变化时重建（罕见），每帧复用（重置 remaining_deps = initial_deps）。
- 可与 render_graph 概念统一，但本轮只服务 CPU system 调度。
F2. CommandBuffer（结构变更延迟）
并行区内禁止直接 create/destroy/emplace/remove；改为录入命令，同步点单线程 apply：
class CommandBuffer {                       // 每 worker 一个，避免竞争，apply 时合并
    void createEntity(...); void destroyEntity(Entity);
    template<class C,class...A> void emplace(Entity, A&&...);
    template<class C> void remove(Entity);
    void apply(Registry&);                  // 主线程、单线程执行
};
- system 拿到的接口区分「就地读写已有组件数据」（并行安全）与「结构变更」（走 CommandBuffer）。
- apply 时机：每个 stage 结束 / 每帧结束（见 §B）。

---
A. C# ECS 高性能存储
A1. 稀疏集重构：uuid→本地稠密索引 + 分页 sparse array（对齐 entt）
问题：managed 实体 ID 是 64-bit uuid（稀疏），故现在 sparse 用 Dictionary<ulong,int> 每次 hash。
方案：引入本地稠密索引（回收复用），uuid↔localIndex 只在实体创建/跨界时映射一次（集中一张表），所有 ComponentSet 改用 localIndex 键：
// 集中式实体表（World 持有）
int[]  _sparse;              // localIndex -> denseSlot（分页数组，可扩容）
ulong[] _uuidByLocal;        // localIndex -> uuid
Dictionary<ulong,int> _localByUuid;   // 仅创建/跨界时用，不进热循环
Stack<int> _freeLocals;      // 回收
热循环（迭代/按实体随机访问）走 int[] sparse array，无 hash、cache 友好。uuid 只在与 native 交互边界出现。
A2. 零分配查询：struct 枚举器取代 yield
ComponentSet<T> 内部：int[] _sparse（localIndex→dense）、int[] _denseEntities（dense→localIndex）、T[] _dense（连续，扩容用数组不是 List）。查询返回 ref struct 枚举器，无堆分配、无 GC：
public ref struct Query<T> {                 // foreach 可用，零分配
    public Enumerator GetEnumerator();
    public ref struct Enumerator {
        public bool MoveNext();
        public ref T Current { get; }         // struct 组件返回 ref；class 组件返回引用
    }
}
// 多组件：交集迭代最小集，struct 枚举器 yield (localIndex, ref T1, ref T2...)
public Query2<T1,T2> Query<T1,T2>();          // 无 params Type[]、无 List/HashSet 分配
- 现 World.Query/QueryIds（World.cs:158）的 yield/List/HashSet 全部替换。
- class 组件：dense 仍是引用数组（无法避免），但迭代器零分配、按最小集交集，仍显著优于现状。
A3. 可选 struct 数据组件（SoA，高性能路径）
为性能关键的 gameplay 数据提供值类型组件：
public interface IComponentData { }          // struct 标记

// SoA 存储：T[] 连续内存，支持 Span/ref 访问与分块
public sealed class StructComponentSet<T> where T : struct, IComponentData {
    T[] _dense; int[] _sparse; int[] _denseEntities; int _count;
    public ref T Get(int local);
    public Span<T> AsSpan();                  // 连续内存 → SIMD/批处理/Job 载荷
}
- 与 class 组件并存：entity.GetComponent<PlayerData>()（struct）返回 ref，GetComponent<PlayerController>()（MonoBehaviour）返回引用。
- 这些 SoA 数组正是 §C 并行 Job 的安全载荷（IJobParallelFor 按 index 写不同槽位）。
- 与「组件系统重构」的 ManagedComponentStore 融合：store 内部对 class 用 A2 存储、对 struct 用 A3 存储，统一在 GetComponent<T> 之下。
A4.（可选，二期）Archetype/Chunk
按组件签名分组实体到定长 chunk，多组件查询顺序遍历 chunk → 最佳 cache 局部性 + SIMD。改动大，标记为二期；A1–A3 已能拿到大部分收益。

---
B. C++ System 自动并行（Task Graph）
B1. 访问声明
给 System 加读/写集声明（用 entt 类型 id）：
struct SystemAccess {
    std::vector<entt::id_type> reads, writes;
    bool structural = false;                 // 需 create/destroy/add/remove
};
class System {
    virtual SystemAccess access() const = 0; // 各 system 实现
    // 声明辅助：AccessBuilder().reads<Rigidbody2dComponent>().writes<TransformComponent>()
};
例：Physics2dSystem → reads Rigidbody2dComponent,BoxCollider2dComponent；writes TransformComponent；start 阶段 structural（建 body 不改 entt 结构，纯读组件+外部 box2d 状态，可细分 start/update 的 access）。
B2. 冲突规则 → 建 DAG
两 system 冲突则连边（按注册顺序定方向，保证确定性）：
- write(A) ∩ (read(B) ∪ write(B)) ≠ ∅ → A、B 有序（先注册者在前）。
- structural(A) → 与所有并发节点冲突：要么隔离单跑，要么其结构变更走 CommandBuffer 延迟、update 本身仅按 read/write 集参与并行。推荐后者：绝大多数 system 的结构变更延迟后即可并行。
- 无冲突 → 无边 → 可并行。
从 m_runtime_systems/m_simulation_systems 构建一次 TaskGraph，缓存；系统集变化才重建。
B3. 执行
World::onRuntimeUpdate 从「顺序 for 循环」改为「跑缓存的 TaskGraph」：
void World::onRuntimeUpdate(Registry& reg, float dt) {
    m_runtime_graph.reset();                        // 重置计数器
    // 每个节点 work = [sys,&reg,dt]{ sys->update(reg,dt); }
    m_runtime_graph.run(TaskScheduler::Self());     // 依赖并行派发，阻塞至完成
    m_command_buffers.applyAll(reg);                // 同步点：单线程 apply 结构变更
}
- 保留顺序执行开关（m_state/调试标志），便于回归对比与排错。
B4. 系统内数据并行（intra-system）
单个 system 内对 view 用 TaskScheduler::parallelFor 分块并行（仅写自身 writes 集、不同 entity 槽位）：
auto view = reg.view<TransformComponent, Rigidbody2dComponent>();
TaskScheduler::Self().parallelFor(0, view.size_hint(), [&](size_t b, size_t e){ /* 处理 [b,e) */ });
- 注意 entt 多组件 view 迭代最小池；分块需按 view 的稠密顺序切。
- 防伪共享：分块粒度 ≥ cache line。
B5. 确定性 & 调试
- 注册顺序作冲突定向的 tie-break → 结果可复现。
- 用现有 instrumentor.h（instrumentor.h）标注每节点耗时、图关键路径，量化 before/after。

---
C. C# 侧并行（Unity 式 Job System）
C1. MonoBehaviour.Update 留主线程
Unity 同款：MonoBehaviour.Update 在主线程顺序跑（用户 Update 里常调 Transform.Position 等 engine InternalCall，非线程安全）。优化点仅在于扁平数组迭代 active 列表（来自 store，减少每调用开销），不并行。
C2. 显式 Job System
public interface IJob { void Execute(); }
public interface IJobParallelFor { void Execute(int index); }

public static class JobScheduler {
    public static JobHandle Schedule(this IJob job, JobHandle dependsOn = default);
    public static JobHandle Schedule(this IJobParallelFor job, int length, int batch, JobHandle dependsOn = default);
}
public struct JobHandle { public void Complete(); /* 依赖组合 */ }
- 载荷限定：Job 只能碰 §A3 的 struct 组件数组（Span<T>/NativeArray<T>-like）或值拷贝数据；禁止 worker 线程做结构性 engine 调用。按 index 写不同槽位 → 无竞争。
- 依赖：Schedule(dependsOn) 串依赖；主线程 Complete() 等待并做写回同步点。
C3. 执行后端（两选一，建议分期）
- 一期：托管 TPL 后端（System.Threading.Tasks）。实现简单、无 mono attach/thunk 复杂度。风险：与 C++ 池线程超订 → 限制 Job 并行度（如 = 核数 − C++ 池占用）。
- 二期：共享 native TaskScheduler。managed Schedule → InternalCall 提交 native task，task 回调 managed Execute(index)（[UnmanagedCallersOnly] 函数指针 / mono thunk），按 batch 跨界（非按元素，摊薄开销）。worker 线程需 mono_thread_attach。收益：与 C++ system 共池、无超订、统一调度。
C4. 与 A/B 的衔接
- C 的并行数据 = A3 的 struct-SoA 组件（安全按 index 并行）。
- C 的 Job 与 B 的 system graph 共用 §F1 TaskScheduler（二期后端），帧内调度统一。

---
实施顺序（每步独立可验证）
1. F1 TaskGraph + F2 CommandBuffer（基座）。
2. B｜C++ system 自动并行：加 SystemAccess 声明 → 建 DAG → World::update 跑图 + 同步点 apply CommandBuffer；保留顺序 fallback 对比。
3. A1+A2｜C# 存储核心：uuid→localIndex + 分页 sparse array + 零分配 struct 枚举器，替换 ComponentSet/World.Query。
4. A3｜struct 数据组件 SoA：IComponentData + StructComponentSet<T>，融入 ManagedComponentStore。
5. C｜C# Job System：IJob/IJobParallelFor + JobScheduler（先 TPL 后端），载荷=A3 数组。
6.（可选二期）A4 archetype/chunk；C3 二期共享 native 调度后端。
B 与 A 相互独立，可并行推进；C 依赖 A3（安全载荷）。

---
风险与权衡
- 结构变更安全：并行前必须先落 CommandBuffer，否则 entt 结构竞争必崩。
- 确定性：注册顺序 tie-break；并行 system 不得依赖隐式执行序（未声明的读写即隐 bug）——access 声明遗漏是主要风险源，需 review/断言辅助。
- 线程超订：C++ 图 + C# Job 必须共池或严格配额；渲染线程独立不冲突。
- class 组件性能天花板：引用数组无法 SoA；性能关键数据请用 struct 组件（A3）。
- 伪共享：并行写相邻数组元素按 cache line 分块。
- mono thread attach（C3 二期）：native worker 回调 managed 前必须 attach，且注意 GC 安全点。
- 粒度收益：小场景并行开销可能 > 收益；给 system graph 设「实体数阈值」，低于阈值退顺序。
验证 / 度量
1. 性能：用 instrumentor 采集——每 system 耗时、图关键路径、帧时间；A/B/C 各自 before/after 对比。
2. 零分配：C# 端每帧 GC 分配计数 == 0（查询路径），用 GC.GetAllocatedBytesForCurrentThread 断言。
3. 正确性/确定性：并行结果 == 顺序结果（同种子、同输入的逐帧状态哈希对比）。
4. 压测：万级实体场景下，物理/动画/渲染准备等 system 的并行加速比与线程伸缩性。
5. Job：IJobParallelFor 在 struct-SoA 组件上的加速比与 batch 粒度扫描。