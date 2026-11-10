# Cakery 编辑器 MVC/MVVM 架构

本文定义 Cakery 编辑器 UI 的目标架构。它不替代 [cakery-qt-architecture.md](cakery-qt-architecture.md) 的 Qt 入门说明，而是回答代码增长后“状态放哪、逻辑放哪、谁来通知谁”的问题。

## 一、目标

采用 **MVC 为骨架、面板级 MVVM、应用服务层承接 Controller 职责** 的结构：

- **Model**：文档、选择、历史、项目与资产等纯数据与规则。
- **View**：Qt 控件，只负责显示和收集输入。
- **Controller**：按用例拆分的应用服务，不再是单个全能对象。
- **ViewModel**：每个面板一份展示状态与意图接口，View 只和 ViewModel 交互。
- **Application Bus**：命令总线负责执行与撤销，事件总线负责跨模块通知。

核心原则：

1. Model 是唯一事实来源，View 不保存权威状态。
2. 单向数据流：View → ViewModel → Controller → Model → Event → ViewModel → View。
3. View 不直接访问文件系统、Backend、全局单例。
4. Controller 按用例拆分，避免再造 God Object。
5. 高频视口输入保留直通后端的快速路径。

## 二、现状问题

| 问题 | 位置 |
|---|---|
| View 承担业务逻辑 | `ProjectPanel` 做文件系统操作；`InspectorPanel` 直接拼命令 |
| 全能 Facade | `EditorSession` 同时管文档、选择、历史、后端、Play、Benchmark、Tilemap |
| 全能窗口 | `EditorWindow` 同时管布局、菜单、工具栏、定时器、订阅、Play 控制 |
| 通知混用 | `cakery::Signal` 与 Qt signal 混用，面板直接订阅 Model |
| 跨面板耦合 | `ProjectPanel::assetSelected` → `EditorWindow` → `InspectorPanel` |
| 全局单例 | `EditorConfig`、`CommandRegistry`、`McpServer`、`ExecutionRecorder` |
| 选择双链 | 实体选择走 `EditorSelection`，资产选择另走 Qt 信号 |

## 三、分层

```text
┌───────────────────────────────────────────────────────────┐
│ Shell / Composition Root                                   │
│ EditorApplication（启动、依赖装配、生命周期）               │
│ EditorShell（工作区生命周期、窗口切换）                     │
│ EditorWindow（Dock、菜单、工具栏布局与绑定）                │
└───────────────────────┬───────────────────────────────────┘
                        │ bind
┌───────────────────────▼───────────────────────────────────┐
│ Presentation：View + ViewModel                             │
│ HierarchyView/VM  InspectorView/VM  ProjectView/VM          │
│ ConsoleView/VM    SceneSurface/VM   ShellView/VM            │
└───────────────────────┬───────────────────────────────────┘
                        │ intent
┌───────────────────────▼───────────────────────────────────┐
│ Application：Controller + Bus                              │
│ DocumentController  SelectionController  AssetController    │
│ PlayModeController  ProjectController                       │
│ EditorCommandBus     DomainEventBus                         │
└───────────────────────┬───────────────────────────────────┘
                        │ read/write
┌───────────────────────▼───────────────────────────────────┐
│ Domain Model                                               │
│ EditorDocumentModel  EditorSelection  EditorHistory         │
│ EditHistory  Commands  Project/Asset 领域对象               │
└───────────────────────┬───────────────────────────────────┘
                        │ ports
┌───────────────────────▼───────────────────────────────────┐
│ Infrastructure / Adapters                                  │
│ RuntimeBackend  AssetDatabase  EditorConfig                 │
│ EditorResourceLocator  McpServer  FileSystem                │
└───────────────────────────────────────────────────────────┘
```

## 四、各层职责

### 4.1 Domain Model

保持不依赖 Qt、不直接刷新 UI：

- `EditorDocumentModel`：场景文档、实体与组件、dirty 状态。
- `EditorSelection`：实体/资产/无选择三种目标。
- `EditorHistory` / `EditHistory`：撤销、重做、checkpoint。
- `ProjectDescriptor` 与资产目录模型。

所有修改通过命令进入，Model 只负责保存状态和维护不变量。

### 4.2 Application

| 组件 | 职责 |
|---|---|
| `DocumentController` | 打开/保存/关闭文档、新建场景、实体与组件修改 |
| `SelectionController` | 统一实体/资产/无选择 |
| `AssetController` | 导入、重命名、删除、重导入、扫描、缩略图 |
| `PlayModeController` | Play/Pause/Stop、Play 快照 |
| `ProjectController` | 项目打开/关闭与切换 |
| `EditorCommandBus` | 命令分发、参数校验、错误结果、Undo 组合 |
| `DomainEventBus` | 发布领域事件，驱动 ViewModel 刷新 |

统一事件：

```text
DocumentChangedEvent
SelectionChangedEvent
PlayStateChangedEvent
AssetCatalogChangedEvent
ProjectOpenedEvent / ProjectClosedEvent
NotificationEvent
```

`EditorSession` 在迁移期保留为兼容 Facade，内部逐步委托给上述控制器。

### 4.3 Presentation

每个面板拆成 `XxxView`（现有 `*Panel`）与 `XxxViewModel`：

| 现有类 | 目标 |
|---|---|
| `HierarchyPanel` | `HierarchyPanel` + `HierarchyViewModel` |
| `InspectorPanel` | `InspectorPanel` + `InspectorViewModel` |
| `ProjectPanel` | `ProjectPanel` + `ProjectViewModel` + `AssetController` |
| `ConsolePanel` | `ConsolePanel` + `ConsoleViewModel` |
| `EditorWindow` | `EditorWindow`(布局) + `ShellViewModel` + `Menu/Toolbar ViewModel` |
| `SceneSurface` | 保留视口快速路径，只把相机/模式等粗粒度状态放进 VM |

ViewModel：

- 持有面板展示状态：过滤词、展开状态、缓存索引。
- 暴露意图：`CreateEntity()`、`SetFilter()`、`ImportAssets()`、`CommitField()`。
- 通过 `cakery::Signal` 通知 View，不依赖 QWidget。
- 可以单元测试。

View：

- 创建控件、布局、转发输入。
- 订阅 ViewModel 信号刷新。
- 不直接调用 Session、Backend、文件系统。

### 4.4 Shell

- `EditorApplication`：Composition Root，只做启动、装配、生命周期。
- `EditorShell`：管理“项目选择 → 编辑器窗口 → 返回项目选择”。
- `EditorWindow`：只做 Dock、菜单、工具栏、窗口行为。
- `ProjectSelectionController`：项目选择子控制器。

`EditorWindow` 中需要拆出的部分：

```text
EditorWindow
├─ ShellViewModel
├─ MenuBarViewModel / MenuBuilder
├─ ToolbarViewModel
├─ LayoutController（Dock 布局保存与恢复）
├─ PlayModeController（Play 状态）
└─ ThemeService（注入，不再 qobject_cast<EditorApplication>）
```

## 五、关键契约

### 5.1 EditorContext

应把当前 `EditorWorkspaceContext` 升级为显式字段的上下文，而不是通用服务定位器：

```cpp
class EditorContext {
public:
    DocumentController& documents();
    SelectionController& selection();
    AssetController& assets();
    PlayModeController& play();
    DomainEventBus& events();
    EditorConfig& config();
};
```

ViewModel 通过构造函数注入所需控制器。

### 5.2 ViewModel 基类

```cpp
class ViewModel {
public:
    virtual ~ViewModel() = default;
    Signal<> changed;
protected:
    void notifyChanged() { changed.fire(); }
};
```

### 5.3 错误模型

Controller 返回结构化结果：

```cpp
struct Result {
    bool ok;
    String message;
    String detail;
};
```

失败时发送 `NotificationEvent`，由 Console 展示。跨层调用不再忽略返回值。

## 六、数据流

### 6.1 Hierarchy 选择实体

```text
HierarchyView 点击
  -> HierarchyViewModel.select(uuids)
  -> SelectionController.setEntities(uuids)
  -> EditorSelection 更新
  -> DomainEventBus.SelectionChanged
  -> HierarchyViewModel / InspectorViewModel / SceneViewModel 刷新
```

### 6.2 Inspector 修改字段

```text
InspectorView 控件变化
  -> InspectorViewModel.commitField(...)
  -> DocumentController.updateComponent(...)
  -> EditorCommandBus 执行 Command
  -> EditorDocumentModel 变更
  -> DomainEventBus.DocumentChanged
  -> InspectorViewModel 重新读取快照
  -> RuntimeBackend 同步 document_changed
```

### 6.3 Project 导入资产

```text
ProjectView 拖入文件
  -> ProjectViewModel.importAssets(paths)
  -> AssetController 启动异步任务
  -> AssetCatalogChangedEvent
  -> ProjectViewModel 增量刷新
  -> ConsoleViewModel 显示进度/错误
```

## 七、目录规划

增量迁移，避免一次性搬目录：

```text
engine/src/editor/
  app/
    EditorApplication.*        # Composition Root
    EditorShell.*              # 工作区生命周期
    project_selection/
  presentation/                # ViewModel 层，不依赖 QWidget
    ViewModel.h
    hierarchy/HierarchyViewModel.*
    inspector/InspectorViewModel.*
    project/ProjectViewModel.*
    shell/ShellViewModel.*
  core/
    domain/                    # 文档、选择、历史、项目、资产领域
    application/               # Controller、CommandBus、EventBus
  ui/
    common/                    # EditorWorkspaceContext、Qt 绑定工具
    shell/                     # EditorWindow + ShellViewModel
    panels/                    # View 层
      HierarchyPanel.*
      InspectorPanel.*
      ...
  services/                    # Config、ResourceLocator
  backend/                     # RuntimeBackend 适配器
  infrastructure/              # MCP、文件系统、异步任务
```

ViewModel 统一放在 `presentation/`，按面板分子目录；View 继续放在 `ui/panels/`。ViewModel 不包含任何 QWidget 头文件，也不依赖 `ui/`；它只依赖 Domain、`core/application` 与 `EditorSession`。

迁移期间允许新文件先和旧面板放在同一目录，稳定后再移动。

## 八、生命周期与线程

- 服务与 Controller 的生命周期长于所有 ViewModel/View。
- 迁移期允许 View 持有自己的 ViewModel，稳定后由 Shell 统一创建。
- 销毁顺序：View → ViewModel → Controller → Session → Backend。
- 资产扫描、缩略图、导入放到后台线程，结果通过主线程事件回到 ViewModel。
- 视口鼠标、相机、Gizmo 保留直接快速路径，不逐帧经过事件总线。

## 九、迁移阶段

| 阶段 | 内容 | 验收 |
|---|---|---|
| Phase 0 | 建立 `ViewModel`、`EditorContext`、`EditorCommandBus`、`DomainEventBus` | 行为不变 |
| Phase 1 | 拆分 `HierarchyViewModel`，View 不再直接调用 Session | Hierarchy 选择链唯一 |
| Phase 2 | 拆分 `InspectorViewModel` 与字段描述 | Inspector 不直接拼命令 |
| Phase 3 | 引入 `AssetController`，Project 业务下沉 | ProjectPanel 无文件写操作 |
| Phase 4 | 拆分 Shell、菜单、工具栏、布局、主题 | EditorWindow 只剩布局与绑定 |
| Phase 5 | 按用例拆分 `EditorSession` | 单控制器可独立测试 |
| Phase 6 | 清理全局单例，统一错误模型 | UI 无直接单例访问 |

当前进度：

- Phase 0：`presentation/ViewModel.h` 与 `core/application/SelectionController.h` 已落地。
- Phase 1：`HierarchyViewModel`、`HierarchyPanel` 已解耦。
- Phase 2：`InspectorViewModel`、`InspectorPanel` 已解耦。
- Phase 3：`AssetController`、`ProjectViewModel`、`ProjectPanel` 已解耦。
- Phase 4：`ShellViewModel`、`EditorLayoutController`、`ThemeController` 已落地；`EditorWindow` 的 Play/Undo/相机/Gizmo/标题/资产进度全部走 ViewModel，主题不再依赖 `qobject_cast<EditorApplication*>`。菜单与工具栏仍由 `EditorWindow` 构建，但只绑定 ViewModel 状态与意图。
- Phase 5：新增 `DocumentController`、`PlayModeController`、`EditorViewController`、`ToolActionController`、`ScriptWindowController`、`RuntimeController`；`HierarchyViewModel`、`InspectorViewModel`、`ProjectViewModel`、`ShellViewModel` 均已不再依赖 `EditorSession`。`PlayState` 已抽到 `core/PlayState.h`。
- Phase 6（进行中）：`McpServer`、`ExecutionRecorder`、`AICommandBridge`、`WorkflowLibrary`、`CommandRegistry` 已从单例改为 `EditorSession` 拥有的实例；`FieldAttributeRegistry` 改为 `RuntimeBackend` 拥有的实例。命令、MCP、Workflow、测试均通过 `EditorSession` 访问。剩余单例：`EditorConfig`；错误模型尚未统一。

ViewModel 位于 `engine/src/editor/presentation/`，不再放在 `ui/common/`。后续按上表继续推进。

## 十、相关文档

- [cakery-qt-architecture.md](cakery-qt-architecture.md)：Qt 外壳、对象生命周期、事件流、排查方法。
- [editor.md](editor.md)：编辑器模块总览。
- [ai-command-mcp.md](ai-command-mcp.md)：命令系统与 MCP Server。
- [qt-basics.md](qt-basics.md)：Qt 基础概念。
