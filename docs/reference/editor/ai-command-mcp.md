# AI 命令面与 MCP Server

编辑器的 AI 自动化分三层:命令系统(引擎能力)、执行记录(事件流)、MCP Server(协议适配)。MCP 不是引擎能力本身,只是把命令面暴露给 AI 客户端的传输层。

```
AI Client (Claude / opencode / 任意 MCP 客户端)
    │ JSON-RPC 2.0 (Streamable HTTP, 127.0.0.1)
    ▼
McpServer (engine/src/editor/core/console/McpServer.cpp)
    │ 主线程命令队列(EditorSession::tick 每 16ms drain)
    ▼
AICommandBridge(白名单/拒绝列表/事务会话)
    ▼
CommandRegistry(executeStructured + 参数校验)
    ▼
EditorSession / EditHistory(真实执行 + 可撤销)
    │
    ▼
ExecutionRecorder(记录 → WorkflowPanel / workflow.graph)
```

## 1. 三层职责

| 层 | 文件 | 职责 |
|---|---|---|
| CommandRegistry | `core/console/CommandRegistry.*`、`core/commands/builtin_commands.cpp` | 注册命令、行式/结构化执行、参数校验、`toolSchema()` |
| ExecutionRecorder | `core/console/ExecutionRecorder.*` | 每次执行的 author/session/mutating/耗时记录,`toJSON`/`toGraphJSON` |
| AICommandBridge | `core/console/AICommandBridge.*` | AI 身份(author=ai)、allowlist/denylist、事务会话(checkpoint/revert) |
| McpServer | `core/console/McpServer.*`、`core/console/mcp/McpHttp.h`、`mcp/McpRpc.h` | 内置 MCP Server:Streamable HTTP + JSON-RPC 2.0,tools/resources/prompts |

关键原则:**MCP 层不直接修改任何场景数据**。所有工具调用经主线程命令队列(`EditorSession::tick()` 里的 `drain()`),由 `CommandRegistry` 在正确的线程执行,同时产生执行记录与撤销事务。

## 2. MCP Server

### 启动方式

- Console 命令:`mcp.start [port]`(默认 7800,`0` 自动选口)、`mcp.stop`、`mcp.status`
- 编辑器配置 `editor.json`:

```json
{
  "mcp": {
    "autostart": true,
    "port": 7800
  }
}
```

- 命令行:`Cakery.exe --mcp-port 7800`(优先级高于配置文件)

只绑定 `127.0.0.1`,不对局域网开放。

### 客户端接入示例

opencode(`~/.config/opencode/opencode.json`):

```json
{
  "mcp": {
    "cakery": {
      "type": "remote",
      "url": "http://127.0.0.1:7800/mcp",
      "enabled": true
    }
  }
}
```

Claude Desktop(`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "cakery": {
      "type": "http",
      "url": "http://127.0.0.1:7800/mcp"
    }
  }
}
```

curl 手工调试:

```bash
# initialize(响应头会带 Mcp-Session-Id,后续请求需原样带回)
curl -i -X POST http://127.0.0.1:7800/mcp \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18"}}'

# 工具调用
curl -X POST http://127.0.0.1:7800/mcp \
  -H 'Content-Type: application/json' \
  -H 'Mcp-Session-Id: <上一步返回的 id>' \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"entity.create","arguments":{"name":"Tree"}}}'
```

### 协议面

| 方法 | 说明 |
|---|---|
| `initialize` | 协议版本协商(2024-11-05 / 2025-03-26 / 2025-06-18),返回 `Mcp-Session-Id` |
| `tools/list` | `CommandRegistry::toolSchema()` 自动转换(`parameters` → `inputSchema`),无需为单个工具写适配 |
| `tools/call` | 入队 → 主线程执行 → 同步返回;`tools/call` 走 SSE 流,执行中可推送 `notifications/message`、`notifications/progress` |
| `resources/list` / `resources/read` | `cakery://scene/current`(场景 JSON)、`cakery://recorder/records`(执行记录)、`cakery://workflow-template/<name>` |
| `prompts/list` / `prompts/get` | `optimize_scene`、`summarize_workflow`、`inspect_entity` |
| `ping` / `notifications/*` | 空响应 / 202 |

工具执行错误以 `result.isError=true` + 文本 JSON 返回;协议错误(未知工具等)走 JSON-RPC error。

### 长任务与进度

`benchmark.run` 是 cooperative 异步命令:调用立即入队,主线程每 tick 泵 4 帧(编辑器不冻结),进度经 SSE 推送 `notifications/progress`(需要客户端在请求 `_meta.progressToken` 里提供 token),完成后同一连接返回 summary。300s 无完成则返回超时,且**未执行的超时调用会被取消,不会在之后悄悄生效**。

## 3. 安全模型

- 默认拒绝列表(`AICommandBridge::denylist`,启动时设置):`scene.new`、`scene.open`、`workflow.clear`、`mcp.start`、`mcp.stop`、`mcp.deny`、`mcp.allow`
- `mcp.deny <prefix>` / `mcp.allow <prefix>` 运行时调整(Console 用户可用,AI 不可调)
- allowlist 为空 = 未拒绝的全部可调;需要最小化时用 `AICommandBridge::setAllowlist`
- 所有 AI 执行的命令在 ExecutionRecorder 中 `author=ai`,WorkflowPanel 可按作者过滤

## 4. AI 事务会话

`AICommandBridge` 的核心工作流,现在有命令入口:

```
ai.begin_session goal="构建塔防关卡"
entity.create name=Tower
component.add <uuid> Sprite
component.set <uuid> Transform.position.x 10
benchmark.run frames=240      # 验证
ai.end_session keep=false     # 不满意 → 整体回滚到 checkpoint
```

`begin` 创建 EditHistory checkpoint,`end(keep=false)` 整体回滚;期间所有执行记录携带同一 session id,`workflow.graph` 可视化依赖关系。

## 5. 观测工具

| 命令 | 内容 |
|---|---|
| `query.entities` / `query.components` / `query.assets` | 场景/组件/资产查询 |
| `query.perf.frame_stats` | 帧遥测(CPU 线程耗时、draw call、`gpu_frame_ms` — RHI timer query) |
| `query.perf.passes` | render graph 拓扑 + 每 pass CPU 耗时(`cpu_ms`) |
| `query.logs` | 引擎日志(级别/数量过滤) |
| `query.history` | undo/redo 栈状态与标签 |
| `workflow.list` / `workflow.graph` / `workflow.capture` / `workflow.run` | 执行记录与模板回放(UUID 重绑定) |

`gpu_frame_ms` 由 `RenderSystem` 用 cutie-rhi 的 `createTimerQuery/beginTimerQuery/pollTimerQuery` 测量(GPU 帧耗时,毫秒);后端不支持 timer query 时保持 0。

## 6. 测试

`tests/unit/editor/test_console.cpp`(命令校验、执行记录、模板捕获/过滤)、`tests/unit/editor/test_mcp_protocol.cpp`(HTTP 解析、JSON-RPC 形状、协议协商、工具目录映射)。

## 7. 已知边界

- `component.set` 嵌套字段用点路径(`Transform.position.x`),不支持数组下标路径
- 视口截图需要 RHI readback API(尚无),未实现
- overdraw heat map、RenderDoc 集成属于渲染 Phase 4,未实现
- MCP Server 每请求一条连接(`Connection: close`),`tools/call` 除外(SSE)
