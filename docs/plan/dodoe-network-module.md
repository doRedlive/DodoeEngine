# 网络模块方案（分层架构 + 客户端-服务器状态同步）

状态：设计中
日期：2026-08-26
范围：Dodoe runtime、SystemContext、ECS 组件/系统、C# 桥接、头less 服务器模式

## 1. 背景与目标

### 1.1 现状

- 引擎目前**没有任何网络代码**（全仓库无 socket/Enet/asio 依赖）。
- 模块模式统一：`XxxSystem : Managed<XxxSystem, XxxCreateInfo>`，`Scope<>` 挂在 `SystemContext`，`initializeModules` 创建、`updateTick` 逐帧驱动、`finalizeModules` 反序销毁（`PhysicsSystem` 为最标准范本）。
- 已有可复用的底座：
  - `EventSystem`（entt::dispatcher）→ 网络生命周期事件直接发布/订阅；
  - `spsc_queue.h` + `TaskScheduler` + 线程池 → 网络收发线程与主线程解耦；
  - EnTT ECS + `World`/`Scene`/`Entity` → 网络实体/组件直接进现有世界；
  - C# 脚本（GreenCake）输入桥接（`NativeCalls.cs` + `script_glue.cpp`）→ 网络 API 照此模式暴露给玩法层。

### 1.2 两个缺口

1. **无头less/服务器模式**：`AppMode` 仅 `Game`/`Sandbox`/`Editor`，且 `WindowManager`/渲染/输入/UI 必建。dedicated server（无窗口、无渲染）目前跑不起来——这是网络模块的前置缺口。
2. **现有 `Serializer` 是 JSON 反射版**（nlohmann json，面向场景资源），不适合做紧凑网络包。需要独立的位流编解码器。

### 1.3 目标

搭建四层网络架构，**先实现"服务器权威状态同步"（模型 A）**，结构上不排斥后续"确定性锁步"（模型 B）。分阶段交付：先打通传输，再补协议/连接，最后做游戏层同步。

### 1.4 核心决策（默认值 + 可替换点）

| 决策点 | 默认 | 可替换 |
|---|---|---|
| 联网模型 | 客户端-服务器（服务器权威） | P2P + NAT 穿透/Relay |
| 同步策略 | 状态同步（属性复制 + 插值） | 确定性锁步（需另行论证确定性） |
| 传输层 | 自研 Winsock UDP 最小层 | Enet / SteamNetworkingSockets |
| 玩法层 API | C# GreenCake 桥接 | 纯 C++ 玩法 |
| 服务器形态 | `AppMode::Server`（头less） | 编辑器内 host + 隐身服务器 |

---

## 2. 核心模型

### 2.1 分层图

```
┌─ 4. 玩法同步层  NetworkBehaviour / [ServerRpc] / 属性复制    （C# + world system）
├─ 3. 协议层      NetMessage 注册分发 + NetSerializer(bitstream) （C++）
├─ 2. 连接层      NetConnection 状态机（握手/心跳/重传/超时）
└─ 1. 传输层      NetTransport 抽象 → WinsockUdpTransport
                        │
             收包线程 → SPSC 队列 → 主线程 updateTick drain
```

主线程单向依赖：玩法层只通过 `NetworkSystem` 接口说话，不直接碰 socket。

### 2.2 传输层 `NetTransport`

```cpp
class NetTransport {
    virtual Bool init(const NetAddress& bind_addr, UInt16 port) = 0;
    virtual void shutdown() = 0;
    virtual Int32 recvFrom(NetAddress& src, Byte* buf, Int32 max_len) = 0;
    virtual Bool sendTo(const NetAddress& dst, const Byte* data, Int32 len) = 0;
};
```

- `NetAddress` 封装 IP:Port（IPv4 起步，`WinSock2` 的 `sockaddr_in`）。
- `WinsockUdpTransport` 为默认实现：UDP socket，**收包线程**阻塞 `recvfrom` → 推入 SPSC 队列；`sendTo` 主线程直发（低量场景足够，后续可加发送队列）。
- 抽象存在是为了将来能换 Enet（`EnetTransport`），传输层之上的代码零改动。

### 2.3 连接层 `NetConnection`

UDP 无连接语义，可靠/会话由本层补：

- 状态机：`Disconnected → Connecting → Connected → TimedOut/Closed`。
- 握手：Client 发 `Handshake`（含协议版本、客户端 build）→ Server 回 `Welcome`（分配 connection_id、服务器 tick、玩家 NetId）。
- 心跳：间隔发送 `Heartbeat`，超时未收判 `TimedOut`。
- 可靠子层（M1–M3 阶段从简）：序号 + ACK + 简单重传；多通道/优先级留到同步层成熟后再设计，不在本期做全。

### 2.4 协议层 `NetMessage` + `NetSerializer`

```cpp
struct NetMessage {
    UInt16 type_id;
    UInt32 sequence;
    // 载荷由 NetSerializer 编码
};
```

- `NetSerializer`：独立位流编解码器（**不复用 JSON Serializer**）。能力：`WriteBits/ReadBits`、定长/变长整型、量化浮点、紧凑字符串、量化向量；`Begin(Message)/End()` 校验预算。
- 消息注册表：`type_id → (name, 编码/解码函数)`，集中一个 TU 注册，C# 侧按名字引用。
- 序列化策略：属性同步走"上一个快照为基准的 delta"（`NetSerializer::WriteDelta`），少动不传。

### 2.5 网络事件（走 EventSystem）

```cpp
struct NetPeerConnected;    // 服务器侧：新玩家接入
struct NetPeerDisconnected;
struct NetConnected;        // 客户端侧：本端连上
struct NetDisconnected;
struct NetMessageReceived;  // 通用消息（低量场景），高频消息走消息表直发
```

用 `EventSystem::Subscribe/Publish` 接线，编辑器/调试面板可直接订阅。

### 2.6 网络组件 + 同步系统

```cpp
NetworkIdentityComponent    // 网络唯一身份
  UInt32 net_id;            // 服务器分配的全局 NetId
  UInt32 owner_id;          // 拥有者 connection_id（0 = 服务器）
  Bool   is_static;         // 静态实体只同步一次，不进快照 diff

NetworkTransformComponent   // 需要网络同步的变换状态
  // 本地权威模式：本地直接写 TransformComponent，对端靠插值收敛
  // 服务器权威模式：本地把输入/状态上行，服务器广播
  // 插值缓存（prev/curr 快照 + alpha）
```

- 注册进 `ComponentDB::registerBuiltinComponents`（`addable=false`，仅由网络系统管理，Inspector 隐藏）。
- `network_sync_system`（`world/systems/`，继承现有 `System` 基类）：
  - **发送侧**：按 tick rate（默认 20Hz，可配）遍历 `NetworkIdentityComponent`，构建增量快照。
  - **接收侧**：解析快照 → 写 `NetworkTransformComponent` 插值缓存 → `transform_system` 渲染时插值。
- 与 `World` 集成：客户端由服务器 `SpawnEntity` 消息创建远端实体（带 `NetworkIdentityComponent`），`World::start` 时清除非静态网络实体。

### 2.7 头less/Server 模式

- `AppMode` 增加 `Server`：`SystemContext::initializeModules` 在 Server 模式**跳过** `WindowManager`/`RenderSystem`/`InputManager`/`UIManager`，保留 `TimeSystem`/`PhysicsSystem`/`World`/`ScriptSystem`/`NetworkSystem`；`tickOneFrame` 跳过 `renderTick`。
- 生命周期/线程模型不变，`Managed` + `Scope` 所有权保持一致。
- 本地联调路径：`Server` 进程 + `Game` 进程跑同一台机器（127.0.0.1）；后续可编辑器 `Play` 时内嵌 host。

### 2.8 C# 桥接（GreenCake）

沿 input 既有模式：

- `NativeCalls.cs` 增 `native_net_*` 函数指针族（`native_net_host`、`native_net_connect`、`native_net_send`、`native_net_poll_event` 等）。
- `script_glue.cpp` 注册对应实现，形态与 `native_input_*` 一致。
- C# API：
  - `NetworkManager`：`Host(port)` / `Connect(ip, port)` / `Disconnect()` / `Status`；
  - `NetworkBehaviour`（`Behaviour` 子类）：`[ServerRpc]` / `[ClientRpc]` 标记方法，属性同步用 `[Replicated]` 字段；
  - `NetworkTransform` 组件封装。

---

## 3. 文件改动

### 3.1 传输层

- `engine/src/runtime/function/network/net_address.h` — IP:Port 封装。
- `engine/src/runtime/function/network/net_transport.h` — 传输抽象基类。
- `engine/src/runtime/function/network/transports/winsock_udp_transport.h/.cpp` — Winsock UDP 实现（收包线程 + SPSC 推送）。
- （可选）`engine/src/runtime/function/network/transports/enet_transport.h/.cpp` — 换装 Enet。

### 3.2 网络系统 + SystemContext 接线

- `engine/src/runtime/function/network/network_system.h/.cpp` — `Managed<NetworkSystem, NetworkSystemCreateInfo>`；接口：`host/connect/disconnect`、`sendMessage`、`poll(dt)`、`getConnectionCount`；内部持 `NetTransport`、连接表、消息注册表。
- `engine/src/runtime/function/network/net_connection.h/.cpp` — 连接状态机。
- `engine/src/runtime/core/context/system_context.h/.cpp` —
  - 成员 `Scope<NetworkSystem> m_network_system` + getter `GetNetworkSystem()`；
  - `initializeModules` 创建（Server/Game 模式按配置）；`finalizeModules` 反序 `NetworkSystem::Destroy`（先于 World 销毁，断网先行）；
  - `updateTick` 调 `m_network_system->poll(dt)`（drain SPSC + 心跳 + 消息分发）。
- 配置：`NetworkSystemCreateInfo` 含端口、协议版本、tick rate、最大连接数，从 `ApplicationSpecification`/config 文件加载（复用现有 json 配置链路）。

### 3.3 协议层

- `engine/src/runtime/function/network/net_serializer.h/.cpp` — 位流编解码。
- `engine/src/runtime/function/network/net_message.h` + `net_messages.cpp` — `NetMessage` 头 + 类型注册表 + 各消息编解码。

### 3.4 事件

- `engine/src/runtime/function/network/net_events.h` — `NetPeerConnected` / `NetPeerDisconnected` / `NetConnected` / `NetDisconnected` / `NetMessageReceived`。

### 3.5 组件 + 同步系统

- `engine/src/runtime/function/world/components/network_identity_component.h`
- `engine/src/runtime/function/world/components/network_transform_component.h`
- `engine/src/runtime/function/world/systems/network_sync_system.h/.cpp`
- `engine/src/runtime/core/meta/component_db.cpp` — `registerBuiltinComponents` 注册两个网络组件（`addable=false`）。
- `engine/src/runtime/function/script/script_glue.cpp` — `NativeComponents` 列表加入网络组件（C# 可见）。

### 3.6 C# 桥接

- `engine/src/scriptcore/Source/NativeCalls.cs` — `native_net_*` 函数指针族。
- `engine/src/scriptcore/Source/Network/NetworkManager.cs`
- `engine/src/scriptcore/Source/Network/NetworkBehaviour.cs`（含 `[ServerRpc]`/`[ClientRpc]`/`[Replicated]` 特性）
- `engine/src/runtime/function/script/script_glue.cpp` — 注册 `native_net_*` 实现。

### 3.7 头less/Server 模式

- `engine/src/runtime/core/application.h` — `AppMode` 加 `Server`。
- `engine/src/runtime/core/context/system_context.cpp` — Server 模式跳过 Window/Render/Input/UI 初始化与 `renderTick`。
- `engine/src/runtime/function/window/window_manager.h/.cpp` — 仅构造入口改为按模式可选（Server 下不创建原生窗口）。

### 3.8 CMake

- `engine/src/runtime/CMakeLists.txt` — `GLOB_RECURSE CONFIGURE_DEPENDS` 已自动覆盖新源文件，无需逐条加；新增链接 `ws2_32`（Windows socket）。

---

## 4. 实施顺序

1. **Milestone 1 — 传输打通**：`NetAddress` + `WinsockUdpTransport` + `NetworkSystem` 骨架（收包线程 + SPSC）→ 两个实例本地互通 ping/pong（事件/日志确认）。验收：本机起 Server + Game，日志互见 `NetPeerConnected` / ping 往返可打印。
2. **Milestone 2 — 协议层 + C# 桥接**：`NetSerializer` + 消息注册/分发 + `net_events`；`native_net_*` + `NetworkManager` C# API（`Host`/`Connect`/`Send`/`Disconnect`）。验收：C# 脚本里 `NetworkManager.Connect` 能收发自定义消息。
3. **Milestone 3 — 连接管理**：握手 + 协议版本校验 + 心跳 + 超时断线 + 玩家进出场消息。验收：杀掉对端进程能触发 `NetDisconnected`。
4. **Milestone 4 — 游戏同步**：`NetworkIdentityComponent` + `NetworkTransformComponent` + `network_sync_system`（快照增量 + 插值）。验收：Server 端移动一个实体，Game 端平滑跟随；静态实体只同步一次。
5. **Milestone 5 — RPC + 场景加载同步**：`[ServerRpc]`/`[ClientRpc]`、`SpawnEntity`/`DespawnEntity`、场景内实体由服务器批量下发。验收：C# 调 `[ServerRpc]` 能被服务器执行；新玩家加入能看到场上所有网络实体。
6. **Milestone 6 — 扩展（可选）**：interest management / 带宽预算、可靠性通道优先级、加密与防作弊。

---

## 5. 边界与风险

- **头less 模式侵入现有初始化**：`SystemContext` 目前假定 Window/Render/Input/UI 全部存在（`startRuntime` 依赖窗口、`renderTick` 依赖渲染线程）。Server 模式需要一次性梳理条件化，是 M1 前置但独立的工作项。
- **UDP 可靠子层复杂度**：序号/ACK/重传/乱序缓存是网络里最容易做错的模块。M1–M3 用单序号 + stop-and-wait 起步，可靠子层单独设计，不在传输打通阶段贪多。
- **NetSerializer 与反射脱钩**：位流编解码先手写。若想用反射元数据自动生成位流编解码，需扩展 metaparser，工作量大，列为后续优化不做本期目标。
- **C# 桥接边界**：函数指针参数需 blittable；字符串/数组 marshal 跟随 input 既有模式，避免把非 POD 直接传原生侧。
- **确定性（模型 B）风险**：若未来走确定性锁步，要求 C# 玩法 + Jolt/Box2D 确定性；当前未做确定性验证，模型 B 需单独论证，本期不承诺。
- **线程竞争**：收包 SPSC 队列在 `finalizeModules` 关闭顺序中必须先于 `NetworkSystem` 销毁停止收包线程；回包对象生命周期与编辑器退出路径需核对。
- **环境**：本地联调用 127.0.0.1；公网/NAT 属 P2P 扩展，本期不处理。
- **安全**：无加密、无反作弊，仅适合联调与局域网；上生产前另行评估。
