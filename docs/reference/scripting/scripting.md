# Scripting

## 目录

| 目录 | 关键类型 |
|---|---|
| `engine/src/runtime/function/script/` | `ScriptSystem`、`ScriptEngine`、`ScriptRuntime`、`ScriptGlue`、native host |
| `engine/src/runtime/function/script/tooling/` | `ToolInterpreter` |
| `engine/src/scriptcore/Source/` | C# World、Scene、Entity、Behaviour、Render、UI、Input、Time API |
| `engine/src/scriptcore/Source/ScriptHub/` | unmanaged entrypoint、assembly、types、instance、field、snapshot、native calls |
| `engine/src/_generated/script/` | generated native calls、bindings、glue declarations |

## Runtime

`ScriptSystem` 创建 `ScriptEngine` 和 `ScriptRuntime`，初始化 `ScriptGlue`，加载 assembly classes，并管理 ToolInterpreter。关闭时依次关闭 tooling、glue、runtime 和 engine。

`ScriptEngine` 负责应用程序集的构建、加载、卸载和 source fingerprint。`ScriptRuntime` 维护 managed type、实例、field data、lifecycle 调用、snapshot 与 restore。

## Native Bridge

`ScriptGlue` 注册 native component、World、Entity、Scene、Input、Time、Resource、Render、UI 等函数。`native_host` 定义 native host 与托管程序集调用边界。

`ScriptHub` 是 C# 的 unmanaged 调用入口。其 method dispatch 覆盖 type scan、instance create、start/update/finalize、field read/write、snapshot/restore、assembly load/unload、entity component 操作和 native registration。

## C# API

`Source/World/` 定义 World、Entity、System、command buffer、component store、scheduler 和 behaviour binder。`Source/GameObject/` 定义 Behaviour 与 GameObject facade。`Source/Scene/` 定义 Scene 与 SceneManager。`Source/Render/`、`UI/`、`Input/`、`Time/`、`Resource/` 提供对应 native subsystem 的托管接口。

## Reload

脚本重载顺序：

```text
build app assembly
snapshot fields
clear runtime state
unload assembly
load assembly
register glue
reload classes
restore fields
commit fingerprint
```

跨边界状态使用 UUID、ID、JSON 或托管数据；native 指针、RHI handle、RenderGraph 临时资源和 scene/entity 临时句柄不跨程序集重载保存。
