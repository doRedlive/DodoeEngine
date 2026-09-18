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

- **NativeComponent**：由 C++ ECS pool 持有，引擎内置。例如 `TransformComponent`、`TagComponent`、`MeshRendererComponent`、`SpriteRendererComponent`、`Rigidbody2dComponent`、`BoxCollider2dComponent`、`AnimatorComponent` 等。`GetComponent<T>()` 返回按 (entity, type) 复用的只读代理——同一实体同一类型返回同一实例；代理不持有原生状态，实体销毁后读取会得到默认值，不要把它当作独立对象跨帧缓存。
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

## 编辑器 API（GreenCake.Editor）

`GreenCake.Editor` 是 C# 反向操作编辑器的绑定层，等价于 Unity 的 `UnityEditor`。它只在 Cakery 编辑器进程内可用；在沙盒/运行时下 `EditorApplication.IsAvailable` 为 `false`，所有调用返回空结果。

```csharp
using GreenCake.Editor;

ulong[] selected = Selection.Get();
Selection.Set(uuid);                    // 单选
Selection.Set(uuidA, uuidB);            // 多选
Selection.Clear();

EntityInfo[] entities = EditorScene.GetEntities();
if (EditorScene.TryFind("Player", out var player)) { }

ulong created = EditorApplication.CreateEntity("Enemy");
EditorApplication.RenameEntity(created, "Enemy 2");
EditorApplication.ReparentEntity(created, player.Uuid);
EditorApplication.DeleteEntity(created);

EditorApplication.Undo();
EditorApplication.Redo();
```

| 类型 | 说明 |
|---|---|
| `EditorApplication` | `IsAvailable` / `IsReady` / `State` / `IsPlaying` / `ProjectRoot`；`Execute` / `Undo` / `Redo`；`CreateEntity` / `DeleteEntity` / `RenameEntity` / `ReparentEntity` |
| `Selection` | `Get` / `Active` / `IsEmpty` / `Contains` / `Set` / `Clear` |
| `EditorScene` | `Name` / `EntityCount` / `GetEntities` / `TryFind` |
| `EntityInfo` | `Uuid` / `Parent` / `Name` |
| `PlayState` | `Edit` / `Playing` / `Paused` |

约束：

- 所有操作与编辑器文档一致，走 `EditorHistory`，因此可撤销。
- 结构变更（创建/删除/重命名/重父级）会触发文档同步，不要在原生系统同一帧内高频调用。
- 绑定由 `EditorScriptBridge` 在运行时启动时注册，随脚本热重载自动重发；未打开项目（运行时代理未启动）时 `IsReady` 为 `false`。

## 自定义编辑器 UI

C# 可以声明式地扩展编辑器界面：`[CustomEditor]` 替换组件在 Inspector 中的绘制，`[EditorWindow]` 注册一个可停靠窗口。两者都用 `EditorGUI` 构造控件树，由 Qt 侧渲染，事件回传到 C#。

```csharp
using GreenCake.Editor;

[CustomEditor(typeof(TransformComponent))]
public sealed class TransformEditor : Editor
{
    public override void OnInspectorGUI(EditorGUI gui)
    {
        ulong uuid = Selection.Active;
        gui.Help("Transform 由 C# 自定义编辑器绘制");
        gui.BeginRow();
        gui.Button("reset", "Reset Position");
        gui.EndRow();
    }

    public override void OnEvent(EditorEvent e)
    {
        if (e.ControlId == "reset")
            Debug.Log("reset clicked");
    }
}

[EditorWindow("Tuning")]
public sealed class TuningWindow : EditorWindow
{
    private bool _enabled = true;
    private float _speed = 1.0f;

    public override void OnGUI(EditorGUI gui)
    {
        gui.Toggle("enabled", "Enabled", _enabled);
        gui.Float("speed", "Speed", _speed, 0.0f, 10.0f);
        gui.Button("spawn", "Spawn Cube");
    }

    public override void OnEvent(EditorEvent e)
    {
        if (e.ControlId == "enabled") _enabled = e.Value == "true";
        if (e.ControlId == "spawn") EditorApplication.CreateEntity("Cube");
    }
}
```

`EditorGUI` 控件：`Label` / `Button` / `Toggle` / `Float` / `Int` / `Text` / `Vector3` / `Color` / `Object` / `Asset` / `Separator` / `Space` / `Help` / `Property`，容器：`BeginRow`/`EndRow`、`BeginColumn`/`EndColumn`、`BeginFoldout`/`EndFoldout`。

`Property` 把控件绑定到当前组件字段，写入走 `EditorSession::updateComponent`（可撤销），控件类型由 `inspectComponent` 元数据或 JSON 值推断：

```csharp
[CustomEditor(typeof(TransformComponent))]
public sealed class TransformEditor : Editor
{
    public override void OnInspectorGUI(EditorGUI gui)
    {
        gui.Property("position");                 // 使用元数据渲染向量控件
        gui.Property("rotation", "Rotation");     // 自定义标签
        gui.Property("scale");
    }
}
```

约束与现状：

- `Property` 只在自定义 Inspector 生效（编辑器窗口没有组件上下文，会显示 `(unbound)`）。
- Inspector 自定义编辑器在选中实体刷新时重建；`[EditorWindow]` 窗口由定时器（约 150ms）轮询 C# `OnGUI` 并在控件树变化时重建，输入焦点在窗口内时跳过重建。
- 自定义编辑器与窗口在脚本热重载时自动重新扫描。



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
