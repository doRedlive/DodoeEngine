# 配置系统

Dodoe 的配置分为两大类：**编译期配置**（CMake 决定能力是否存在）和**运行期配置**（统一由 ConfigSystem 分层加载）。运行期配置遵循同一条优先级链，所有 debug 开关的合法 token 只有一份注册表。

## 运行期统一模型

### 优先级链（低 → 高）

```text
C++ 结构体默认值  <  engine-builtin  <  project  <  env  <  cli
(ApplicationSpecification)  (engine/res/configs)  (<项目>/)  (环境变量)  (命令行)
```

| 层 | ConfigLayer | 来源 | 说明 |
|---|---|---|---|
| 默认值 | `Default` | C++ 结构体成员初始化 | 任何文件都没有提供时的兜底 |
| 引擎内置 | `EngineBuiltin` | `engine/res/configs/app_config.json` | 引擎出厂默认 |
| 项目 | `Project` | `<项目目录>/app_config.json` | 深度合并（merge_patch），只覆盖出现的字段 |
| 环境变量 | `Env` | `DODOE_DEBUG_SWITCHES` / `DODOE_DEBUG_SWITCHES_OFF` | 仅 debug 开关 |
| 命令行 | `Cli` | `--config=` / `--dswitch=` / `--dswitch-off=` | 最高优先级 |

要点：

- `--config=<path>` **独占替换**：指定后不再走分层合并，直接加载该文件（找不到则报错退出加载）。
- 未指定 `--config` 时：先加载引擎内置作为基底，再 merge_patch 项目配置（项目里没写的字段继承引擎内置，而不是回落到 C++ 默认值）。
- `ApplicationSpecification::config_file` 为遗留字段，目前没有赋值方，作为 Project 层之后的额外候选保留。
- 最终生效的层与文件路径会记录在 `ConfigSource`，启动日志打印 `[layer:xxx]`。

### 核心 API

- `runtime/core/config/config_system.h` — `ConfigSystem`：
  - `Initialize(cli_args)` / `Shutdown()`：解析环境变量与 CLI 的 debug 开关
  - `BuildAppConfig(cli_args, out_source, explicit_path)`：分层合并 app 配置，返回最终 JSON
  - `IsSwitchEnabled / IsSwitchOn / IsSwitchPresent / Switches() / SwitchSummary()`：开关查询
- `runtime/core/config/config_switch_registry.h` — `ConfigSwitchRegistry`：**唯一**的已知开关列表（key、描述、默认值），代码警告、ImGui 面板、文档共用这一份
- `runtime/core/config/config_layer.h` — `ConfigLayer` 枚举与 `config_layer_name()`
- `runtime/core/debug/debug_switches.h` — `DebugSwitches`：门面（facade），全构建可用，全部转发到 ConfigSystem；渲染代码继续用 `IsRenderFeatureEnabled("sprite")` 等接口
- `shared/config/layered_json.h` — 纯 nlohmann 的分层加载/合并帮助器，runtime 与 editor（Qt 侧）共用

### 开关查询语义

对 `key` 的判定顺序：

1. 显式 token（`--dswitch=xxx` / `--dswitch-off=xxx` / 环境变量）→ 取该 token 的 on/off
2. 命名空间折叠：若存在任一 `ns.*` 的开启 token，则未显式声明的 `ns.*` 子项视为关闭（例如 `-render.base` 之外的 `render.*` 不受影响，但 `-imgui` 只影响 imgui）
3. 注册表默认值：`imgui`、`render.*` 默认开；`server.simulation` 默认关
4. 注册表里不存在的 key：视为开（同时启动时打 WARN 提示 unknown switch）

## 运行期配置入口速查

### app 配置（app_config.json）

字段由 metaparser 反射生成序列化代码，与 `ApplicationSpecification` 的 `META(Enable)` 字段一一对应：

```json
{
    "name": "dodoe-sandbox",
    "app_mode": 1,
    "engine_mode": 0,
    "width": 1600,
    "height": 900,
    "window_resizeable": true,
    "render_settings": {
        "api": 3,
        "pipeline": 3,
        "present_mode": 1,
        "enable_single_thread": false,
        "enable_baseline_renderer": true,
        "windowless": false
    }
}
```

全部 `META(Enable)` 字段（含 `render_settings` 子字段）：

| 字段 | 类型 | 内置默认 | 说明 |
|---|---|---|---|
| `name` | string | "Dodoe Engine" | 窗口/日志名 |
| `app_mode` | enum | Game(0) | Game/Sandbox/Editor/Server |
| `engine_mode` | enum | Full(0) | Full/TwoD/GUI |
| `width` / `height` | u32 | 1920×1080 | 窗口尺寸 |
| `window_resizeable` | bool | true | |
| `render_settings.api` | enum | D3D12(3) | None/OpenGL/Vulkan/D3D12 |
| `render_settings.pipeline` | enum | Deferred(3) | None/Forward/ForwardPlus/Deferred/DeferredPlus/Only2D/OnlyGUI |
| `render_settings.present_mode` | enum | Mailbox(1) | VSync/Mailbox/Immediate |
| `render_settings.enable_single_thread` | bool | false | 渲染单线程 |
| `render_settings.enable_baseline_renderer` | bool | false | Baseline 参考渲染层（Sandbox 的 RenderDebug） |
| `render_settings.windowless` | bool | false | 无窗口渲染；`app_mode=Server` 时运行时强制 true |

引擎内置文件（`engine/res/configs/app_config.json`）实际值见上例；其中 `enable_baseline_renderer` 内置为 `true`，与 Sandbox 代码预设一致（该层只被 SandboxApp 消费）。

枚举取值（json 里写裸数字，反射序列化的限制）：

| 字段 | 枚举 | 取值 |
|---|---|---|
| `app_mode` | `AppMode` | 0=Game, 1=Sandbox, 2=Editor, 3=Server |
| `engine_mode` | `EngineMode` | 0=Full, 1=TwoD, 2=GUI |
| `render_settings.api` | `RenderBackendApiType` | 0=None, 1=OpenGL, 2=Vulkan, 3=D3D12 |
| `render_settings.pipeline` | `RenderingPipelineType` | 0=None, 1=Forward, 2=ForwardPlus, 3=Deferred, 4=DeferredPlus, 5=Only2D, 6=OnlyGUI |
| `render_settings.present_mode` | `PresentMode` | 0=VSync, 1=Mailbox, 2=Immediate |

### 功能开关（全构建生效）

开关在 Debug/Release/Shipping 下语义一致：显式传参即生效，默认值由注册表决定。

环境变量（逗号分隔，后者覆盖前者）：

```powershell
$env:DODOE_DEBUG_SWITCHES = "imgui,server.simulation"
$env:DODOE_DEBUG_SWITCHES_OFF = "render.gizmo"
```

命令行：

```text
--dswitch=imgui,render.base      # 开
--dswitch imgui                  # 开（空格分隔写法）
--dswitch-off=render.gizmo       # 关
```

合法 token 一览（`ConfigSwitchRegistry`）：

| key | 默认 | 说明 |
|---|---|---|
| `imgui` | 开 | ImGui 调试界面 |
| `server.simulation` | 关 | 服务器端同步模拟 |
| `render.base` | 开 | Base pass |
| `render.lighting` | 开 | Lighting pass |
| `render.postprocess` | 开 | Post process pass |
| `render.sprite` | 开 | Sprite pass |
| `render.ui` | 开 | UI pass |
| `render.postprocess2d` | 开 | 2D post process pass |
| `render.gizmo` | 开 | Gizmo pass |
| `render.present` | 开 | Present pass |

遗留环境变量 `DODOE_RENDER_FEATURES` 已移除，等价写法为 `DODOE_RENDER_FEATURES=imgui,foo` → `--dswitch=imgui,render.foo`。

### 编辑器配置（editor.json 等）

Cakery 编辑器使用同一套分层语义（builtin → project → user 逐层 merge_patch），文件包括 `editor.json`、`menus.json`、`panels.json`、`inspectors.json`：

- builtin：`engine/res/editor/config/`
- project：`<项目>/ProjectSettings/Editor/`
- user：用户目录（如主题偏好持久化）

加载入口 `cakery::EditorConfig`，合并逻辑与 runtime 共用 `shared/config/layered_json.h`。

### 其他

| 文件 | 用途 |
|---|---|
| `project_manager_config.json` | 项目管理器列表（QStandardPaths AppData），启动器状态，非引擎运行配置 |
| `DodoeProfile*.json` | Tracy/Chrome trace 采样输出，不是配置 |
| `imgui.ini` | ImGui 布局持久化，不是引擎配置 |
| `server_config.json` | 已删除（历史上无任何代码引用；server 配置直接用 `app_mode: 3` 的 app_config.json） |

## 编译期配置（CMake）

编译期决定"能力是否存在"，不参与运行期优先级链；运行期开关只能在编译期已启用的能力范围内调整。

| CMake 选项 | 默认 | 生成的宏 | 说明 |
|---|---|---|---|
| `DODOE_EDITOR` | ON | `DODOE_EDITOR_ENABLED` | 构建 Cakery 编辑器 |
| `DODOE_SHIPPING` | OFF | `DODOE_SHIPPING=1` | 发布构建（去掉 smoke test 等） |
| `DODOE_PYTHON_ENABLED` | OFF | `DODOE_PYTHON_ENABLED` | Python 脚本（pybind11） |
| `DODOE_ENABLE_PROFILING` | ON | `DODOE_PROFILE_ENABLED=1` | Chrome trace |
| `DODOE_ENABLE_PERF` | OFF | `DODOE_PERF_ENABLED=1` | 非 Debug 配置下的渲染性能计数 |
| （CMake 配置类型=Debug） | — | `DODOE_DEBUG_ENABLED` / `DODOE_IMGUI_ENABLED` | Debug 构建自动定义；门控 ImGui 面板、内存诊断等调试基础设施（功能开关本身不受此限制，全构建生效） |
| （Debug + Tracy 可用） | — | `DODOE_TRACY_ENABLED=1` | 仅 Debug 配置链 Tracy |

构建变体通过 `CMakePresets.json` 组合：`{msvc,ninja} × {editor,sandbox} × {debug,release}`，外加 `msvc-sandbox-shipping`。

## 常用操作

- 指定配置启动：`DodoeSandbox.exe --config=path/to/app_config.json`
- 临时关掉某个 pass：`--dswitch-off=render.lighting`
- 查看当前开关状态：启动日志 `ConfigSystem: switches [...]`，或 ImGui Debugger 面板的 Launch Switches 区（列出全部注册表 token 的实时状态）
- 新增一个开关：在 `ConfigSwitchRegistry` 加一条（key/描述/默认值），查询端用 `DebugSwitches::IsEnabled("<key>")`，UI 与警告自动跟随
