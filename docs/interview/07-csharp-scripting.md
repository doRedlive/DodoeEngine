# 07 C# 脚本系统与反射架构

对应简历条目：C# 脚本系统。含面试高频题"反射架构是什么样的"。

## 一句话

> 用 hostfxr 手工宿主 CoreCLR，提供生命周期 + ECS 全套托管 API；C# 脚本支持热重载并做运行时字段快照/恢复；绑定代码由 libclang 元解析器自动生成——做出一套类似 Unity 的脚本系统。

## 一、C++ 宿主侧（`runtime/function/script/`）

### 手工宿主 CoreCLR（native_host.*）

不依赖托管宿主 exe，自己走完整引导链（native_host.cpp:155-253）：

1. `LoadLibraryW` 加载 `nethost.dll` → `get_hostfxr_path` 定位 hostfxr → 加载；
2. 解析 `hostfxr_initialize_for_runtime_config / hostfxr_get_runtime_delegate / hostfxr_close` 函数指针；
3. 读 `GreenCake.runtimeconfig.json` 初始化运行时，`hostfxr_set_runtime_property_value` 设置 `APP_CONTEXT_BASE_DIRECTORY` / `PROBING_DIRECTORIES`；
4. 取 `LOAD_ASM_AND_GET_FN` delegate 加载 `GreenCake.dll`，`UnmanagedCallersOnly` 方法经 `delegate_type == (const wchar_t*)-1` 约定获取函数指针。

### 入口与反向表

- **C++ → C#**：5 个 `[UnmanagedCallersOnly]` 入口（`Call` + `InvokeStart/Update/FixedUpdate/Finalize`，script_engine.cpp:157-191 解析一次）。所有 C++→C# 调用编码成 `ScriptCommand` 枚举（LoadAppAssembly/ScanTypes/CreateInstance/GetField/Snapshot/Restore/...）经 `Call` 的 switch 分发（ScriptHub.cs:18-63）。
- **C# → C++**：`script_glue.cpp` 用 `FOR_EACH_NATIVE_BINDING` 宏填一个 `NativeBindings` 结构（全是 `delegate* unmanaged` 函数指针），经 `RegisterNatives` 交给 C# 存进 `NativeCalls.b`（NativeCalls.cs:508-513）。字段顺序两侧来自同一个生成列表，天然一致。
- 字符串跨界：C++→C# 用 `CoTaskMem` UTF-8（`Marshal.PtrToStringUTF8` + `CoTaskMemFree`），C#→C++ 用 `Marshal.StringToCoTaskMemUTF8` 显式释放。

## 二、托管侧（`engine/src/scriptcore/Source/`，GreenCake 命名空间）

### 对象模型

| 类型 | 说明 |
|---|---|
| `World.Current` / `Scene` / `SceneManager` | 世界与场景门面 |
| `GameObject` / `Entity` | 场景对象门面与底层句柄（ulong UUID） |
| `CakeComponent` | 组件基类；`NativeComponent` 子类由 C++ ECS pool 持有 |
| `CakeBehaviour` | MonoBehaviour 式生命周期：Awake/Start/Update/FixedUpdate/LateUpdate/OnEnable/OnDisable/OnDestroy + 协程 + 碰撞回调 |
| `CakeSystem` | 全局逻辑系统，`[Reads]/[Writes]` 声明依赖 |

- Native 组件代理由 `NativeProxyFactory` 按 (entity, type) 复用（每类型上限 4096 缓存），代理不持原生状态。
- 结构变更（增删组件/销毁实体）统一走 `CakeCommandBuffer` 命令缓冲，帧同步点 apply；字段写入目标组件未创建时挂 pending-writes，apply 后回放。

### 调度（与 C++ 侧同构）

`[Reads]/[Writes]` 特性 → `CakeTaskGraph` Kahn 分层（CakeTaskGraph.cs:10-110）→ 同层 `Parallel.ForEach` 并行（CakeSystemScheduler.cs:50-54）。物理碰撞事件由 `CakeBehaviourSystem.DispatchCollisionEvents` 从原生事件分发。

## 三、反射架构（面试题：反射是什么样的？）

> 总起句：反射分两层——C++ 侧因为语言没有运行时反射，用**注解 + libclang 离线解析 + 代码生成**的编译期反射；C# 托管侧直接用 .NET System.Reflection。两侧通过生成的绑定代码桥接，运行时不做动态解析。

### C++ 侧四步链路

1. **注解层**：`reflection.h:13-21`，`__REFLECTION_PARSER__` 宏下 `META(...)` / `STRUCT(..., WhiteListFields, ScriptBind)` 展开为 Clang `__attribute__((annotate(...)))`；正常编译是空宏——**业务代码零侵入、零运行时开销**。标志：`WhiteListFields / WhiteListMethods / ScriptBind / Enable`（meta_data_config.h:3-17）。

2. **解析器**：自研 `DodoeParser`（`engine/src/metaparser`，libclang）。构建期自定义 target `DodoePreCompile`（precompile.cmake:53-70）：汇总头文件 → 遍历 AST 读 `CXCursor_AnnotateAttr` 提取注解与成员。

3. **代码生成**：一个解析器挂三个生成器（parser.cpp:44-62 注册）——`ReflectionGenerator`、`SerializerGenerator`、`ScriptBindingGenerator`，产出 `_generated/{reflection,serializer,script}`。以 `transform_component.reflection.gen.h:46-115` 为例，每类生成 `TypeXxxOperator`：
   - `ConstructorWithJson`：按字段名从 JSON 构造；
   - `WriteJsonByName`：按字段名序列化；
   - `Register()`：类级 `ClassFuncTuple`（基类列表/构造/序列化三元组）进 `class_map`，**每字段一个 `FieldFuncTuple`**（setter lambda、getter lambda、类名/字段名/类型名、`FieldType` 枚举、附加属性）进 `field_map`（reflection.cpp:15-16 两个静态 map）。

4. **运行时 API**：`TypeMeta` / `FieldAccessor` / `ReflectionInstance`（reflection.h:174-229）。按名字遍历字段读写、按名字 + JSON 动态构造（`newFromNameAndJson`）。

### 消费方（体现架构完整性）

- 编辑器 Inspector：反射遍历字段自动画 UI（配合 `FieldType` 与 `[Header]/[Enable]` 属性）；
- 场景序列化：`WriteJsonByName / ConstructorWithJson`；
- **脚本绑定**：标 `ScriptBind` 的组件由 `ScriptBindingGenerator` 生成 C# 代理类（`_generated/script/NativeComponents.generated.cs`，2113 行）+ C# 调用封装（`NativeCalls.generated.cs`）+ C++ glue（`script_glue.generated.cpp`，Mustache 模板渲染），`splice_generated.py` 注入 `NativeCalls.cs` / `script_glue.cpp` 的标记位。**改 C++ 组件头文件，三方绑定自动再生**。

### C# 侧运行时反射

- `ScanAssemblyTypes`：反射扫描 AppDomain + app ALC 里的 `CakeComponent/CakeSystem` 子类建类型表（ScriptHub.Types.cs:24-60）；
- 热重载字段快照/恢复：`GetFields(Public|Instance)` + `JsonSerializer`（见下节）；
- `[Reads]/[Writes]`：反射读取一次后按类型缓存（CakeSystemAccessCache.Resolve）。

**可能的追问**：
- *为什么不用 RTTI/运行时注册宏？* → RTTI 拿不到成员布局；宏注册每字段手写、易漏易错。libclang 方案单一事实源（头文件本身），且能顺带生成跨语言绑定。
- *字段为什么存 lambda tuple 而不是成员指针 offset？* → 类型擦除后统一塞 map，天然支持非平凡类型与元信息（名字/枚举/属性）；反射路径非热路径，间接调用代价可接受。

## 四、热重载完整链路（`script_system.cpp:25` 十步）

1. **指纹检测**：递归扫 `.cs`，哈希 `路径|大小|修改时间`（script_engine.cpp:48-83）；
2. **编译**：`dotnet build` 用户程序集（PlatformTool::BuildCSharpAssembly）；
3. **字段快照**：`ScriptCommand::Snapshot` → 遍历 `ObjectRegistry` 全部托管对象，公有实例字段序列化 JSON（ScriptHub.Snapshot.cs:11-31）；
4. **重置状态**：`ResetState` 清 ObjectRegistry/组件句柄/类型缓存/调度器（ScriptHub.Assembly.cs:83-94）；
5. **卸载 ALC**：collectible `AssemblyLoadContext`（isCollectible: true）`Unload()` + 3 次 `GC.Collect` 强制回收 + 等终结器（ScriptHub.Assembly.cs:70-104）；
6. **加载新程序集**：读 DLL 字节 `LoadFromStream`，返回 ALC GCHandle；
7. **重注册原生函数表**：`ScriptGlue::Register` 重发 `NativeBindings`（卸载的 ALC 里旧指针失效必须重绑）；
8. **重扫类型 + 重建系统实例**；
9. **字段恢复**：按句柄反序列化回填；
10. **提交新指纹**。

### 诚实提醒（被追问细节时）

字段恢复按托管 `ObjectRegistry` 句柄索引，而 `ResetState` **没有重置 `NextHandle`**，重建实例的句柄与快照 key 可能对不上，恢复并非 100% 可靠。建议表述："实现了基于字段快照的回滚/恢复机制，对跨重载的对象句柄映射还有改进空间"——不要说成完美的热重载状态保持。静态状态与正在执行的协程不保留，走整体卸载重建，生命周期重新 Awake/Start。

## 高频追问

- **"为什么手工宿主而不是 dotnet 主程序？"** → 引擎主进程必须是 C++（渲染/平台层），hostfxr 手工宿主是 .NET 官方支持的原生嵌入方式；nethost 定位 + runtimeconfig 初始化 + delegate 加载三步是标准链路。
- **"托管组件和原生组件怎么统一？"** → `ComponentManager.IsNative` 分流：native 走命令缓冲 + 原生类型 ID，managed 走 `ManagedComponentStore`（类型→ComponentSet<T>）；`Query<T...>` 两种都能查。
- **"C# 调 C++ 的性能？"** → 全部是 `delegate* unmanaged` 直接函数指针调用，无 P/Invoke marshal 层；字符串走 CoTaskMem UTF-8 一次性分配；高频路径（组件字段读写）由生成代码直连。
- **"为什么给 C# 系统也做任务图调度？"** → 与 C++ World 调度同构（[Reads]/[Writes] → Kahn 分层 → 层内并行），托管系统与原生系统语义一致，用户心智成本低。
