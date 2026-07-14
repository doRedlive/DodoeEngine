# Cakery 编辑器架构文档

## 概述

Cakery 是 DoDoE 引擎的 Qt6 编辑器，借鉴 Unity 编辑器的交互模式，采用数据驱动配置 + 反射驱动的 Inspector 系统。

**技术栈**: C++20 · Qt6 Widgets · qt-ADS (高级停靠系统) · nlohmann/json · EnTT · DX12

> **⚠️ 规则：对编辑器架构的改动（新增面板类型、修改 Framework 接口、改变配置 schema），必须同步更新本文档。**

---

## 目录结构

```
engine/
├── src/editor/
│   ├── CMakeLists.txt                  # 三个构建目标：EditorFramework / EditorProperty / Cakery
│   ├── Cakery/
│   │   ├── main.cpp                    # 入口，安装 Qt 日志桥接
│   │   └── app/
│   │       ├── EditorApplication.cpp/.h # QApplication 子类，管理窗口生命周期
│   │       ├── EditorWindow.cpp/.h      # 主编室窗口：菜单/工具栏/停靠/帧循环
│   │       ├── LayoutManager.cpp/.h     # 布局管理：JSON 预设 + Qt-ADS perspective 存取
│   │       └── PanelRegistry.cpp/.h     # 面板工厂注册表，JSON 按名引用
│   │   ├── panels/
│   │   │   ├── Panel.h                 # 面板基类
│   │   │   ├── ScenePanel.h/.cpp       # 3D 视口
│   │   │   ├── GamePanel.h/.cpp        # 运行时 Game 视口
│   │   │   ├── HierarchyPanel.h/.cpp   # 实体层级树
│   │   │   ├── InspectorPanel.h/.cpp   # 属性检视器（反射驱动）
│   │   │   ├── ConsolePanel.h/.cpp     # 命令行控制台
│   │   │   ├── ProjectPanel.h/.cpp     # 项目资产浏览器
│   │   │   ├── TerminalPanel.h/.cpp    # AI/终端面板
│   │   │   └── TilePalettePanel.h/.cpp # Tilemap 调色板
│   │   ├── project/                    # 项目管理器窗口
│   │   ├── widgets/                    # 可复用控件（DragSpinBox 等）
│   │   └── resources/style.qss        # 应用级 QSS 样式
│   │
│   ├── framework/                      # 编辑器核心（零 Qt 依赖，或最小 Qt 依赖）
│   │   ├── EditorContext.cpp/.h        # 编辑器中枢：持有所有服务、管理启动/关闭
│   │   ├── core/
│   │   │   ├── Signal.h               # 类型安全的事件信号（观察者模式）
│   │   │   └── UuidResolve.h          # UUID → Entity 解析辅助
│   │   ├── command/
│   │   │   ├── ICommand.h             # 命令基类（execute/undo/redo）
│   │   │   ├── CommandStack.h/.cpp     # 无界 Undo/Redo 栈
│   │   │   └── commands/              # 具体命令：CreateEntity / DeleteEntity / RenameEntity /
│   │   │                              #   AddComponent / RemoveComponent / SetFieldValue /
│   │   │                              #   ReparentEntity / PaintTiles
│   │   ├── selection/
│   │   │   └── SelectionManager.h      # 实体选中状态 + changed 信号
│   │   ├── document/
│   │   │   └── SceneDocument.h/.cpp    # 场景文件的新建/打开/保存/脏标记
│   │   ├── camera/EditorCamera.h/.cpp  # 编辑器自由相机
│   │   ├── gizmo/GizmoService.h/.cpp   # 位移/旋转/缩放 Gizmo
│   │   ├── picking/PickingService.h/.cpp # 鼠标拾取（射线→实体）
│   │   ├── playmode/PlayModeController.h/.cpp # 运行/暂停/停止状态机
│   │   ├── event/EventBridge.h/.cpp    # 编辑器事件桥接（实体增删、组件变更等）
│   │   ├── viewport/ViewportService.h/.cpp # 多视口管理
│   │   ├── history/EditHistory.h/.cpp  # 编辑历史
│   │   ├── asset/AssetDatabase.h/.cpp  # 资产索引
│   │   ├── tilemap/TilePaintService.h/.cpp # Tilemap 绘制
│   │   ├── console/
│   │   │   ├── CommandRegistry.h/.cpp  # 文本命令注册与执行
│   │   │   ├── builtin_commands.cpp    # 内置命令注册
│   │   │   └── AICommandBridge.h/.cpp  # AI 命令桥接
│   │   └── config/
│   │       └── EditorConfig.h/.cpp     # 三级 JSON 配置加载器
│   │
│   └── property/                       # 属性绘制系统
│       ├── PropertyDrawer.h            # PropertyDrawer 基类 + PropertyDrawerRegistry
│       ├── PropertyDrawerRegistry.cpp  # 注册表实现
│       ├── CustomEditorRegistry.h/.cpp # 组件级自定义编辑器注册表
│       └── drawers/
│           ├── ScalarDrawer.h/.cpp     # float/int/bool
│           ├── StringDrawer.h/.cpp     # std::string
│           ├── VectorDrawer.h/.cpp     # Vector2f/i, Vector3f/i, Vector4f/i
│           ├── ColorDrawer.h/.cpp      # Color
│           ├── EnumDrawer.h/.cpp       # 枚举（反射驱动）
│           ├── PPtrDrawer.h/.cpp       # 资产指针（支持拖放）
│           └── CompositeDrawer.h/.cpp  # 复合类型（递归子字段）
│
├── res/editor/                         # 内置编辑器配置（进入版本管理）
│   ├── editor.json                     # 全局：主题、默认布局预设、快捷键
│   ├── menus.json                      # 菜单栏 + 工具栏动作表
│   ├── panels.json                     # 面板工厂清单 + 图标
│   ├── inspectors.json                 # 字段级属性（Range/Hidden/Tooltip）
│   ├── layouts/
│   │   └── default.layout.json         # 默认停靠布局
│   └── themes/
│       └── unity-dark.qss              # Qt-ADS 深色主题
│
└── <工程目录>/Configs/editor/           # 项目级覆盖（可选，随工程走）
```

---

## 应用生命周期

```
main()
  │
  ├─ EditorApplication 构造
  │   ├─ EditorConfig::self().load("engine/res/editor")    ← 加载 JSON 配置
  │   ├─ 加载 QSS 样式表
  │   └─ new EditorContext()                               ← 构造所有 Framework 服务
  │
  ├─ EditorApplication::run()
  │   ├─ 显示 ProjectManagerWindow（选择/新建工程）
  │   └─ QApplication::exec() 进入事件循环
  │
  └─ 用户选择工程后:
      ├─ new EditorWindow(*m_ctx)
      ├─ EditorWindow::enterWorkspace(projectPath)
      │   └─ m_ctx.boot(cfg)                               ← 启动引擎运行时
      │       ├─ TaskScheduler::Self()
      │       ├─ new Application(DX12 / Deferred / DualThread)
      │       ├─ Project::Load()
      │       ├─ initializeModules() → startRuntime()
      │       ├─ RegisterBuiltinCommands()
      │       └─ PropertyDrawerRegistry::registerBuiltinDrawers()
      │
      └─ EditorWindow 构造
          ├─ PanelRegistry::registerBuiltinPanels()
          ├─ setupDockWidgets()  → m_layoutManager->applyDefault()  ← JSON 布局驱动
          ├─ setupMenuBar()      → 读取 menus.json 构建菜单
          ├─ setupToolBar()      → 读取 menus.json 构建工具栏
          ├─ setupStatusBar()
          ├─ connectActions()    → 信号连接（文档脏标记、PlayMode 状态同步等）
          └─ setupFrameTimer()   → ~0ms QTimer：每帧 tick + camera.update + gizmos + render
```

---

## 核心架构

### EditorContext —— 编辑器中控

`EditorContext` 不继承任何 Qt 类，是整个编辑器的依赖注入容器。所有服务通过它互相访问。

```
EditorContext
├─ Application (dodoe 引擎运行时)
│   └─ SystemContext → World → Scene → Entities
├─ CommandStack          # Undo/Redo 命令栈
├─ SelectionManager      # 实体选中状态
├─ SceneDocument         # 场景文件 I/O + 脏标记
├─ EditorCamera          # 自由视角相机
├─ GizmoService          # 变换 Gizmo（位移/旋转/缩放）
├─ PickingService        # 鼠标射线拾取
├─ PlayModeController    # Play/Pause/Stop 状态机
├─ EventBridge           # 编辑器事件（实体增删、组件变更信号）
├─ ViewportService       # 多视口渲染管理
├─ EditHistory           # 编辑历史记录
├─ AICommandBridge       # AI 自然语言→命令
├─ AssetDatabase         # 项目资产索引
├─ TilePaintService      # Tilemap 绘制
└─ (boot 时自动调用 RegisterBuiltinCommands / registerBuiltinDrawers)
```

### Panel 基类

```cpp
class Panel : public QWidget {
    Q_OBJECT
protected:
    EditorContext& m_ctx;                              // 面板持有上下文引用
    std::vector<ScopedConnection> m_connections;       // RAII 信号断开
};
```

要点：
- 所有面板继承 `Panel`，通过 `m_ctx` 访问编辑器的所有服务
- 用 `m_ctx.selection().changed.connect(...)` 订阅选中变化，返回值存入 `m_connections` 自动管理生命周期
- `ScopedConnection` 析构时自动 `disconnect`，面板销毁时信号自动清理

---

## 面板系统

### PanelRegistry

```cpp
// 注册（在 registerBuiltinPanels 中集中完成）
PanelRegistry::self().registerPanel("Scene", [](EditorContext& ctx, QWidget* parent) -> Panel* {
    return new ScenePanel(ctx, parent);
});

// 创建（LayoutManager 按 JSON 布局调用）
Panel* panel = PanelRegistry::self().create("Scene", ctx, nullptr);
```

### 添加新面板（完整步骤）

假设要添加一个 **StatsPanel**（显示场景统计信息）。

**步骤 1**: 创建头文件 `Cakery/panels/StatsPanel.h`

```cpp
// do@Redlive
#pragma once
#include "Panel.h"
#include <QLabel>

namespace cakery {

class StatsPanel : public Panel {
    Q_OBJECT
public:
    explicit StatsPanel(EditorContext& ctx, QWidget* parent = nullptr);

    void refresh();

private:
    QLabel* m_entityLabel  = nullptr;
    QLabel* m_componentLabel = nullptr;
};

} // namespace cakery
```

**步骤 2**: 创建实现文件 `Cakery/panels/StatsPanel.cpp`

```cpp
// do@Redlive
#include "StatsPanel.h"
#include "framework/EditorContext.h"
#include "framework/selection/SelectionManager.h"
#include "runtime/function/world/scene.h"

#include <QVBoxLayout>

namespace cakery {

StatsPanel::StatsPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    m_entityLabel = new QLabel("Entities: 0", this);
    m_componentLabel = new QLabel("Components: 0", this);
    layout->addWidget(m_entityLabel);
    layout->addWidget(m_componentLabel);
    layout->addStretch();
}

void StatsPanel::refresh()
{
    auto* scene = m_ctx.activeScene();
    if (!scene) return;
    auto entities = scene->getEntities();
    m_entityLabel->setText(QString("Entities: %1").arg(entities.size()));
}

} // namespace cakery
```

**步骤 3**: 在 `PanelRegistry.cpp` 注册工厂

```cpp
#include "Cakery/panels/StatsPanel.h"

void PanelRegistry::registerBuiltinPanels()
{
    // ... 现有注册 ...
    registerPanel("Stats", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new StatsPanel(ctx, parent);
    });
}
```

**步骤 4**: 在 `engine/res/editor/layouts/default.layout.json` 添加停靠位

```json
{ "id": "Stats", "factory": "Stats", "area": "Right", "tabWith": "Inspector" }
```

**步骤 5**: 在 `CMakeLists.txt` 的 `CAKERY_SOURCES` 和 `CAKERY_HEADERS` 中添加文件

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/Cakery/panels/StatsPanel.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Cakery/panels/StatsPanel.h
```

完成。新面板会随编辑器启动自动创建并停靠在 JSON 指定的位置。

### 面板间通信

面板之间**不直接引用**。通信通过以下方式：

| 方式 | 示例 |
|------|------|
| `EditorContext` 服务 | `m_ctx.selection().primary()` — 任意面板获取当前选中 |
| `Signal` 订阅 | `m_ctx.selection().changed.connect([this](auto& sel) { ... })` — 选中变化时响应 |
| `EventBridge` 事件 | `m_ctx.events().entityCreated.connect(...)` — 实体创建时刷新 |
| `CommandStack` | `m_ctx.commands().execute(cmd)` — 所有修改走命令系统，天然支持 Undo |

---

## 布局系统

### 两级存储

| 层级 | 格式 | 位置 | 用途 |
|------|------|------|------|
| 预设（Preset） | JSON | `engine/res/editor/layouts/*.layout.json` | 团队共享、可 diff、进版本管理 |
| 会话（Session） | Qt-ADS 字节流 | `%APPDATA%/Cakery/<project>/session.json` | 用户拖拽后的私有布局，不污染团队 |

### 布局 JSON Schema

```json
{
  "name": "Default",
  "panels": [
    { "id": "Scene",      "factory": "Scene",      "area": "Center", "central": true },
    { "id": "Game",       "factory": "Game",       "area": "Center", "tabWith": "Scene" },
    { "id": "Hierarchy",  "factory": "Hierarchy",  "area": "Left" },
    { "id": "Inspector",  "factory": "Inspector",  "area": "Right" },
    { "id": "Project",    "factory": "Project",    "area": "Bottom", "relativeTo": "Hierarchy" },
    { "id": "Console",    "factory": "Console",    "area": "Bottom", "relativeTo": "Inspector" }
  ],
  "sizes": { "left": 300, "right": 360, "bottom": 260 }
}
```

| 字段 | 说明 |
|------|------|
| `id` | 停靠窗口唯一名（objectName），Qt-ADS 状态还原依赖它 |
| `factory` | `PanelRegistry` 中的注册名 |
| `area` | `Center` / `Left` / `Right` / `Top` / `Bottom` |
| `central` | `true` 则作为 `CDockManager::setCentralWidget`（仅一个） |
| `tabWith` | 与指定面板同区域 Tab 并列 |
| `relativeTo` | 锚定面板 id（`addDockWidget` 的 `force_area` 参数） |

### 用户拖拽后

- 用户拖拽面板 → Qt-ADS 自动调整
- `LayoutManager::saveSession()` 将 `CDockManager::saveState()` 写入 `session.json`
- 下次启动 `LayoutManager::restoreSession()` 先 `applyDefault()` 建所有面板，再 `restoreState()` 还原用户布局
- 还原失败则 fallback `applyDefault()`

---

## 菜单 & 工具栏

### JSON 驱动

菜单和工具栏完全由 `engine/res/editor/menus.json` 定义，修改即时生效，无需重编译。

```json
{
  "menus": [
    { "path": "File/New Scene",        "command": "scene.new",   "shortcut": "Ctrl+N" },
    { "path": "File/Save",             "command": "scene.save",  "shortcut": "Ctrl+S" },
    { "path": "GameObject/3D Object/Cube",  "command": "entity.create", "args": { "preset": "Cube" } },
    { "path": "Component/Physics/Rigidbody","command": "component.add", "args": { "component": "RigidbodyComponent" } }
  ],
  "toolbar": [
    { "id": "Play",  "command": "playmode.play",  "checkable": true, "group": "play" },
    { "id": "Pause", "command": "playmode.pause", "checkable": true, "group": "play" },
    { "id": "Move",  "command": "gizmo.setmode", "args": { "mode": "Translate" }, "shortcut": "W", "checkable": true, "group": "gizmo" }
  ]
}
```

| 字段 | 说明 |
|------|------|
| `path` | `/` 分割的层级路径，自动创建子菜单。如 `GameObject/3D Object/Cube` 创建 GameObject→3D Object→Cube |
| `command` | `CommandRegistry` 中注册的命令名 |
| `args` | 传给命令的 JSON 参数 |
| `shortcut` | Qt 快捷键字符串 |
| `group` | 工具栏按钮分组，同组自动互斥（如 gizmo 工具组） |
| `checkable` | 按钮是否可选中 |

### 添加新菜单项

1. 确保命令已在 `builtin_commands.cpp` 注册（见命令系统章节）
2. 在 `menus.json` 添加条目
3. 重启编辑器生效

---

## 属性系统（Inspector）

### 整体流程

```
选中实体
  → InspectorPanel::rebuildForEntity(uuid)
    → 遍历 ComponentDB::entries()
      → 跳过 IDComponent / TagComponent
      → 对每个实体持有的组件:
        → TypeMeta::newMetaFromName(componentName)  ← 反射元数据
        → 对每个字段 FieldAccessor:
          → PropertyDrawerRegistry::create(field)
            → 查属性注册表 (Range/Color/...) → 属性对应 drawer
            → 查类型注册表 (float/int/Vector3f/...) → 类型对应 drawer
            → 复合类型 → CompositeDrawer（递归子字段）
            → 兜底 → ScalarDrawer（只读文本）
          → drawer->build(pc) → QWidget
```

### PropertyDrawer 基类

```cpp
class PropertyDrawer {
public:
    virtual QWidget* build(const PropertyContext& pc) = 0;   // 首次构建控件
    virtual void updateValue(const PropertyContext& pc) = 0; // 增量刷新值
};

struct PropertyContext {
    EditorContext*         ctx;
    dodoe::Uuid            entity;
    std::string            componentName;
    void*                  componentPtr;
    dodoe::FieldAccessor*  field;
};
```

### 已有 Drawer

| Drawer | 匹配类型 | 说明 |
|--------|---------|------|
| `ScalarDrawer` | float, double, int, bool 等 | 数字输入框 / 复选框 |
| `StringDrawer` | std::string | 文本输入框 |
| `VectorDrawer<N>` | Vector2f/i, Vector3f/i, Vector4f/i | 带 X/Y/Z/W 标签的多轴输入 |
| `ColorDrawer` | Color | 颜色选择器 |
| `EnumDrawer` | 枚举类型 | 反射驱动的下拉框 |
| `PPtrDrawer` | PPtr\<T\> | 资产引用（支持拖放） |
| `CompositeDrawer` | 有反射元数据的复合类型 | 递归展开子字段 |

### 添加新 PropertyDrawer

**场景**: 需要一个角度输入控件（0-360 度带拨盘）。

**步骤 1**: `property/drawers/AngleDrawer.h`

```cpp
// do@Redlive
#pragma once
#include "property/PropertyDrawer.h"

namespace cakery {

class AngleDrawer : public PropertyDrawer {
public:
    QWidget* build(const PropertyContext& pc) override;
    void updateValue(const PropertyContext& pc) override;
};

} // namespace cakery
```

**步骤 2**: `property/drawers/AngleDrawer.cpp` — 实现 build/updateValue，修改时通过 `pc.ctx->commands().execute(std::make_unique<SetFieldValueCommand>(...))` 走命令栈。

**步骤 3**: 在 `PropertyDrawerRegistry::registerBuiltinDrawers()` 中注册

```cpp
// 按类型注册
registerByType("Angle", []() { return std::make_unique<AngleDrawer>(); });

// 或按属性注册（当字段标记了 META(Range=0,360) 时自动匹配）
registerByAttribute("Range", []() { return std::make_unique<AngleDrawer>(); });
```

### 字段属性（fieldAttributes）

在 `inspectors.json` 中可以为任何字段配置属性，无需改 C++ 代码：

```json
{
  "fieldAttributes": {
    "TransformComponent.rotation":  { "Range": [0, 360], "Tooltip": "欧拉角（度）" },
    "TransformComponent.internalId":{ "Hidden": true }
  }
}
```

| 属性 | 效果 |
|------|------|
| `Range: [min, max]` | 数字字段限范围 |
| `Hidden: true` | 字段不在 Inspector 中显示 |
| `Tooltip: "..."` | 鼠标悬停提示 |
| `ReadOnly: true` | 只读显示 |

---

## 命令系统 & Undo/Redo

### 架构

```
用户操作（控件/菜单/快捷键）
  → CommandRegistry::executeStructured(ctx, "command.name", args)
    → CommandSpec.handler(ctx, args)
      → 构造具体 ICommand 子类
        → CommandStack::execute(cmd)
          → cmd->execute(ctx)     ← 真正执行
          → 压入 m_undo 栈
          → 清空 m_redo 栈
          → fire changed 信号
```

### ICommand 接口

```cpp
class ICommand {
public:
    virtual bool execute(EditorContext& ctx) = 0;   // 执行
    virtual void undo(EditorContext& ctx) = 0;      // 撤销
    virtual void redo(EditorContext& ctx) { execute(ctx); }  // 重做（默认=执行）
    virtual std::string label() const = 0;           // 用于 UI 显示
    virtual bool mergeWith(const ICommand& next) { return false; }  // 合并连续同类型命令
};
```

### 已有命令

| 命令 | 说明 |
|------|------|
| `CreateEntityCommand` | 创建实体，undo 删除 |
| `DeleteEntityCommand` | 删除实体，undo 重建 |
| `RenameEntityCommand` | 重命名，undo 恢复旧名 |
| `AddComponentCommand` | 添加组件，undo 移除 |
| `RemoveComponentCommand` | 移除组件，undo 恢复 |
| `SetFieldValueCommand` | 修改字段值，undo 恢复旧值 |
| `ReparentEntityCommand` | 修改父节点，undo 恢复 |
| `PaintTilesCommand` | 绘制 Tile，undo 擦除 |

### 添加新命令

**步骤 1**: 在 `framework/command/commands/` 创建 `MyCommand.h`

```cpp
// do@Redlive
#pragma once
#include "framework/command/ICommand.h"
#include "runtime/core/utils/json.h"

namespace cakery {

class MyCommand : public ICommand {
public:
    MyCommand(/* 参数 */) { /* 保存旧状态 */ }

    bool execute(EditorContext& ctx) override {
        // 执行修改
        return true;
    }

    void undo(EditorContext& ctx) override {
        // 恢复修改前的状态
    }

    std::string label() const override {
        return "My Action";
    }

private:
    // 保存 undo 所需的旧状态
};

} // namespace cakery
```

**步骤 2**: 在 `builtin_commands.cpp` 注册文本命令接口

```cpp
reg.add({"my.command", "Description",
         "my.command <arg>",
         {{"arg", "string", "Argument", true}},
         true,  // mutating = true → 可撤销
         [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
             auto cmd = std::make_unique<MyCommand>(/* args */);
             ctx.commands().execute(std::move(cmd));
             return CommandResult::Ok("Done");
         }});
```

**步骤 3**: 在 `menus.json` 添加菜单项（可选）

```json
{ "path": "Tools/My Action", "command": "my.command", "args": { "arg": "hello" } }
```

---

## 事件系统

### Signal 模板

```cpp
// 声明（发布者）
Signal<dodoe::Uuid> entityCreated;

// 发射
entityCreated.fire(newEntityUuid);

// 订阅（消费者，返回 Handle）
auto h = ctx.events().entityCreated.connect([](dodoe::Uuid uuid) {
    // 处理
});

// 手动断开
ctx.events().entityCreated.disconnect(h);

// 或存入 m_connections 自动管理（Panel 析构时自动断开）
m_connections.emplace_back(ctx.events().entityCreated, h);
```

### 内置事件（EventBridge）

| 信号 | 参数 | 触发时机 |
|------|------|---------|
| `entityCreated` | Uuid | 创建实体后 |
| `entityDestroyed` | Uuid | 删除实体后 |
| `hierarchyChanged` | Uuid | 父子关系变化后 |
| `componentChanged` | Uuid, string | 组件字段值变化后 |

### 内置信号（其他服务）

| 信号 | 位置 | 触发时机 |
|------|------|---------|
| `SelectionManager::changed` | SelectionManager | 选中实体变化 |
| `SceneDocument::dirtyChanged` | SceneDocument | 场景脏标记变化 |
| `SceneDocument::sceneChanged` | SceneDocument | 场景切换 |
| `CommandStack::changed` | CommandStack | Undo/Redo 栈变化 |
| `PlayModeController::stateChanged` | PlayModeController | Play/Pause/Stop 切换 |

---

## 配置系统（EditorConfig）

### 三级合并

```
engine/res/editor/          ← 内置默认（团队共享，进版本管理）
  ↓ merge_patch
<工程>/Configs/editor/      ← 项目级覆盖（可选）
  ↓ merge_patch  
%APPDATA%/Cakery/<project>/ ← 用户偏好（私有，不进版本管理）
```

后者覆盖前者的同名键。数组（如菜单项）直接替换而非追加。

### 配置文件

| 文件 | 用途 |
|------|------|
| `editor.json` | 全局设置（主题、默认布局、快捷键、栅格吸附等） |
| `menus.json` | 菜单栏 + 工具栏 |
| `panels.json` | 面板工厂清单、图标、默认可见性 |
| `inspectors.json` | 字段级属性（Range/Hidden/Tooltip） |
| `layouts/*.layout.json` | 预设布局 |

### API

```cpp
auto& cfg = EditorConfig::self();

cfg.editorJson();           // editor.json 合并结果
cfg.menusJson();            // menus.json 合并结果
cfg.inspectorsJson();       // inspectors.json 合并结果
cfg.layoutJson("default");  // 加载指定预设布局
cfg.themeName();            // "unity-dark"
cfg.defaultLayoutName();    // "default"
cfg.themePath();            // 主题 qss 完整路径
cfg.shortcut("tool.move");  // 获取快捷键
```

---

## 反射系统（与编辑器的关系）

引擎运行时通过 codegen 为每个组件生成反射元数据（`TypeMeta` / `FieldAccessor` / `MethodAccessor`）。

编辑器利用这些元数据：
- **Inspector**: 遍历 `TypeMeta::get_field_list()` → 为每个字段创建 PropertyDrawer
- **序列化**: `TypeMeta::writeByName()` / `newFromNameAndJson()`
- **隐藏字段**: `FieldAccessor::isHidden()` 配合 `inspectors.json`
- **范围限制**: `FieldAccessor::attributeRange()` 配合 `inspectors.json`

组件注册 `addable=false` 意味着它不可通过 "Add Component" 按钮添加（如 `TransformComponent`、`HierarchyComponent` 总是自动存在），但仍会在 Inspector 中显示（`IDComponent` 和 `TagComponent` 除外）。

---

## 构建目标

| 目标 | 类型 | 内容 | 依赖 |
|------|------|------|------|
| `EditorFramework` | STATIC 库 | `framework/*.cpp` | `DodoeRuntime` |
| `EditorProperty` | STATIC 库 | `property/*.cpp` | `EditorFramework` + `Qt6::Widgets` |
| `Cakery` | EXE | Cakery app 源码 | `EditorFramework` + `EditorProperty` + `Qt6::Widgets` + `qtadvanceddocking-qt6` |

编译后 `engine/res/editor/` 被拷贝到 `${BINARY_ROOT}/resources/editor/`。

---

## 关键设计决策

1. **面板不互相引用** — 所有通信通过 `EditorContext` 服务或 `Signal` 订阅，面板可独立开发和测试
2. **所有修改走命令栈** — 控件值变更 → `SetFieldValueCommand` → `CommandStack::execute()`，保证全部操作可撤销
3. **反射优先** — 90% 的组件不需要手写 Inspector 代码，反射自动生成字段控件
4. **JSON 配置可 diff** — 布局预设和菜单是纯 JSON，可进版本管理、做 code review
5. **三级配置分离** — 内置/项目/用户三级，允许项目自定义编辑器行为而不污染引擎内置配置
6. **零 Qt 的 Framework 层** — `EditorContext`、`CommandStack`、`SelectionManager` 不依赖 Qt，可独立测试
7. **Session 失败安全** — Qt-ADS 的 `restoreState` 跨版本可能损坏，失败时自动 fallback 到 JSON 预设
