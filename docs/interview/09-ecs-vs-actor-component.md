# 09 ECS 架构与 Actor-Component 对比

对应面试题：ECS 架构是什么样的；跟 Actor-Component 体系有什么区别。

## 一句话

> 引擎原生层是数据导向的 ECS（EnTT registry + 组件纯数据 + 系统按读写声明调度 + 命令缓冲延迟结构变更）；脚本层又提供 Unity 风格的 GameObject/CakeBehaviour 门面——一套引擎同时有两种模型，用代理层桥接。

## 一、本引擎的 ECS 事实（先讲自己再讲对比）

代码位置：`engine/src/runtime/function/world/`

| 要素 | 实现 |
|---|---|
| 实体 | `Entity` 是 registry 的轻量句柄；`IDComponent` 携带 UUID + 名称；`Scene` 维护 UUID→Entity 映射 |
| 组件 | 纯数据结构体（TransformComponent 带 position/rotation/scale + dirty 标志），无虚函数、无行为 |
| 系统 | `System` 子类遍历 registry 视图（camera/mesh/sprite/light/physics/animation/mono 等系统）；渲染系统把数据写入 RenderScene |
| 并行 | 每个系统 `getAccess()` 声明 `readsComponents/writesComponents`（entt type_hash）→ `BuildGraphForSystems` 按读写依赖建边拓扑分层 → 层内 `TaskScheduler::submit` 并行、层间屏障（见 05） |
| 结构变更 | 增删组件/销毁实体不直接改 registry，走 `entity_requests.h` / `world_commands.h` 命令缓冲，同步点统一 apply——避免遍历中迭代器失效，也是并行安全的前提 |
| 组件初始化 | `Scene::onComponentAdd` 与组件/system hook |
| 持久化 | `Scene::serialize/deserialize` 经 Resource 模块的 `SceneRes/EntityRes/ComponentRes`，组件数据由反射 + serializer 写出（见 07 反射一节） |

### 脚本层的混合设计（这道题的加分点）

C# 侧（见 07）在 ECS 之上做了 Unity 风格门面：

- `GameObject` 是 Entity 的视图（Name/Transform/父子/组件操作），不是独立对象；
- `CakeBehaviour` 是**托管组件**：带生命周期回调（Awake/Start/Update...），存 `ManagedComponentStore`；`NativeComponent` 是原生组件的只读代理（按 entity+type 复用）；
- 结构变更在 C# 侧同样走命令缓冲（`CakeCommandBuffer`），与原生语义一致；
- 换句话说：**对外是 Actor-Component 的手感，对内是 ECS 的数据布局**。这个"为什么要门面"的问题答案：脚本作者的心智模型是 Unity 的，硬暴露 EnTT 视图会把用户绑死在引擎内部实现上。

## 二、ECS vs Actor-Component 对比表（背这个）

| 维度 | Actor-Component（Unity/UE Actor） | ECS（本引擎原生层） |
|---|---|---|
| 身份 | Actor 本身是有身份的对象（继承树 + GUID），持有组件数组 | Entity 只是 ID/索引，没有任何行为与数据 |
| 内存布局 | 组件散布堆上，按对象聚合，指针互相引用 | 组件按类型密集存储（EnTT sparse-set），同组件连续，缓存友好 |
| 逻辑位置 | 组件的虚函数（Update/OnCollision）里写行为 | 行为在系统里，批量遍历同类组件；组件只有数据 |
| 并行性 | 难——组件回调间依赖不可知，只能粗粒度锁或单线程 | 系统声明读写集，依赖图自动分层并行 |
| 组合 | 多重继承不够用 → 挂更多组件 + GetComponent 互查 | 天然组合：同一实体挂任意组件集 |
| 变换层级 | Actor 父子即层级，Transform 在 Actor 上 | 独立的 Hierarchy 组件 + Transform 组件，层级遍历是系统逻辑 |
| 结构变更 | 立即生效（AddComponent 当场构造） | 命令缓冲延迟到同步点（迭代安全 + 并行安全） |
| 典型瓶颈 | 缓存不命中、指针追逐 | 结构频繁变化时（命令缓冲 apply、稀疏集迁移）有代价 |

**一句话总结**：Actor-Component 是"以对象为中心的组合"，解决的是代码组织问题；ECS 是"以数据为中心的分解"，解决的是批量处理与并行问题。两者不是对立——本引擎就是 ECS 内核 + Actor 门面的混合体，UE 的 Actor 也是按系统批量处理（UObjectWorld），只是没把数据布局彻底拆开。

## 高频追问

- **为什么选 EnTT 而不是自研？** → sparse-set 实现成熟、视图迭代零分配；自研收益低。把精力放在系统调度（读写依赖分层）与命令缓冲这些 EnTT 不提供的部分。
- **命令缓冲为什么必须？** → 两个原因：① 系统遍历中增删组件会失效迭代器/触发稀疏集迁移；② 并行层内多个系统同时改结构需要串行化合并——统一 apply 是唯一的正确性锚点。
- **托管（C#）组件怎么进 ECS？** → 双存储：原生组件在 EnTT pool，托管组件在 `ManagedComponentStore`（类型→ComponentSet<T>）；查询层统一（`Query<T...>` 两种都能查），结构变更统一走命令缓冲再分别落地。
- **变换层级怎么做的？** → Hierarchy 组件维护父子，系统按层级遍历计算 world matrix；渲染侧只消费最终 world transform（PrimitiveSceneInfo）。
- **ECS 的缺点是什么？**（考察是否辩证）→ 数据打碎后"一个实体"的概念变弱，调试不如对象直观；逻辑分散在系统里，功能内聚性下降；结构高频变化场景（大量生灭的粒子/弹幕）命令缓冲有延迟与合并成本。
