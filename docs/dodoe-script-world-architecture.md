---
name: dodoe-script-world-architecture
description: Dodoe Engine 原生世界与 C# 托管世界的实体 / GameObject / 场景架构,以及当前实现中的已知问题与设计张力。
metadata:
  type: project
---

# Dodoe 脚本世界架构(Entity / GameObject / Scene)

## 1. 两套世界的模型

Dodoe 里同时存在两套并行的"实体"体系:

| | 原生世界(C++) | C# 托管世界(GreenCake) |
|---|---|---|
| 实体本体 | entt ECS registry(`Scene::m_reg`) | 无 —— `Entity` 只是 `ulong ID` 句柄 |
| 实体视图 | `Entity`(entt 封装) | `GameObject`(id + `_scene` + 行为列表) |
| 组件存储 | ECS 组件池(`ComponentDB` 反射) | `ManagedComponentStore`(`Dictionary<Type, ICakeComponentSet>`) |
| 组件访问 | 直接读 ECS | 原生组件走 FFI 代理 / 托管组件读 store |

**同一个实体可以同时拥有两类组件**。例如 doscn 里的 Player:

```
Player(uuid=1002)
├── 原生组件:  IDComponent / TagComponent / TransformComponent / SpriteRendererComponent
└── 托管组件:  OnlyOne.PlayerController (CakeBehaviour)
```

组件归属由 `ComponentManager.IsNative(type)` 判定:`typeof(NativeComponent).IsAssignableFrom(type)`。

## 2. 创建 Entity

### C++ 侧 —— 唯一真相源

```cpp
// scene.cpp:230
Entity Scene::createEntity(UUID uuid, const String& name) {
    auto entity = m_reg.create();
    auto id = entity.addComponent<IDComponent>(uuid, name);  // uuid + name
    entity.addComponent<TagComponent>();
    entity.addComponent<TransformComponent>();
    m_entity_umap[uuid] = entity;                            // 场景登记表
    return entity;
}
```

创建入口:
- **原生内部**:`Scene::deserialize`、sandbox、编辑器命令都走 `Scene::createEntity`。
- **C# 间接触发**:`Native_CreateEntity` → `script_glue.cpp` 的 `native_create_entity(name)` → `s->createEntity(name)` 返回 uuid。

### C# 侧 —— 只是句柄

```csharp
public class Entity {
    public readonly ulong ID;   // 就一个 id,原生实体的"句柄"
}
```

C# 的 `Entity` 不持有任何实体状态,所有 `GetComponent/AddComponent` 都透过这个 id 去查原生或托管 store。

## 3. 创建 GameObject

GameObject 是**纯 C# 概念**,是原生实体的视图。唯一手写创建路径是 `Scene.CreateGameObject`(scene.cs):

```csharp
public GameObject CreateGameObject(string name = "GameObject") {
    var entity = World.Current.CreateEntity(name);  // ① 原生真的创建一个实体
    var go = new GameObject { Entity = entity, _scene = this };  // ② C# 视图
    _gameObjects[go.ID] = go;  // ③ 登记进 C# 场景
    return go;
}
```

`GameObject.Transform` 不再是托管组件,而是懒加载的 `TransformComponent` FFI 代理(数据住在原生 ECS,见 §7 问题 3 已修复)。

## 4. 场景创建与加载

### C++ 侧

`World::loadScene(name, mode)`(world.cpp):

```
读 doscn → SceneRes
  → 没有场景就 createScene + scene->deserialize(scene_res)
  → (Single) deactivate 其他场景 → activateScene → setActiveScene
```

`Scene::deserialize`(scene.cpp)对每个实体条目:

```
createEntity(uuid, name)        ← 原生实体(带 ID/Tag/Transform)
→ DeserializeNativeComponents   ← 原生组件
→ DeserializeManagedComponents  ← 调 add_entity_component → C# ComponentManager.Add 往托管 store 塞组件
```

### C# 侧

`SceneManager.ActiveScene` getter 是**惰性**的:每帧 `CakeBehaviourSystem.OnUpdate` 访问它,一旦 `Native_WorldGetActiveSceneName()` 第一次返回非空,就建 C# Scene + `SyncFromNative()`(拉原生实体清单逐个 `RegisterEntity` 建视图)+ `NotifyLoad` + `SetActiveScene`。

### 同步机制(最近修复)

- `native_world_get_active_scene_entities`(script_glue)返回当前激活场景的 `[{id, name}]`。
- `Scene.SyncFromNative()` 在 C# Scene 首次创建时调用,对每个原生实体 `RegisterEntity(id)` 建 GameObject 视图(复用原生实体,**不**再 `Native_CreateEntity`)。
- `ComponentManager.Add<T>`:场景就绪且镜像命中 → 直接绑 GameObject + `QueueAwake`;否则挂 `BehaviourBinder` 孤儿表。
- `CakeBehaviourSystem.OnUpdate`:先 `BindOrphans` 补绑,再 `NotifyUpdate` → `ProcessAwakeQueue` → `Awake`。

## 5. 行为生命周期驱动

```
每帧 update → invoke_update → CakeBehaviourSystem.OnUpdate
  → Scene.NotifyUpdate → ProcessLifecycle → ProcessAwakeQueue/StartQueue/Updates
```

- `MonoSystem` 只注册在 **runtime systems**(world.cpp),所以 **Simulation 状态下 C# 脚本不更新**。
- sandbox 初始状态是 Simulation(非 Game 模式),切到 Runtime 后 `invoke_update` 才跑,孤儿行为才被补绑、Awake 才触发。

## 6. 已知问题与设计张力

以下是当前实现里刻意记录的问题,便于后续演进时对齐:

### 问题 1:双世界、双 store,边界模糊
同一实体原生组件在 ECS、托管组件在 C# store,`ComponentManager` 用类型判断二选一。新增一个组件时开发者必须先分清它该进哪个世界,这层划分没有在类型系统上强制。

### 问题 2:镜像是一次性的,不是持续的
`SyncFromNative` 只在 C# Scene 首次创建时跑一次。之后原生再创建/销毁/改名实体,C# 视图不会跟随。当初期望的"C++ 创建 entity 时 C# 同步创建 gameobject"并没有真正实现,只是激活时批量补了一次。

### 问题 3:~~Transform 镜像污染托管 store + special-case 跳过~~(已修复)
~~`RegisterEntity` / `CreateGameObject` 都会往托管 store 塞一个 `Transform`,于是 `GetEntityComponentData` 里不得不 `if (type == typeof(Transform)) continue;` 来防止存场景时把 `GreenCake.Transform` 写进 doscn。这是一个 hack:store 里有个"不算序列化组件"的组件。深层问题是 **Transform 不该进托管 store**,它应该像其他原生组件一样走 FFI 代理。~~

**修复**:删除整个 `Transform` C# 类(`Transform.cs`)。`GameObject.Transform` 现在是 `TransformComponent` FFI 代理(懒加载缓存),数据仍在原生 ECS;`GetEntityComponentData` 的 skip 一并删除。`Parent` / `ChildCount` / `GetChild` 移到 `GameObject` 上,不再有 `Transform.Parent` 这类包装。

### 问题 4:`SceneManager.ActiveScene` 是带副作用的 getter
读这个属性会建场景、同步实体、触发 `NotifyLoad/NotifyStart` 事件。任何地方读它都可能触发隐式初始化,场景生命周期由 getter + `SetActiveScene` + 原生三方驱动,难以推理。

### 问题 5:孤儿绑定依赖 Runtime 状态
`BehaviourBinder.BindOrphans` 在 `CakeBehaviourSystem.OnUpdate` 里跑,而这个系统只在 Runtime 状态更新。Simulation 状态下行为永不绑定、永不 Awake。如果设计意图是"Simulation 只做物理预览、不跑脚本",那是对的——但目前是隐式的,没有文档或代码层面保证。

### 问题 6:`Entity.AddComponent<T>(T component)` 丢参
`AddEntityComponent` 里 `Activator.CreateInstance` 出的实例被 `Entity.AddComponent<T>(T component)` 丢弃(`ComponentManager.Add<T>` 会 `new T()` 新建),白建一个实例。

### 问题 7:原生 binding 用 JSON + 线程局部字符串返回
`native_world_get_active_scene_entities` 用 `thread_local static String` 存 JSON 再返回 `c_str()`,调用方必须马上消费,否则被覆盖。模式松散,但已有先例(`native_object_get_type_name` 等)。

## 7. 可能的演进方向(未定)

1. **~~Transform 走原生代理~~(已完成)**:GameObject.Transform 已改为 FFI 代理原生 `TransformComponent`,不占托管 store,消除问题 3 的 special-case。
2. **持续同步**:原生在创建/销毁实体时向 C# push 事件(或 C# 按需拉取),让视图真实跟随原生,替代一次性镜像。
3. **显式场景生命周期**:用显式 `LoadScene/ActivateScene` 流程替代 `ActiveScene` getter 的副作用,消除问题 4。
4. **托管组件与原生组件统一注册**:让组件归属由注册表决定,而非运行时 `IsNative` 类型判断。
