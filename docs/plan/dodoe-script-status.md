# 脚本系统现状评估

> 文档日期：2026-08-13
> 适用范围：engine/src/runtime/function/script、engine/src/scriptcore、engine/src/metaparser
> 关联文档：docs/dodoe-script-world-architecture.md（架构 + 已知问题记录）

## 架构概览

```
原生侧（C++）                        托管侧（C# / GreenCake）
ScriptEngine ── FFI（script_glue.cpp）── ScriptHub
ScriptRuntime ── ScriptSystem ──────── World / Scene / GameObject / Entity
ToolInterpreter ── pybind11 ──────────（工具脚本 Python 绑定）
metaparser ── 反射生成 ────────────── NativeBindings / NativeComponents
热重载：可回收 AssemblyLoadContext（Load / Unload / CollectAndWait）
```

## 已实现

| 模块 | 说明 |
|---|---|
| C# 托管世界 | Entity（ulong 句柄）/ GameObject（视图）/ Scene / World；ManagedComponentStore 存托管组件 |
| 组件分流 | ComponentManager 按 `IsNative(type)` 走 FFI 代理或托管 store |
| Behaviour | Awake / OnEnable / Start / Update / OnDisable / OnDestroy 生命周期；BehaviourSystem + 孤儿补绑 |
| 系统调度 | CakeSystemScheduler 依赖图分层 + 同层并行；CakeCommandBuffer 延迟命令 |
| FFI 通道 | ScriptHub 命令集：实体/组件增删改查、系统 invoke_update、assembly load/unload、reset |
| 热重载 | 可回收 ALC（isCollectible）+ 依赖解析 + Unload + 三轮 GC 等待 |
| 反射生成 | metaparser 生成 NativeBindings.generated.cs / script_glue_bindings.generated.h / NativeComponents.generated.cs / py_bindings_components.generated.cpp（`_generated` 为构建产物） |
| Python 工具脚本 | pybind11：Entity 增删改查、组件字段、Color 等（`DODOE_PYTHON_ENABLED`） |

## 2D 物理脚本接入现状

### 已接入

| 侧 | 内容 |
|---|---|
| C# | Rigidbody2dComponent（gravity_scale / fixed_rotation）；BoxCollider2dComponent 字段（offset / size / density / friction / restitution / restitution_threshold） |
| Python | Rigidbody2dComponent（set_linear_velocity / apply_force_to_center / apply_linear_impulse_to_center + BodyType 枚举）；Box / Circle Collider2d 反射字段 |

### 未接入

| 缺口 | C# | Python |
|---|---|---|
| `is_sensor` / `layer` / `mask`（本次新增） | 缺：script_glue.cpp 字段绑定宏只到 restitution_threshold | 缺：依赖 metaparser 重新生成反射层 |
| DistanceJoint2dComponent / RevoluteJoint2dComponent | 缺：NativeComponents 类型清单无关节，无字段绑定 | 缺：DO_REG 未注册 |
| 物理查询 raycast / overlap / 碰撞事件 | 缺：无 FFI 入口 | 缺 |
| CircleCollider2dComponent 类型注册（既有遗漏） | 缺：字段 get/set 有，但类型清单未注册 | 已注册 |

注：`_generated` 层由 metaparser 在构建时重新生成，会自动带出新字段/新组件；script_glue.cpp 的类型清单与字段绑定、py_bindings_entity.cpp 的 DO_REG 为手写，需手工补齐。

## 已知问题现状核查

对照 docs/dodoe-script-world-architecture.md §6 逐项核对：

| # | 问题 | 现状 |
|---|---|---|
| 1 | 双世界双 store 边界模糊 | 未解决：仍按 `ComponentManager.IsNative` 类型判断归属，无类型系统强制 |
| 2 | 视图镜像一次性 | 未解决：`Scene.SyncFromNative` 仅激活时批量跑一次；原生侧创建/销毁实体不推送 |
| 3 | Transform 镜像污染托管 store + special-case | 已解决：`GetEntityComponentData` 现按 `Native_ComponentExists` 判定原生组件，special-case 已移除 |
| 4 | `SceneManager.ActiveScene` 带副作用 getter | 未解决：getter 仍隐式建场景、同步实体、触发 NotifyLoad / NotifyStart |
| 5 | 孤儿绑定依赖运行时状态 | 未解决：`BindOrphans` 挂在 `CakeBehaviourSystem.OnUpdate`，由原生侧 `invoke_update` 触发，Simulation/Runtime 差异隐式 |
| 6 | `Entity.AddComponent<T>(T component)` 丢参 | 未解决：入参被丢弃，内部 `ComponentManager.Add<T>()` 新建实例 |
| 7 | 原生 binding 用 JSON + thread_local 返回 | 未解决：`DEF_STR_RET`（thread_local static String）+ c_str 模式仍在 |

## 结论

- 脚本系统核心链路（托管世界、Behaviour、FFI、热重载、反射生成）完整可用。
- 2D 物理仅接入旧字段；本次新增的 sensor 字段、关节组件、物理查询两测均未接入。
- 架构文档 7 个已知问题中仅问题 3 已解决，其余 6 项仍存在。

## 建议补齐顺序

1. 组件字段与关节接入：script_glue.cpp 补 CircleCollider 类型注册 + is_sensor/layer/mask 字段 + 关节类型/字段绑定；py_bindings_entity.cpp 注册关节组件（生成层随 metaparser 重建自动带出）
2. 物理查询接入：raycast / overlap / 碰撞事件两侧 FFI 入口
3. 架构缺陷修复（按 §6 未解决项）：持续同步（问题 2）→ 显式场景生命周期（问题 4）→ AddComponent 丢参（问题 6）→ 组件归属注册表化（问题 1）


我已经看到了你上传的这份教程，它是一份完整的 CoreCLR(hostfxr) C++ ↔ C# 桥接方案，涵盖：
hostfxr 启动 CoreCLR
[UnmanagedCallersOnly] 双向调用
函数指针表（delegate* unmanaged）
AssemblyLoadContext 热重载
Snapshot/Restore
C++ Host 封装
一个完整的 Demo 工程结构
结合我之前和你的聊天记录，我发现它和你的 dodoe ECS 引擎 非常契合，因为你之前的目标一直是：
C++ ECS（EnTT）
C# 编写脚本
多个 Script Component
Unity 风格生命周期
热重载
尽可能少的跨语言开销
这份教程基本就是一个很好的起点。

不过，如果是让我来设计你的引擎，我不会完全照着这份教程实现，主要有几个原因。
① 命令通道用字符串，我觉得太重
教程采用
Call("create_instance", ...)
Call("invoke_update", ...)
Call("scan_types", ...)

然后 C# switch：
switch(method)
{
    case "scan_types":
    case "restore":
    ...
}

优点：
写起来简单
调试方便
缺点：
每一次都
UTF8
↓

string

↓

switch

↓

执行

对于引擎来说其实没有必要。
我会改成：
enum class ScriptCommand : uint16_t
{
    LoadAssembly,
    UnloadAssembly,
    ScanTypes,
    CreateObject,
    DestroyObject,
    Snapshot,
    Restore,
    ...
};

然后：
Call(Command::CreateObject,...)

C#：
switch(command)
{
    case ScriptCommand.CreateObject:
}

这样：
没有字符串解析
不会拼错
更容易自动生成

② NativeBindings 手写维护不了
教程：
struct NativeBindings
{
    void (*native_log)();
    int (*native_get_frame)();
};

对应 C#
delegate* unmanaged...

教程里也说了真实项目应该代码生成。
我完全同意。
对于你的引擎，我建议：
Engine API
↓

生成 bindings.json

↓

自动生成

NativeBindings.cs

NativeBindings.hpp

这样以后新增：
Transform_GetPosition()

Audio_Play()

Input_IsKeyDown()

全部自动生成。
不用人工维护。

③ 不建议所有功能都走 Call()
教程里面：
Call("invoke_update")
Call("create_instance")
Call("snapshot")

其实真正高频的是：
Update()

Start()

OnEnable()

OnDisable()


我会直接缓存：
delegate*

↓

ScriptMethodTable

例如：
Player

↓

Update ptr

↓

FixedUpdate ptr

↓

Start ptr

这样 Update 根本不用再走
Call()

↓

switch

↓

Invoke()

直接：
UpdatePtr(instance)

Unity IL2CPP 基本也是这种思路。

④ Snapshot 建议不要 JSON
教程：
field

↓

JSON

↓

restore

很好理解。
但是：
10000 Components

↓

JSON

↓

Deserialize

↓

GC

会非常重。
我建议：
Reflection

↓

Binary Serializer

↓

Blob

↓

Restore

甚至：
Field Offset

↓

memcpy

效率高很多。

⑤ ObjectRegistry 建议放 C++ 而不是 C#
教程：
C#

Dictionary<long, object>

负责所有对象生命周期。
对于一般程序没问题。
但是 ECS 引擎里：
Entity

↓

ScriptComponent

↓

Managed Object

真正的 Owner 应该是：
EnTT

↓

ScriptComponent

↓

GCHandle

而不是 C# 自己维护 Handle。
这样删除 Entity：
Destroy Entity

↓

Release GCHandle

↓

对象消失

更加自然。

⑥ 热重载流程我基本认可
教程：
Snapshot

↓

Unload ALC

↓

GC

↓

Load

↓

Register Native

↓

Restore

这个流程非常标准。
这一部分我基本不会改。
唯一会增加：
BeforeReload()

↓

Snapshot()

↓

Unload()

↓

Load()

↓

Restore()

↓

AfterReload()

让用户脚本可以响应热重载。

⑦ 最后再加 Reflection
你之前一直在问：
如何像 Unity/Godot 一样做 Inspector
实际上可以把 Reflection 放到 Core。
例如：
class Player : DoBehaviour
{
    [Serialize]
    public float Speed;

    [Range(0,100)]
    public int HP;
}

Core：
Scan

↓

FieldInfo

↓

Metadata

↓

返回 C++

Editor 就知道：
Speed

float

Slider

HP

int

Range

这一套和热重载完全兼容。

我对你的引擎最终建议
结合你之前介绍的 dodoe + EnTT + C# + 热重载，我会把整体架构设计成：
                +----------------------+
                |      C++ Engine      |
                |----------------------|
                | ECS (EnTT)           |
                | Renderer             |
                | Physics              |
                | Audio                |
                +----------▲-----------+
                           │
                 NativeBindings (auto-generated)
                           │
                +----------▼-----------+
                |      Core.dll        |
                |----------------------|
                | Reflection           |
                | Script Runtime       |
                | Hot Reload           |
                | GCHandle Manager     |
                +----------▲-----------+
                           │
                  AssemblyLoadContext
                           │
                +----------▼-----------+
                |      App.dll         |
                |----------------------|
                | Player               |
                | Enemy                |
                | Camera               |
                | User Scripts         |
                +----------------------+

我认为，这会比教程中的 Demo 更接近 Unity、Godot 等现代引擎的脚本架构，同时也更适合你的 ECS 引擎。整体而言，我会保留它的核心思路（Core 常驻、App 可回收、函数指针表、高频/低频双通道），但把命令通道、绑定表生成、对象生命周期管理、序列化和生命周期调用进一步工程化。
是不是也有这些问题
