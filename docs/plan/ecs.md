Dodoe 统一 C# 数据 ECS 查询架构
状态与结论
本文定义一套让 C# 独有的 unmanaged struct 数据组件、现有 C++ 原生组件 和 CakeBehaviour 共存的架构。
核心决策：
1. 对外只有一套数据查询入口：World.Query<T...>()。调用者不需要选择 QueryNative 或 QueryManaged。
2. 查询中的组件类型可以混用 C# 数据组件与 NativeComponent；C++ 统一完成实体交集计算。
3. 统一的是“查询/筛选接口”，不是承诺所有组件都能被任意 C# struct 零拷贝写入。原生组件按访问能力分为筛选、批量操作和显式 ABI 视图。
4. CakeBehaviour 保留为主线程、对象级逻辑；高密度批处理进入 Data ECS 系统。两者共享同一个实体。
5. C++ 仍是实体生命周期的真相源；C# 数据组件类型是 C# 的真相源。
这套方案不是把现有 ManagedComponentStore 改成 DOTS。它新增一条数据导向路径，以避免破坏 CakeBehaviour、编辑器和现有原生组件代理 API。

---
1. 问题与边界
当前项目已有两条组件路径：
C++ entt registry
  └─ NativeComponent 的实际数据

C# ManagedComponentStore
  └─ CakeComponent class / CakeBehaviour 实例
NativeComponent 是 C# 对 C++ 组件的单对象代理。它适合 GameObject、CakeBehaviour、编辑器和低频调用；但在大量实体的循环中，反复 GetComponent 与属性 getter/setter 会产生跨边界调用、字符串类型查找和代理对象分配。
新的数据路径只解决以下问题：
- C# 定义高性能、无托管引用的结构体组件；
- C# 系统按组件组合批量处理实体；
- 查询可以把 C# 数据组件和已有 C++ 原生组件放在同一条件中；
- 循环体不发生逐实体 FFI；
- 保留原生组件的 dirty 标记、物理句柄、资源句柄等语义。
非目标：
- 不把所有 CakeComponent class 自动纳入数据查询。它们是托管对象、可有引用，内存模型与 Data ECS 不兼容。
- 不让任意 C++ 原生组件自动变成 C# ref struct。只有经过 ABI 合约验证的白名单组件可直接映射。
- 第一阶段不追求 Unity DOTS archetype/chunk 存储；使用 EnTT sparse-set/runtime-view 完成正确且高效的基线实现。

---
2. 组件分类与职责
同一个 C++ 实体
│
├─ NativeComponent
│   C++ 静态类型、entt storage、C# 单对象代理
│   例：TransformComponent、Rigidbody2dComponent、SpriteRendererComponent
│
├─ ICakeDataComponent
│   C# 独有、unmanaged struct、native 字节存储
│   例：Velocity、Health、Steering、Cooldown
│
└─ CakeBehaviour / CakeComponent class
    C# 托管对象、生命周期、引用关系、对象级逻辑
    例：PlayerController、UIBinder、技能配置对象
建议的 C# 类型约束：
public interface IQueryComponent { }

public interface ICakeDataComponent : IQueryComponent { }

public abstract class NativeComponent : CakeComponent, IQueryComponent
{
}

public struct Velocity : ICakeDataComponent
{
    public Vector3f Value;
}
数据 API 进一步要求：
where T : unmanaged, ICakeDataComponent
CakeBehaviour 和普通 CakeComponent class 不实现 IQueryComponent。这是一条刻意的类型边界：它们可以访问 Data ECS，但不是高性能查询列。

---
3. 单一查询 API
3.1 调用形态
查询入口始终是一套 API，Native 与 C# 数据类型可混写：
using var query = World.Query<Velocity, TransformComponent>()
    .Without<Disabled>()
    .ReadOnly<Velocity>()
    .ReadWrite<TransformComponent>()
    .Build();
这里不存在 QueryNative、QueryData 或“先查 Native、再查 C#”两套接口。
ComponentDescriptor 决定每个类型属于哪个 storage、能否直接取得数据列，以及写入后需要何种提交动作。统一的是查询表达；访问能力由组件描述符决定。
3.2 访问能力
类型
查询中的用途
循环内访问方式
ICakeDataComponent
包含/排除/读写
原生内存上的 ref T 或批量列 view
NativeComponent / Filter
包含/排除
仅作为实体筛选条件
NativeComponent / Batch
包含/读写
调用一次 C++ 批量操作
NativeComponent / DirectView
包含/读写
经过 ABI 验证的受控 native view
所以同一个查询可以写成：
using var q = World.Query<Velocity, TransformComponent>().Build();

NativeTransform.BatchIntegrate(
    q.Entities,
    q.GetReadOnly<Velocity>(),
    Time.DeltaTime);
它同时筛选了 Velocity + TransformComponent，但 Transform 的实际写入仍在 C++ 一次批量完成。这确保 TransformComponent::setPosition 的 dirty 语义不被绕过。
3.3 为什么不能为所有 NativeComponent 返回 ref T
现有原生组件并不都是纯数据：
- TransformComponent 有 dirty 状态；
- Rigidbody2dComponent 有 Box2D 句柄和行为方法；
- SpriteRendererComponent 有资源指针、dirty 状态；
- 未来组件可能有缓存、所有权或线程约束。
若 C# 可以任意写原始字节，C++ 系统就无法知道副作用是否已经执行。因此：
- 默认 NativeComponent 是筛选 token；
- 高频写入用 C++ 批量 API；
- 只有 ABI 稳定且数据语义清晰的组件才能声明 DirectView。
这不是“两套查询接口”，而是同一查询得到不同能力的列句柄。

---
4. 类型注册与 storage 解析
4.1 稳定 TypeId
禁止用每次启动的递增 nextId。热重载、程序集遍历顺序和存档都会使它不稳定。
每个查询组件都有稳定标识：
TypeId = Hash64(logicalTypeName + schemaVersion)
例如：
Game.Combat.Velocity@1
Dodoe.TransformComponent@1
注册时必须校验 hash 冲突、布局版本、大小和对齐。
4.2 统一描述符
ComponentDescriptor
├─ TypeId
├─ 逻辑全名与 schema version
├─ Domain: Data 或 Native
├─ AccessMode: Filter / Batch / DirectView
├─ C# 数据布局：size、alignment、字段布局 hash（Data）
├─ ResolvePresenceStorage(Scene) → entt::sparse_set*
├─ ResolveDataStorage(Scene) → RuntimeDataStorage*（Data）
└─ NativeBatchAdapter / NativeViewAdapter（Native，可选）
注册来源：
- C# ICakeDataComponent：应用程序集加载后扫描并注册；
- 现有 NativeComponent：由 C++ ComponentDB 和现有绑定代码生成器注册；
- NativeComponent 类型名不再通过每帧字符串查找，而是在程序集加载时解析为稳定的 descriptor。
4.3 每个 Scene 独立 storage
组件“元数据”可全局共享，组件“数据”必须归属当前 Scene/Registry。
Global ComponentRegistry
  └─ descriptors

Scene A
  ├─ entt static storages
  └─ C# RuntimeDataStorages

Scene B
  ├─ entt static storages
  └─ C# RuntimeDataStorages
不能使用一个全局 g_storages 按原始 entt::entity 存数据：多个 Scene 中 entity handle 可重叠。实体销毁时，Data storage 必须和 EnTT registry 一起移除对应数据，不能留下孤儿条目。

---
5. C++ 查询执行模型
5.1 原生侧完成求交
C# 把 TypeId 列表与访问模式传入 C++。C++ 使用 descriptor 取得每个 entt::sparse_set：
entt::runtime_view view{};
for (const QueryTerm& term : include_terms) {
    view.iterate(*ResolvePresenceStorage(scene, term.type_id));
}
for (const QueryTerm& term : exclude_terms) {
    view.exclude(*ResolvePresenceStorage(scene, term.type_id));
}
这允许如下组合：
C# RuntimeDataStorage<Velocity>
∩ C++ registry.storage<TransformComponent>
∩ C++ registry.storage<TagComponent>
runtime_view 会选较小 sparse-set 作为驱动集合，避免 C# 逐实体问 C++ “是否有组件”。
5.2 QueryBatch，而不是每帧巨大对象图
查询以可复用句柄和固定大小批次工作：
QueryHandle
  └─ Refresh / NextBatch
       ├─ raw entt entity handles
       ├─ 每个 Data 列的 dense index 或数据地址
       └─ Native 列的访问 capability
推荐返回 dense index 列，而不是默认创建 void* 指针表：
- index 通常为 32 位，指针通常为 64 位；
- C# 可通过 base + index * stride 计算数据地址；
- 结果缓冲更小，缓存局部性更好；
- 只为 DirectView/特殊布局使用显式地址表。
多组件 sparse-set 的交集不是 archetype chunk：每个组件在其 dense 数组中的 index 可以不同。不要把多组件查询误认为天然可返回多个同长度 Span<T>。
5.3 单组件快路径
单个 ICakeDataComponent 不需要求交，可直接暴露：
entity dense array + component dense array + count
仅当 native stride 严格等于 Unsafe.SizeOf<T>() 时，C# 才能包装为 Span<T>。若 stride 与 C# 元素大小不同，必须使用带 stride 的 ref struct 枚举器；不能用普通 Span<T>。

---
6. 原生组件的批量访问与 ABI view
6.1 默认：NativeBatchAdapter
对 Transform 等常用组件，提供一次调用处理整批数据的 C ABI：
native_transform_integrate_batch(
    entity_handles,
    velocity_column,
    count,
    delta_time)
该函数在 C++ 中查找原生 Transform 并调用语义方法，如 setPosition。优点：
- 每批一次跨边界调用，而非每实体两次 getter/setter；
- 自动维护 dirty、资源/物理副作用；
- C++ 可直接利用现有 entt storage；
- 不暴露 C++ 私有布局。
6.2 可选：DirectView
只有满足全部条件的 NativeComponent 才允许 DirectView：
1. C++ 对象为固定、标准布局的纯数据；
2. C# 和 C++ 字段类型、大小、offset、alignment 完全一致；
3. 写入副作用可由 view 提交阶段明确处理；
4. 有自动化 ABI 校验；
5. 生命周期中不在查询期间增删/扩容该 storage。
TransformComponent 若要开放 DirectView，不能简单删除 dirty。正确方式是：
- 保留 dirty 或改为版本号；
- ReadWrite view 结束时，用一个批量 native 调用标记本批实体 dirty；
- C++ static_assert(sizeof/alignof/offsetof)；
- C# 启动时验证 Unsafe.SizeOf<T>() 和生成的布局 hash。
Rigidbody2dComponent、资源组件等默认不做 DirectView。

---
7. 实体身份与生命周期
现有 C# Entity 面向脚本使用 UUID。Data ECS 内部应使用带 generation 的原生 entt::entity 值：
CakeBehaviour / GameObject 边界：UUID Entity
DataSystem Query 内部：raw DataEntity(entt handle + Scene generation)
查询中不要为每个命中项都创建 C# Entity 或把 handle 转 UUID；只有需要调用 GameObject/Behaviour API 时才转换。
生命周期规则：
- C++ Scene::createEntity 创建实体；
- Data component 的 Add/Remove 通过当前 Scene 的 Data storage；
- C++ Scene::destroyEntity 必须删除此实体的全部 Data component；
- Scene 卸载销毁其全部 Data storage；
- 持有的 DataEntity 和 query handle 不能跨 Scene 卸载使用。

---
8. 调度、并行与结构变更
8.1 第一阶段帧顺序
为先保证正确性，C# DataSystem、CakeBehaviour 和 C++ 原生系统之间采用阶段屏障：
1. Native pre-update / 输入准备
2. C# DataSystem（按数据读写声明分层并行）
3. DataCommandBuffer playback
4. NativeBatchAdapter / 数据同步
5. CakeBehaviour.Update（主线程、顺序）
6. C++ 物理、渲染等原生系统
现有 CakeBehaviourSystem 不应与无访问声明的 DataSystem 自动并行。它应被视为主线程屏障系统。
8.2 CommandBuffer
迭代时禁止直接 Create/Destroy/Add/Remove：
world.CommandBuffer.Add<Velocity>(entity, value);
world.CommandBuffer.Remove<Velocity>(entity);
world.CommandBuffer.Destroy(entity);
每个 worker 使用自己的 command stream，阶段末合并并单线程回放。回放期间可重排 dense storage；回放前暴露给 C# 的 ref、Span、query batch 随即失效。
8.3 查询缓存
QueryHandle 可跨帧复用，但缓存失效条件必须是所有相关 storage 的 structural version 变化：
- include/exclude 任一 storage 的 Add/Remove/Destroy；
- dense storage 扩容、compact、swap-remove；
- Scene 卸载；
- 描述符布局版本变化。
不能只因“组件值没有变化”就认为查询结果安全；结构变化可使指针和 dense index 失效。

---
9. 序列化与热重载
9.1 序列化
每个 C# Data component 序列化为：
stable TypeId + logical name + schema version + component bytes / C# serializer payload
跨平台和长期存档建议走字段级/二进制 schema serializer，不要假设裸内存字节永远兼容端序、packing 或未来字段调整。
NativeComponent 继续走现有 C++ ComponentDB 序列化。C# Data 组件由新的 Data serializer 负责，两者在 Scene asset 中保持不同类别。
9.2 热重载
纯逻辑改动、布局 hash 未改变：可保留数据。
字段增加、删除、顺序调整、大小/对齐变化：必须执行以下之一：
- 显式 migration；
- 清空并重建该 Data component；
- 停止热重载并提示用户。
禁止在布局变化后继续把旧内存解释为新 struct。

---
10. CakeBehaviour 与 NativeComponent 的最终定位
NativeComponent 不删除。它是：
1. CakeBehaviour 的单对象语义代理；
2. 编辑器/Inspector 的属性入口；
3. 统一查询 API 中原生组件的类型 token；
4. NativeBatchAdapter 与 DirectView 的元数据来源。
它不再是 DataSystem 热循环中的主要访问手段。
推荐职责：
场景
使用的 API
单个玩家脚本读自身 Transform
CakeBehaviour.Transform / GetComponent<T>
批量更新数万个 Velocity
World.Query<Velocity>()
批量筛选 Velocity + Transform
World.Query<Velocity, TransformComponent>()
批量写 Transform
NativeTransform.Batch... 或受控 DirectView
带 string/List/对象引用的数据
CakeComponent class

---
11. 实施计划
Phase 0：约束与测试骨架
- 定义 IQueryComponent、ICakeDataComponent、descriptor、stable TypeId；
- 验证 unmanaged、size、对齐和布局 hash；
- 建立每 Scene 的 Data storage 生命周期；
- 增加实体销毁、场景卸载、热重载测试。
Phase 1：C# Data 单组件路径
- RuntimeDataStorage<T> / 类型擦除 storage；
- Add/Get/Has/Remove；
- 单组件 dense view；
- World.Query<T>() 零逐实体 FFI；
- DataCommandBuffer。
Phase 2：统一多组件查询
- ComponentDescriptor.ResolvePresenceStorage；
- C++ runtime_view include/exclude；
- QueryHandle + QueryBatch；
- C# Data 组件的 mixed query；
- structural version 和缓存失效。
Phase 3：NativeComponent 混合筛选
- 将 ComponentDB native storage 注册为 query descriptor；
- 支持 World.Query<Velocity, TransformComponent>()；
- 保持 NativeComponent 只作 filter token；
- 增加多 Scene/Additive 场景测试。
Phase 4：批量 native adapter
- Transform 批量读写/dirty 标记；
- 常用物理、渲染组件的安全批量 API；
- Profile 对比“单对象 Native proxy”与“batch adapter”。
Phase 5：DirectView 与高级优化
- 为白名单纯数据 NativeComponent 生成 ABI view；
- 自动生成 C++/C# layout assertions；
- 需要时引入持久 group、chunk/gather 或 archetype 存储；
- 统一 C++ 与 C# 系统的访问图，减少阶段屏障。

---
12. 验收标准
功能正确性：
- Query<Velocity, TransformComponent> 的实体集合与 C++ EnTT 交集一致；
- Entity destroy、Remove、Scene unload 后无 Data storage 残留；
- Transform 批量写入后渲染/相机/灯光可观察到更新；
- 热重载布局变化不会静默解释旧数据。
性能：
- 单组件 Data query：每帧无托管分配；
- 多组件 QueryHandle：刷新时复用缓冲，不按帧构建对象图；
- DataSystem 循环内没有 Native_EntityHasComponent、字符串封送或逐实体 getter/setter；
- Native batch 路径跨边界次数与 batch 数量成正比，而非实体数量；
- 用 profiler 记录实体数、查询构建/刷新时间、GC 分配、FFI 次数和每系统耗时。

---
13. 最终原则
一套 Query API
一份实体身份
多种组件 storage
按组件能力决定数据访问方式
结构变更统一延迟
高频跨语言访问必须批量化
“统一查询”解决的是开发者能用一个表达式描述组件组合；“访问能力分级”保证这份统一不会牺牲原生组件的正确性与性能。