# C# 脚本编写参考

## 总览

引擎的托管脚本运行在 `GreenCake` 命名空间下。C# 脚本编译为程序集，由 `ScriptRuntime` 加载；C# 通过 native calls（`NativeCalls`）与 C++ 运行时通信。脚本可以操作场景对象（GameObject/Entity）、组件、系统、行为、资源、输入、时间、UI，并注册编辑器工具入口。

典型的脚本组成：

- `CakeSystem`：全局逻辑，每帧驱动（类似 Unity 的 SystemBase / 早期 MonoBehaviour-free 模式）。
- `CakeBehaviour`：挂在 GameObject 上的组件行为（类似 MonoBehaviour）。
- 工具类静态方法 + `[ToolMenuItem]`：编辑器菜单入口。

## 对象模型

| 类型 | 说明 |
|---|---|
| `World` | 运行时世界，`World.Current` 访问；创建/销毁实体、查询组件。 |
| `Scene` / `SceneManager` | 场景的加载、激活、保存、卸载；`SceneManager.ActiveScene` 取当前激活场景。 |
| `GameObject` | 场景对象门面，持有 `Entity`；名字、Transform、父子关系、组件操作。 |
| `Entity` | 底层实体句柄，`ID`（ulong UUID）。 |

```csharp
Scene scene = SceneManager.ActiveScene;
if (scene == null) return;

GameObject go = scene.CreateGameObject("Player");
go.Transform.Position = new Vector3f(0f, 0f, 0f);

GameObject player = GameObject.Find("Player");
```

没有激活场景时 `ActiveScene` 返回 null，工具代码需要判空。

## 组件系统

所有组件继承 `CakeComponent`（只有一个 `Entity` 属性）。分两类：

- **NativeComponent**：由 C++ ECS pool 持有，引擎内置。例如 `TransformComponent`、`TagComponent`、`MeshRendererComponent`、`SpriteRendererComponent`、`Rigidbody2dComponent`、`BoxCollider2dComponent`、`AnimatorComponent` 等。`GetComponent<T>()` 每次返回一个新代理对象，不要跨帧缓存后持有。
- **托管组件**：直接继承 `CakeComponent` 的 C# 组件，由托管 store 持有。`CakeBehaviour` 是其中带生命周期回调的一类。

```csharp
go.AddComponent<MeshRendererComponent>();
MeshRendererComponent mr = go.GetComponent<MeshRendererComponent>();
bool has = go.HasComponent<MeshRendererComponent>();
go.RemoveComponent<MeshRendererComponent>();
```

`GetOrAddComponent<T>()` 在组件不存在时创建（`CakeBehaviour` 内可用）。

## Behaviour 生命周期

`CakeBehaviour` 提供与 MonoBehaviour 对齐的回调：

```csharp
public class PlayerController : CakeBehaviour
{
    public override void Awake() { }
    public override void OnEnable() { }
    public override void Start() { }
    public override void Update() { }
    public override void FixedUpdate() { }
    public override void LateUpdate() { }
    public override void OnDisable() { }
    public override void OnDestroy() { }

    public override void OnCollisionEnter2D(Collision2D collision) { }
    public override void OnTriggerEnter2D(NativeComponent other) { }

    protected override void StartCoroutine(IEnumerator routine) { }
}
```

- `Awake`/`Start` 由场景生命周期队列按序触发；`Update`/`FixedUpdate` 每帧/固定步驱动，行为在 `Enabled` 且 `ActiveInHierarchy` 时才执行。
- `Enabled`、`GameObject.ActiveSelf` 变化会触发 `OnEnable`/`OnDisable`。
- 协程：`StartCoroutine(IEnumerator)` / `StopCoroutine` / `StopAllCoroutines`。

## 系统与调度

`CakeSystem` 提供 `OnCreate` / `OnUpdate` / `OnFixedUpdate` / `OnDestroy`。

系统用 `[Reads]/[Writes]` 特性声明数据访问，`CakeTaskGraph` 据此构建依赖图并分层；同一层内多个系统通过 `Parallel.ForEach` 并行执行。

```csharp
[Reads<SpriteRendererComponent>()]
[Writes<TransformComponent>()]
public class MovementSystem : CakeSystem
{
    public override void OnUpdate()
    {
    }
}
```

约束：

- 不同层的系统按依赖顺序执行；同层并行，不要跨系统共享可变静态状态。
- 结构变更（增删组件、销毁实体）统一走命令缓冲，由引擎在同步点 apply，不直接改 registry。
- 纯托管计算的系统可以放心并行。

## 结构变更语义

`AddComponent<T>()`、`RemoveComponent<T>()`、`GameObject.Destroy`、`World.DestroyEntity` 都是**结构变更**，进入 World 命令缓冲，在帧同步点统一 apply，而不是立即生效。C# 与 C++ 的 ECS 语义一致。

字段写入不是结构变更。当写入目标组件尚未创建时，生成的 setter 会把写入挂到 pending-writes，命令缓冲 apply（组件创建）后立即回放。因此 `AddComponent<T>().Field = value` 这种写法可以直接用。

需要理解的两点：

- **读回时机**：在 apply 之前读字段读到的还是默认值。同一帧先加组件再立即读回，结果未定义。
- **回放失败**：回放时若组件仍不存在（如实体已销毁），写入静默丢弃。

## 场景

```csharp
SceneManager.LoadScene("Level1", LoadSceneMode.Single);
SceneManager.LoadSceneAsync("Level2", LoadSceneMode.Additive);
SceneManager.UnloadScene("Level1");
SceneManager.SetActiveScene(scene);
```

`Scene.Save()` 在序列化前会自动冲刷命令缓冲（apply 结构变更 + 回放 pending-writes），因此"创建对象 → 设置字段 → Save"一步落盘，脚本不需要手动 flush。

场景事件：`SceneManager.OnSceneLoaded` / `OnSceneUnloaded` / `OnActiveSceneChanged`，以及 `Scene` 上的 `OnSceneStart` / `OnSceneUpdate` / `OnSceneStop`。

## 查询

```csharp
foreach (var entity in World.Current.Query<TagComponent>())
{
    var tag = entity.GetComponent<TagComponent>();
}

GameObject found = scene.Find("Player");
GameObject byId = scene.FindByID(go.ID);
GameObject byTag = scene.FindByTag("Enemy");
```

`Query<T>()` 支持最多 5 个类型参数；`Query(params Type[])` 支持任意组合。托管组件与 native 组件均可作为查询类型。

## 资源与对象

`Object` 是资源/运行时对象的托管门面，以 `InstanceID` + `Generation` 标识，`IsValid` 检查是否存活。资源对象由原生侧持有，跨程序集重载只保存 UUID/ID，不保存指针。

```csharp
Mesh mesh = Mesh.Load("Models/backpack/backpack.obj");
Texture tex = Resources.Load<Texture>("Textures/albedo.png");
bool alive = mesh.IsValid;
```

`MeshRendererComponent.Mesh` 这类对象字段存的是对象引用，序列化时写为资源 UUID，重载场景后由渲染系统按 UUID 重新解析。

## 时间

| 成员 | 说明 |
|---|---|
| `Time.DeltaTime` | 帧间隔 |
| `Time.FixedDeltaTime` | 固定步长（场景更新时写入） |
| `Time.time` | 累计时间 |
| `Time.realtimeSinceStartup` | 启动至今真实时间 |
| `Time.timeScale` | 时间缩放 |
| `Time.unscaledDeltaTime` | 未缩放帧间隔 |

## 调试

```csharp
Debug.Log("message");
Debug.LogWarning("warn");
Debug.LogError("error");
```

日志走 `Native_Log`，输出到引擎日志系统。

## 编辑器工具

静态方法加 `[ToolMenuItem("菜单路径")]`，在编辑器 Tools 菜单显示，主线程执行：

```csharp
public static class MyTools
{
    [ToolMenuItem("Tools/Import 3D Model")]
    public static void ImportModel() { }
}
```

其他可用于编辑器与序列化的特性：`[SerializeField]`、`[Header]`、`[TextArea]`、`[RequireComponent]`、`[DisallowMultipleComponent]`、`[Serializable]`。

## 编写模式示例

```csharp
public static class ModelImporter
{
    private const string ModelPath = "Models/backpack/backpack.obj";

    [ToolMenuItem("Tools/Import 3D Model")]
    public static void ImportModel()
    {
        Scene scene = SceneManager.ActiveScene;
        if (scene == null) { Debug.LogError("No active scene."); return; }

        Mesh mesh = Mesh.Load(ModelPath);
        if (mesh == null) { Debug.LogError("Failed to load mesh."); return; }

        GameObject go = scene.CreateGameObject("ImportedModel");
        go.AddComponent<MeshRendererComponent>().Mesh = mesh;
        go.Transform.Position = new Vector3f(0f, 0f, 0f);
    }
}
```
