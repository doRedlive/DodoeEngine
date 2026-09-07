# Physics、Animation、Time

## 目录

| 目录 | 关键类型 |
|---|---|
| `engine/src/runtime/function/physics/` | `PhysicsSystem`、`PhysicsWorld`、`Physics2dWorld`、debug draw |
| `engine/src/runtime/function/animation/` | animation、clip、skeleton、controller、manager |
| `engine/src/runtime/function/time/` | `TimeSystem` |
| `engine/src/runtime/function/world/systems/` | physics、animation、camera 等 World system |

## Physics

`PhysicsSystem` 管理 2D/3D physics world。`PhysicsWorld` 和 `Physics2dWorld` 创建刚体、collider、contact 与 query。World 中的 `Physics2dSystem`、`Physics3dSystem` 将 entity component 与 physics object 同步。

相关组件包括 `RigidbodyComponent`、`Rigidbody2dComponent`、box/sphere/capsule collider、2D collider 与 joint 组件。contact phase 和 body type 由 physics API 枚举定义。

## Animation

animation 模块包含 `Animation`、`AnimClip`、`Anim2DClip`、`Skeleton`、`AnimatorController`、`AnimationManager`。World 中的 `AnimatorSystem` 读取 animator/pose/drive component，并将动画状态应用到实体数据。

## Time

`TimeSystem` 提供 delta time、总时间和帧时间。Application 的 update 通过 `TimeSystem::getDeltaTime()` 向 Layer、World、UI 等系统传递帧间隔。
