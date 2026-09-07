# World

## 目录

`engine/src/runtime/function/world/`

| 子模块 | 关键类型 |
|---|---|
| `world.*` | `World`、`WorldState`、scene 切换与 runtime/simulation 生命周期 |
| `scene.*` | `Scene`、实体创建删除、UUID 查找、序列化与反序列化 |
| `entity.*` | `Entity`、组件访问与实体句柄 |
| `registry.*` | EnTT registry 封装 |
| `components/*` | transform、camera、renderer、light、collider、rigidbody、tilemap、hierarchy 等组件 |
| `systems/*` | camera、mesh/sprite/tilemap renderer、light、physics、animation、mono 等系统 |
| `entity_requests.h`、`world_commands.h` | 实体请求与命令缓冲 |

## World 与 Scene

`World` 保存 active scene 并切换 `Simulation`、`Runtime`、`Pause` 等状态。场景加载、创建、启动、停止与更新由 World 分发。

`Scene` 同时维护 `Registry` 和 UUID 到 `Entity` 的映射。`createEntity`、`destroyEntity`、`tryGetEntityByUUID`、`serialize`、`deserialize` 是场景状态的主要入口。

## Entity 与 Components

`Entity` 是 scene registry 中实体的轻量句柄。组件通过 `addComponent`、`getComponent`、`hasComponent`、`removeComponent` 操作。组件添加后的初始化逻辑由 `Scene::onComponentAdd` 和相应 system/component hook 处理。

`IDComponent` 提供 UUID 与名称；`TransformComponent` 提供 position、rotation、scale；renderer/camera/light/physics/tilemap 组件把 World 数据接入对应 subsystem。

## System 生命周期

scene 在 runtime 与 simulation 状态下分别触发：

```text
onRuntimeStart / onRuntimeUpdate / onRuntimeStop
onSimulationStart / onSimulationUpdate / onSimulationStop
```

`System` 子类遍历 registry 中匹配组件的实体。renderer system 写入 RenderScene；physics system 创建和同步物理对象；`MonoSystem` 驱动托管脚本行为。

## 场景持久化

`Scene::serialize` 与 `Scene::deserialize` 使用 Resource 模块的 `SceneRes`、`EntityRes`、`ComponentRes`。组件数据通过 `ComponentDB` 和 serializer 写入 native/managed component 载荷。
