组件系统重构：统一 API + 零重复（去「双端 ECS」）
目标：消除 native/managed 双存储的冗余与手工同步（P1），代码生成原生镜像组件样板（P2），统一对象模型与 AddComponent 通路（P3，GameObject 在 Entity 之上、Unity 风）。
不追求「单一存储」，追求「一套访问 API + 零重复」——让架构变成 Unity 的真实样子：引擎组件在 native、自定义组件在托管，统一在一个 GetComponent<T> 之下。
本文档独立于热重载（ALC）方案；实施顺序上二者解耦，但本重构会顺带收敛热重载所需的引用面（见 §8）。

---
一、问题回顾
当前存在两套并行 ECS，且部分重叠、手工同步：

Native（C++）
Managed（C#）
存储
entt registry（Scene 持有，引擎权威）
World.Current._componentSets（第二套 ECS）
权威范围
原生组件（Transform/物理/渲染…）
托管组件成员关系 + 全部托管实例
托管 Component 混住两种本质不同的东西：
1. 原生镜像组件（Components.cs：TransformComponent 等）—— 零状态代理，属性 get/set 全是按 entityId 打到 native 的 InternalCall；数据真身在 entt。
2. 用户自定义组件（MonoBehaviour 子类等）—— 真有状态，只活在托管侧。
核心痛点：
- P1｜双存储手工同步：原生镜像组件明明零状态，却仍被塞进 World._componentSets 冗余一份，每次 add/remove 要穿三跳 InternalCall 同步 entt 与托管 store（Entity.cs:29、GameObject.cs:69），native 删除时靠 TryGetEntityByUuid 兜底，存在发散风险。
- P2｜200+ 手写样板：给一个原生组件加字段要改 4 处（native struct、InternalCalls.cs、Components.cs、script_glue.cpp）。且 components.h 里 light/line/rect/foliage 等原生组件根本没镜像。
- P3｜对象模型与 add 通路分叉：Entity.AddComponent（先 Has 再 Add）、GameObject.AddComponent（AddOrReplace + 排 Awake）、World.AddComponent（裸 Add）三条语义不一致的通路。
为什么不「全塞进 entt」：entt 是编译期静态类型的，用户在 C# 新定义的组件类型在 C++ 编译期不存在，不可能成为一等 entt 组件；类型擦除（GCHandle 袋）会让自定义组件退化、失去按类型索引查询与一等地位。故放弃「单一存储」，改走「分治 + 统一 API」。

---
二、目标架构
原则：一个组件只有一个家；访问 API 统一；自定义组件在托管侧保持一等公民。
                 ┌─────────────────────────────────────────┐
   用户代码  →   │  entity.GetComponent<T>() / Add / Has …   │  ← 唯一入口
                 └───────────────┬───────────────┬───────────┘
                        T 是原生镜像            T 是用户类型
                                 │               │
                    InternalCall │               │ 托管 store
                                 ▼               ▼
                        ┌──────────────┐   ┌────────────────────┐
                        │ entt (C++)   │   │ ManagedComponentStore│
                        │ 引擎组件权威 │   │ ComponentSet<T> 稀疏集│
                        └──────────────┘   └────────────────────┘
2.1 存储分区（零重叠）
- 原生/引擎组件 → entt（不变），永不镜像进托管。
- 用户自定义组件（含数据组件与 MonoBehaviour）→ ManagedComponentStore（即现 World._componentSets 收敛后的产物），按类型索引、可查询、可序列化、可进 Inspector —— 这就是 Unity 式自定义组件 ECS。
2.2 统一访问 API（消灭「双端」体感）
用户永远只写一套 API，分区对其透明。分派靠类型标记：
// Components.cs（基类层，手写）
public abstract class Component { public Entity Entity { get; internal set; } }

// 原生镜像代理的公共基类（代码生成的代理都继承它，作为分派标记）
public abstract class NativeComponent : Component { }

// 托管态行为组件
public class MonoBehaviour : Component { /* Awake/Start/Update/... 生命周期 */ }
// 纯数据的用户组件直接继承 Component
分派实现（放在权威通路 ComponentManager，见 §4）：
public static bool IsNative(Type t) => typeof(NativeComponent).IsAssignableFrom(t);   // 结果可缓存

public T GetComponent<T>(Entity e) where T : Component
{
    if (IsNative(typeof(T)))
        return NativeProxyFactory.Create<T>(e);      // 按需构造零状态代理，绝不入库
    return ManagedComponentStore.Get<T>(e.ID);       // 托管 store
}
- 原生镜像组件：GetComponent 返回按需构造的代理（new T{ Entity = e }，零状态、极廉价，或按 entity 缓存 flyweight）。Has/Add/Remove 走 InternalCall 落 entt（复用现有 Native_EntityHasComponent/Native_EntityAddComponent/Native_EntityRemoveComponent，改为按 Type 分派，不再传实例）。不进任何托管容器 → P1 消失。
- 用户组件：全部在 ManagedComponentStore，Query<T> 即稀疏集查找，O(1)/快速迭代。
2.3 代理工厂（替代 new() 约束）
原生代理由代码生成，注册构造工厂，避免泛型 new() 约束扩散：
// 由 §3 代码生成写入 Components.generated.cs 的静态构造函数
NativeProxyFactory.Register<TransformComponent>(e => new TransformComponent { Entity = e });

---
三、P2：代码生成原生镜像组件样板
3.1 挂载点
复用现有 libclang metaparser（engine/src/metaparser）：它已有 ReflectionGenerator / SerializerGenerator（Mustache 模板 + GeneratorInterface，产物落 _generated/，见 generator.h、serializer_generator.h）。新增一个 ScriptBindingGenerator 与之并列即可。
3.2 标记
在 native 组件上用 META 标签声明「需要脚本绑定」及字段可见性（复用 meta_data_config.h 里的 Enable 语义，新增一个 ScriptBind 组件级标签）：
REFLECTION_TYPE(TransformComponent)
STRUCT(TransformComponent, WhiteListFields, ScriptBind) {   // ScriptBind：参与脚本绑定生成
    REFLECTION_BODY(TransformComponent)
    META(Enable) Vector3f position{...};   // Enable：作为脚本可见属性生成 get/set
    META(Enable) Vector3f rotation{...};
    META(Enable) Vector3f scale{...};
    bool dirty{false};                       // 无 META → 不导出
    // 需要方法绑定的（如 Rigidbody2d.ApplyForce）：额外 META 标法见 §3.4
};
3.3 三份产物（由同一元数据驱动，一处改动）
对每个 ScriptBind 组件、每个 META(Enable) 字段，ScriptBindingGenerator 输出：
1. Components.generated.cs —— C# 代理类：
[NativeComponent]                       // 或直接继承 NativeComponent（见 §2.2）
public sealed class TransformComponent : NativeComponent
{
    public Vector3f Position {
        get { InternalCalls.Native_TransformComponent_position_get(Entity.ID, out var v); return v; }
        set { InternalCalls.Native_TransformComponent_position_set(Entity.ID, ref value); }
    }
    // rotation / scale ...
}
2. InternalCalls.generated.cs —— extern 声明（每字段 get/set 一对）。
3. script_glue.generated.cpp —— C++ getter/setter 实现 + mono_add_internal_call 注册 + NativeComponents 组注册：
static void Native_TransformComponent_position_get(uint64_t id, Vector3f* out) {
    if (auto* c = TryGetComponent<TransformComponent>(id)) *out = c->getPosition(); else *out = {};
}
static void Native_TransformComponent_position_set(uint64_t id, Vector3f* in) {
    if (auto* c = TryGetComponent<TransformComponent>(id)) c->setPosition(*in);
}
3.4 类型映射表（生成器核心）
生成器内置 native 字段类型 → (C# 类型, 编组方式) 映射：
native
C#
编组
float/int/ui32/ui64/bool
float/int/uint/ulong/bool
值传递
Vector2f/Vector3f/Vector4f/Color
同名 struct
out/ref
std::string
string
MonoString* ↔ utf8
enum（如 CameraType/BodyType）
生成 C# enum + int 编组
值传递
- 约定命名：Native_<Component>_<field>_get/set，取代现有手写的不规则命名。
- getter/setter 访问器约定：native 侧要求 getXxx()/setXxx() 或直接公有字段；生成器按反射拿到的 accessor 决定。对需要方法绑定的组件（Rigidbody2d.ApplyForceToCenter 等）用 META 在方法上标注，走一份「方法绑定」模板（可作为 P2 的第二阶段，先覆盖字段属性）。
3.5 收益
- 加/改一个原生组件字段 = 只改 native struct 的一行 META(Enable)，其余三份自动重生成。
- light/line/rect/foliage 等未镜像组件，打上 ScriptBind 即自动获得完整 C# 代理。
- 手写的 Components.cs / InternalCalls.cs 中的原生镜像部分整体被生成产物取代（仅保留基类 Component/NativeComponent/MonoBehaviour 与用户组件）。

---
四、P3：统一对象模型与 add 通路
4.1 单一权威通路
新增内部 ComponentManager（C# 侧，或作为 Entity 的内部实现），作为唯一 add/has/get/remove 权威：
internal static class ComponentManager
{
    public static T Add<T>(Entity e, T instance = null) where T : Component
    {
        if (Has<T>(e)) return Get<T>(e);
        if (IsNative(typeof(T))) {
            InternalCalls.Native_EntityAddComponent(e.ID, typeof(T));   // entt emplace
            return NativeProxyFactory.Create<T>(e);
        }
        instance ??= Activator.CreateInstance<T>();
        instance.Entity = e;
        ManagedComponentStore.Add(e.ID, instance);
        if (instance is MonoBehaviour mb) BehaviourScheduler.Register(mb);   // 生命周期唯一注册点
        return instance;
    }
    // Has / Get / Remove 同样按 IsNative 分派
}
Entity.AddComponent、GameObject.AddComponent、World.AddComponent 全部委托 ComponentManager，消除三套分叉语义（P3）。
4.2 GameObject 在 Entity 之上（Unity 风，你的选择）
- GameObject 保留为 Entity 之上的薄门面：持有 Entity，AddComponent<T> → ComponentManager.Add(Entity, ...)。
- MonoBehaviour 的生命周期成员关系来自单一注册点 BehaviourScheduler（取代现 GameObjectManager.cs 里散落的 _awakeQueue/_startQueue/_activeBehaviours 等多份集合），或让 BehaviourScheduler 直接从 ManagedComponentStore 按 MonoBehaviour 类型迭代 —— 优先后者，进一步减少托管侧引用面（利好 §8 热重载）。
- MonoBehaviourSystem（MonoBehaviourSystem.cs）继续驱动 BehaviourScheduler.ProcessLifecycle。

---
五、序列化
- 原生组件：entt → ComponentRes（scene.cpp SerializeNativeComponents，不变）。
- 用户组件：ManagedComponentStore 是成员关系权威，SerializeMonoComponents 从它枚举（现有 round-trip 保留，但只返回用户组件、不再是混合袋）；字段值继续走 MonoComponentInstance::serializeFields（script_class.cpp:283）。
- 原生镜像组件不再进托管 store 后，SerializeMonoComponents 的返回天然干净，ExternalCalls.GetEntityMonoComponents（ExternalCalls.cs:26）里过滤 Native_ComponentExists 的逻辑可简化/删除（因为托管 store 里已不含原生镜像类型）。

---
六、分阶段实施
建议顺序（每步可独立编译、独立验证）：
1. 引入基类标记：加 NativeComponent 基类 + NativeProxyFactory；现有原生镜像类先手动改为继承 NativeComponent（暂不删）。
2. 统一 API 分派：实现 ComponentManager，Entity/GameObject/World 的 add/has/get/remove 全部改为委托它；GetComponent<原生类型> 改为按需代理、停止写入托管 store。→ P1 落地。
3. 收敛托管 store：World._componentSets 收敛为 ManagedComponentStore，只保留用户组件；BehaviourScheduler 从 store 迭代，删 GameObjectManager 的冗余集合。→ P3 落地。
4. P2 代码生成：实现 ScriptBindingGenerator，为 ScriptBind 组件生成三份产物；删除手写 Components.cs/InternalCalls.cs 中的原生镜像部分与 script_glue.cpp 手写 getter/setter。→ P2 落地。
5. 清理：简化 ExternalCalls 过滤逻辑；修正 Native_ComponentExists 命名/语义、Entity.cs:26 死代码等瑕疵。
步骤 1–3 不依赖代码生成，可先行验证核心去重；步骤 4 工量最大，独立推进。

---
七、风险与权衡
- 托管类型查询跨界：Query<原生类型> 需跨到 native 迭代 entt。gameplay 量级可接受；如成热点，按帧缓存实体列表 + 脏标记。用户组件查询仍在托管侧，无跨界。
- 按需代理开销：原生镜像代理每次 GetComponent 新建对象。零状态、极小；如在热路径频繁调用，可按 entity+type 缓存 flyweight。
- 代码生成器复杂度：类型映射表 + 方法绑定模板是主要工量；建议先覆盖「字段属性」全量，方法绑定作为第二阶段。
- 命名迁移：生成的 InternalCall 命名规范化会改动现有用户脚本引用面（若有直接调 InternalCalls 的）——InternalCalls 为 internal，用户不应直接用，风险低。

---
八、与热重载（ALC）方案的衔接
本重构顺带收敛热重载所需的引用面（原 P0）：
- 原生镜像组件不再进托管容器，托管侧 app 类型实例集中到 ManagedComponentStore 一处（外加 BehaviourScheduler 若不并入 store）。
- 将来做 ALC 就地卸载时，「断引用」从「到处追 World/GameObject/GameObjectManager 多份集合」简化为「清 ManagedComponentStore 一个入口」。
- 因此本重构先做、独立做是安全的，且会让后续热重载方案的 teardown 更干净。

---
九、验证
1. 编译：C++ 引擎 + metaparser 代码生成 + dotnet build scriptcore 全绿。
2. 原生组件读写：脚本读写 Transform.Position 等，值与 entt 一致（无双写、无滞后）。
3. 自定义组件：新建一个用户 MonoBehaviour 与一个纯数据 Component，验证 Add/Get/Has/Remove/Query 与生命周期（Awake/Start/Update/OnDestroy）正常。
4. 零重复验证：加断言/日志确认原生镜像类型不出现在 ManagedComponentStore；Query<TransformComponent> 走 native 路径。
5. 序列化往返：含原生组件 + 用户组件的实体，序列化→反序列化后，原生字段值、用户组件字段值、成员关系一致。
6. 代码生成回归：给某原生组件加一个 META(Enable) 字段，仅改 native struct，重跑生成，确认 C#/glue 三份产物自动出现且可用。
7. 统一通路回归：Entity.AddComponent / GameObject.AddComponent / World.AddComponent 三入口行为一致（都走 ComponentManager）。