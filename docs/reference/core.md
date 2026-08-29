# Runtime Core

## 目录

`engine/src/runtime/core/`

| 子模块 | 关键文件 | 类型与职责 |
|---|---|---|
| Application | `application.h/.cpp` | `ApplicationSpecification`、`Application`、主循环、配置加载 |
| Context | `context/system_context.*` | `SystemContext`、运行时模块创建、逐帧调度、关闭顺序 |
| Event | `event/*` | `EventSystem`、事件发布、订阅、轮询和处理 |
| Layer | `layer/*` | `Layer`、`LayerStack`、attach/detach/update/render 回调 |
| Async | `async/*` | `TaskScheduler`、`TaskGraph` |
| Thread | `thread/*` | `RenderThread`、`ThreadPool`、`WaitGroup` |
| Memory | `memory/*` | `Managed`、allocator、thread allocator、deferred deletion |
| Meta | `meta/*` | reflection、serializer、`ComponentDB` |
| Project | `project/*` | `Project`、`ProjectSerializer`、active project |
| Object | `object/*` | `Object`、`ObjectID`、`PPtr` |
| Channel | `channel/*` | camera、gizmo、render channel |
| Platform | `engine/src/runtime/platform/*` | 平台工具与平台相关路径/窗口辅助能力 |
| Service | `engine/src/runtime/service/*` | `ServiceManager`、debug/editor/world service |
| Utility | `utils/*`、`math/*`、`log/*`、`debug/*` | UUID、JSON、数学、日志、profiling、debugger |

## Application 与 Context

`Application` 保存 `ApplicationSpecification`，创建 `SystemContext`，并执行：

```text
initializeModules
startRuntime
LayerStack::attach
EventSystem::Poll -> tickOneFrame -> EventSystem::Handle
LayerStack::detach
finalizeModules
postShutdown
```

`SystemContext` 管理 Window、Render、Input、Script、Physics、Time、UI、World、Debugger、ServiceManager 和渲染线程。`tickOneFrame` 分为 `updateTick` 与 `renderTick`。

## Meta 与序列化

`reflection/*` 提供类型与字段元信息注册；`serializer/*` 在 JSON 与反射类型之间转换；`ComponentDB` 提供组件 schema 和组件操作入口。反射宏使用 `REFLECTION_TYPE`、`CLASS`、`STRUCT`、`META`、`REFLECTION_BODY`。

## 生命周期所有权

- `Managed<T, CreateInfo>` 管理大多数系统的 `Create` / `Destroy` 生命周期。
- `Scope<T>` 和 `Ref<T>` 分别用于独占与共享所有权。
- `SystemContext` 是运行时系统实例的所有者；模块间访问通过 context getter 或已有的 `GetWorld()`、`GetRenderSystem()` 等 helper。
- `Application::Self()` 仅在 Application 已创建且未销毁的期间有效。
