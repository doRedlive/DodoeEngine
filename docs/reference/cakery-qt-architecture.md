# Cakery Qt 架构说明

本文面向第一次接触 C++、Qt 和 Cakery 的读者。目标不是介绍 Qt 的全部功能，而是回答三个问题：

1. Cakery 启动后，各个对象是谁创建的？
2. 用户在界面上点击、拖拽、修改属性时，数据如何流动？
3. 出现问题时，应该在哪一层查找？

本文按当前源码说明，主要涉及 `engine/src/editor/` 下的 Cakery 编辑器。

## 一、先建立整体印象

Cakery 不是把所有逻辑都写在 Qt 控件里。它大致分为四层：

```text
┌──────────────────────────────────────────────────────────────┐
│ Qt 应用壳                                                    │
│ EditorApplication -> ProjectManagerWindow -> EditorWindow   │
│ QMainWindow / QWidget / Qt event loop / QTimer / QSS         │
└──────────────────────────────┬───────────────────────────────┘
                               │ EditorWorkspaceContext
┌──────────────────────────────▼───────────────────────────────┐
│ 编辑器会话层                                                  │
│ EditorSession                                                 │
│ 文档模型、选择、Undo/Redo、编辑历史、后端事件分发             │
└──────────────────────────────┬───────────────────────────────┘
                               │ IEditorBackend
┌──────────────────────────────▼───────────────────────────────┐
│ 后端适配层                                                    │
│ RuntimeEditorBackend              NullEditorBackend           │
│ 运行时、渲染、相机、点选、Gizmo、资产数据库      仅文档编辑   │
└──────────────────────────────┬───────────────────────────────┘
                               │
┌──────────────────────────────▼───────────────────────────────┐
│ Dodoe Runtime                                                 │
│ Application / World / Scene / RenderSystem / ResourceManager  │
└───────────────────────────────────────────────────────────────┘
```

最重要的设计原则是：

- Qt 控件负责显示和收集输入，不应该直接操作 `World`、`Scene` 或渲染私有对象。
- `EditorSession` 是 Qt 和后端之间的稳定入口。
- `EditorDocumentModel` 是编辑态场景的文档来源。
- `RuntimeEditorBackend` 把文档投影到运行时场景；它不是文档本身。
- `NullEditorBackend` 允许只做文档编辑，因此 UI 可以在没有运行时预览时仍然工作。

## 二、源码目录地图

| 目录 | 作用 | 初学者应先看 |
|---|---|---|
| `engine/src/editor/products/` | 产品进程入口 | `cakery/main.cpp` |
| `engine/src/editor/cakery/app/` | Qt 应用和项目选择窗口 | `EditorApplication.cpp`、`ProjectManagerWindow.cpp` |
| `engine/src/editor/cakery/ui/` | Qt 主窗口和面板 | `EditorWindow.cpp`、各 `*Panel.cpp` |
| `engine/src/editor/core/` | 不依赖具体 Qt 控件的编辑器逻辑 | `EditorSession.cpp`、`EditorDocumentModel.cpp` |
| `engine/src/editor/bridge/` | 前端与后端的接口协议 | `EditorBackend.h` |
| `engine/src/editor/adapters/runtime/` | 连接 Dodoe Runtime 的实现 | `RuntimeEditorBackend.cpp` |
| `engine/src/editor/adapters/null/` | 无 Runtime 的占位实现 | `NullEditorBackend.cpp` |
| `engine/src/editor/services/` | 资源定位和编辑器配置 | `EditorResourceLocator.cpp`、`EditorConfig.cpp` |
| `engine/res/editor/` | 图标、QSS、JSON 配置、翻译 | `config/`、`themes/` |

`engine/src/editor/CMakeLists.txt` 将这些部分组织成四个静态库：`EditorCore`、`EditorServices`、`EditorNullBackend`、`EditorRuntimeBackend`，再由 `EditorUI` 和两个产品入口链接起来。这样做的好处是核心逻辑可以不依赖窗口类，问题是接口边界必须保持清楚。

## 三、Qt 的几个基础概念

### 3.1 `QApplication`：整个 GUI 进程的根

`EditorApplication` 继承 `QApplication`。一个 Qt GUI 进程通常只有一个 `QApplication` 对象，它负责：

- 保存应用级设置，例如应用名、组织名、全局样式；
- 接收操作系统消息；
- 启动事件循环；
- 管理全局事件过滤器和翻译器。

在 `EditorApplication::run()` 中调用 `exec()` 后，Qt 开始不断取出鼠标、键盘、绘制、窗口和定时器事件。只要事件循环在运行，按钮点击和面板刷新才会继续发生。

### 3.2 `QWidget`：所有界面控件的基类

`EditorWindow` 是 `QMainWindow`，各种面板是 `QWidget`。一个控件通常包含：

- 父控件：决定对象树归属和生命周期；
- 布局：决定子控件如何排列；
- 事件处理函数：例如 `mousePressEvent`、`paintEvent`；
- 信号和槽：把“发生了什么”通知给其他对象。

Qt 的父子对象树很重要：用 `new QWidget(parent)` 创建的控件通常由父对象自动释放。因此，Cakery 的 Qt 控件大量使用裸指针并不等于完全没有所有权规则，所有权通常隐藏在父控件中。相反，`EditorSession`、后端和资源定位器使用 `std::unique_ptr`，因为它们不是 Qt 对象。

### 3.3 信号和槽

例如：

```cpp
connect(saveAction, &QAction::triggered, this, [this]() {
    m_context.session().saveDocument(std::string());
});
```

含义是：`saveAction` 发出 `triggered` 信号时，执行 lambda。lambda 捕获 `this`，所以它依赖 `EditorWindow` 仍然存在；把动作作为 `EditorWindow` 的子对象、并把接收者写成 `this`，可以让 Qt 在接收者销毁时自动断开连接。

项目还定义了一个轻量的 `cakery::Signal`。它用于 `EditorDocumentModel`、`EditorSelection` 和 `EditorHistory` 等非 Qt 核心对象。Qt 面板通过 `ScopedConnection` 订阅这些信号。理解这两套机制：

| 机制 | 使用场景 | 特点 |
|---|---|---|
| Qt signal/slot | 按钮、窗口、面板之间 | 由 Qt 元对象系统管理，可跨事件循环排队 |
| `cakery::Signal` | Session、Document、History | 简单、同步调用，不知道 Qt 的线程和生命周期 |

当前 `cakery::Signal::fire()` 是同步调用，因此模型改变后，订阅者会立刻执行。不要在模型信号回调里做耗时工作。

### 3.4 事件过滤器和原生事件

`EditorWindow` 在 `qApp` 上安装全局事件过滤器，用于处理无边框窗口的拖动、双击最大化、浮动 Dock 边缘缩放和消息框标题栏。`nativeEvent()` 进一步处理 Windows 的 `WM_NCHITTEST` 与 `WM_GETMINMAXINFO`。

这类代码处于 Qt 和操作系统之间，通常不是编辑器业务逻辑。出现窗口拖动、边框命中或多屏最大化问题时，优先查看 `EditorWindow::eventFilter()`、`nativeEvent()`，不要先改 `EditorSession`。

### 3.5 `QTimer` 和安全点

`EditorWindow::startSafePointTimer()` 创建约 16ms 的定时器，每次超时调用 `EditorSession::tick()`。这相当于编辑器自己的主循环桥：

```text
Qt event loop
    -> QTimer timeout
        -> EditorSession::tick()
            -> resize pending viewport
            -> RuntimeEditorBackend::tickAtSafePoint()
                -> 输入、事件、相机、Gizmo、Runtime 一帧
```

Qt 的定时器不是实时保证。系统繁忙时，超时会延后，因此运行时逻辑不应假设每次间隔严格等于 16ms。Runtime 后端用实际时间差计算 `dt`，并把单帧最大值限制在 0.05 秒。

### 3.6 QSS 样式

Qt Style Sheet（QSS）类似 CSS。`EditorApplication::applyTheme()` 先加载 `cakery-dark.qss`，再拼接用户选择的主题覆盖文件。控件通过 `setObjectName()` 和动态属性暴露样式钩子，例如 `editorToolbar`、`inspectorSection`、`runtimeToolButton`。

修改界面外观时，先检查控件的 `objectName`，再查 `engine/res/editor/themes/*.qss`。不要在每个控件里散落大量 `setStyleSheet()`，否则主题切换会出现不一致。

## 四、启动和关闭流程

### 4.1 进程入口

`engine/src/editor/products/cakery/main.cpp` 做的事情很少：创建 `EditorApplication`，并注入 `RuntimeEditorBackend`。

预览产品 `cakery_preview/main.cpp` 注入的是 `NullEditorBackend`。这就是依赖注入：Qt 外壳不需要知道当前使用哪一种后端。

### 4.2 `EditorApplication` 构造阶段

`EditorApplication` 的构造顺序可以读成：

1. 初始化 `QApplication`，设置组织名和应用名。
2. 根据环境变量或系统区域加载翻译器。
3. 定位编辑器资源目录和应用图标。
4. 加载内置、项目、用户三层 `EditorConfig`。
5. 应用 QSS 主题。
6. 创建 `EditorSession`，把后端移交给 Session。
7. 创建 `EditorWorkspaceContext`，让 Qt 面板通过统一上下文访问 Session 和资源。

此时还没有显示主编辑窗口。

### 4.3 项目选择和工作区

`run()` 创建 `ProjectManagerWindow` 并显示它。用户选择项目后，`onProjectSelected()`：

1. 重新加载项目级编辑器配置；
2. 创建 `EditorWindow`；
3. 调用 `enterWorkspace(projectPath)`；
4. `EditorSession::openProject()` 调用后端打开项目；
5. 后端报告起始场景时，Session 尝试打开场景文档；
6. 主窗口设置项目路径、设置面板、控制台和资源面板；
7. 关闭项目选择窗口，显示编辑器窗口。

### 4.4 关闭顺序

主窗口的 `closeEvent()` 负责：

1. 检查文档 dirty 状态；
2. 让用户保存、放弃或取消；
3. 保存 Dock 布局；
4. 停止安全点定时器；
5. 调用 `EditorSession::shutdown()`。

`EditorApplication` 析构时再释放窗口、workspace、session 和资源定位器。顺序很关键：面板回调可能仍然引用 Session，所以应先停止窗口活动，再释放 Session。

## 五、对象关系和所有权

```text
EditorApplication (QApplication)
├─ ProjectManagerWindow (QDialog, Qt parent/lifetime)
├─ EditorWindow (QMainWindow, manually held pointer)
│  ├─ CDockManager
│  ├─ SceneSurface
│  ├─ HierarchyPanel
│  ├─ InspectorPanel
│  ├─ ProjectPanel
│  ├─ ConsolePanel
│  └─ Settings/Tile panels
├─ unique_ptr<EditorSession>
│  └─ unique_ptr<IEditorBackend>
├─ unique_ptr<EditorResourceLocator>
└─ unique_ptr<EditorWorkspaceContext>
```

`EditorWorkspaceContext` 本身只是引用包装器：它不拥有 Session 或资源定位器。它把面板需要的访问入口集中起来，避免每个面板都依赖 `EditorApplication`。

常见生命周期风险：

- `ScopedConnection` 捕获的是信号对象引用，信号对象必须比连接活得更久；
- `QTimer::singleShot()` 的 lambda 必须绑定到合适的 Qt 接收者，否则窗口关闭后仍可能执行；
- `deleteLater()` 依赖事件循环，如果在事件循环停止后调用，释放时机会改变；
- 后端保存的 `SceneSurface` 原生句柄必须在窗口关闭前解除绑定。

## 六、一次编辑操作是怎样完成的

### 6.1 Hierarchy 选择对象

1. 用户点击 `HierarchyPanel` 中的 `QTreeWidgetItem`。
2. Qt 发出 `itemSelectionChanged`。
3. `HierarchyPanel::onTreeSelectionChanged()` 收集 UUID。
4. 调用 `EditorSelection::selectMany()`。
5. `EditorSelection` 发出 `cakery::Signal`。
6. `EditorSession` 的订阅回调向后端发送 `selection_changed`。
7. `RuntimeEditorBackend` 更新 `m_selectedUuid`，刷新 Tile 编辑状态和 Gizmo。
8. Inspector 和 Hierarchy 自己也订阅 Selection，刷新显示。

选择状态存的是 UUID，不是 `QTreeWidgetItem*`。这是正确方向，因为 UI 刷新会销毁并重建树节点。

### 6.2 Inspector 修改字段

1. `InspectorPanel` 根据选中实体的组件创建 `EditorJsonWidget`。
2. Runtime 后端可以通过 `inspectComponent()` 提供反射字段元数据。
3. `EditorJsonWidget` 将 JSON 类型映射到 Qt 控件：数字对应 SpinBox，布尔对应 CheckBox，枚举对应 ComboBox，颜色对应颜色按钮，资源引用对应拖拽字段。
4. 控件改变后发出 `valueChanged`。
5. Inspector 调用 `EditorSession::updateComponent()` 或 `updateManagedComponent()`。
6. Session 创建 `UpdateComponentCommand`，交给 `EditorHistory` 执行。
7. 文档模型变更并标记 dirty。
8. Session 序列化一份快照，以 `document_changed` 命令发送给 Runtime 后端。
9. Runtime 后端把快照 reconcile 到活动场景。

这里有一个重要事实：JSON 是当前文档值的来源，反射只提供“这个字段是什么、能不能编辑、范围是多少”等元数据。反射失败时，Inspector 会退回通用 JSON 编辑器。

### 6.3 Scene 视口输入

`SceneSurface` 是一个启用 `WA_NativeWindow` 的 Qt 控件。它把自己的原生窗口句柄、逻辑尺寸、像素尺寸和设备像素比率交给 Session，再由 Runtime 后端创建渲染目标。

鼠标路径如下：

```text
SceneSurface::mousePressEvent
    -> EditorCommandMessage("scene_mouse_down", "x,y,button,alt")
        -> EditorSession::execute()
            -> RuntimeEditorBackend::execute()
                -> 相机输入 / Gizmo 命中 / 点选 / TilePaint
```

Gizmo 拖拽期间，后端发出 `transform_drag_begin` 和 `transform_drag_end`，Session 用它们开启和关闭历史合并；中间的 `transform_changed` 事件回写 TransformComponent。

### 6.4 Undo/Redo

所有常见文档操作都应通过 `EditorCommand`：创建、删除、重命名、改组件、重父级等。`EditorHistory` 保存 undo 和 redo 两个栈。

```text
execute(command)
    -> command.execute(model)
    -> 放入 undo 栈
    -> 清空 redo 栈

undo()
    -> 从 undo 栈取出
    -> command.revert(model)
    -> 放入 redo 栈
```

连续字段编辑可以通过 `mergeWith()` 合并成一个历史项。合并的前提是 UUID、组件索引和字段路径一致。

### 6.5 资产刷新

`ProjectPanel::refresh()` 发出 `asset.refresh`。Runtime 后端让 `AssetDatabase` 异步刷新资源数据库；`EditorWindow` 每 100ms 读取进度并显示进度对话框。Runtime 安全点发现刷新完成后，调用 `finalize()`，再通过 `asset_database_changed` 通知 ProjectPanel 重建列表。

资产扫描树和资产数据库是两件事：

- 文件树来自项目目录递归遍历；
- UUID、类型、依赖和 dirty 状态来自 Runtime 的 AssetDatabase。

这也是当前架构中容易产生“文件存在但数据库没有记录”问题的地方。

## 七、各个主要类应该怎样理解

### `EditorSession`

它是编辑器业务的门面（Facade）。UI 应优先调用它，而不是拿到后端指针后自己拼协议。它管理状态：`Created`、`OpeningProject`、`Ready`、`Degraded`、`Failed`、`Closing`、`Closed`。

### `EditorDocumentModel`

它保存当前编辑文档：场景名、实体列表、父子关系、native components 和 managed components。它负责 dirty 标记、增删改实体和组件，并通过 JSON 序列化落盘。

### `IEditorBackend`

这是后端契约。它同时包含文档打开、命令执行、视口挂载、资源查询、日志、工具动作和安全点更新。接口过宽时，调用者很难知道某个能力是否可用，因此使用前应检查 `BackendCapabilities` 和 `BackendStatus`。

### `RuntimeEditorBackend`

它把编辑器协议翻译成 Runtime 操作，持有 `dodoe::Application`、编辑器相机、渲染目标、资源数据库和播放快照。它还把点选、Gizmo、Tilemap 操作反向发回 Session。

### `EditorWorkspaceContext`

它不是状态容器，也不是服务定位器。它只是把 Session、资源定位器、项目和能力查询集中暴露给面板。保持它轻量有利于测试和替换 UI。

### 各个 Panel

- `HierarchyPanel`：实体树、重命名、删除、创建和重父级。
- `InspectorPanel`：当前实体或资源的属性编辑。
- `ProjectPanel`：项目文件树、资产网格、预览、导入和文件监听。
- `ConsolePanel`：本地消息与 Runtime 日志的合并展示。
- `HistoryPanel`：Undo/Redo 和编辑历史展示。
- `TilePalettePanel`、`TileLayersPanel`：Tilemap 专用编辑入口。

面板应当是“视图 + 很薄的交互协调器”，复杂规则应下沉到 Session、Command 或 Backend。

## 八、当前发现的问题

下面的问题来自源码阅读，按用户影响和修复优先级排列。

### P0：错误结果会被误报为成功

1. `EditorSession::openProject()` 调用 `openDocument(startScenePath)` 后没有检查返回值，起始场景打不开时，项目仍可能被认为已经打开。[EditorSession.cpp:63-67]
2. `EditorSession::openDocument()` 调用 `m_backend->openDocument(documentPath)` 后忽略返回值，后端解析失败不会传回 UI。[EditorSession.cpp:70-82]
3. `RuntimeEditorBackend::execute()` 对不认识的命令走到末尾直接返回 `true`，拼写错误的命令会看起来像执行成功。[RuntimeEditorBackend.cpp:721-764]

建议：所有跨层调用都返回结构化错误（错误码、用户消息、诊断上下文），并在 UI 的 Console 中展示；未知命令必须失败。

### P0：文档写入缺少可靠性保护

`EditorDocumentSerializer::save()` 直接使用 `std::ofstream` 写目标文件。写入中途进程退出或磁盘异常时，原文件可能只剩半截 JSON。[EditorDocumentSerializer.cpp:129-138]

建议：写临时文件、刷新并替换目标文件，同时保留最近一次备份；加载时做版本、UUID、父级存在性和父级循环校验。

### P1：UI 线程承担了大规模文件树扫描

`ProjectPanel::reloadAssets()` 使用 `recursive_directory_iterator` 收集监听目录，随后 `addDirectory()` 同步递归创建大量 `QTreeWidgetItem`。[ProjectPanel.cpp:472-539、567-620]

项目变大后，这会阻塞 Qt 事件循环，表现为窗口卡顿、拖拽延迟和定时器不准。建议把文件发现和资产匹配放到工作线程，回到 GUI 线程时只提交增量模型；同时限制 `QFileSystemWatcher` 目录数量并处理添加失败。

### P1：属性搜索只过滤组件标题

Inspector 的过滤器只检查 `inspectorSectionHeader` 文本，没有检查字段名、嵌套字段或 Tooltip。[InspectorPanel.cpp:341-347]

建议让 `EditorJsonWidget` 为每个字段保留稳定路径，过滤时隐藏不匹配字段，并在父组件匹配时显示全部字段。

### P1：实体选择和资产选择是两条状态链

Hierarchy 使用 `EditorSelection`，而 ProjectPanel 直接发出 Qt 的 `assetSelected` 信号给 Inspector；选择资产时没有调用 `EditorSelection::setAsset()`。[EditorWindow.cpp:1212-1215、InspectorPanel.cpp:389-402]

结果是：Session 仍可能认为上一个实体处于选中状态，Runtime 后端也仍保留上一个实体的选择，而 Inspector 显示的是资产。建议把实体、资产和无选择统一成一个选择模型，或者明确两个选择上下文并在切换时互相清空。

### P1：命令参数校验不完整

`CommandRegistry::executeStructured()` 把 JSON 对象直接交给命令处理器，没有统一做类型、必填项和范围验证。[CommandRegistry.cpp:63-76] 处理器内部大量使用 `json::value()`，传入错误类型时可能抛出未处理异常。

建议在 Registry 层根据 `ParamSpec` 做校验，统一捕获 JSON 异常，并让 `toolSchema()` 输出包含 `type`、`properties` 和 `required` 的标准参数结构。

此外，`IEditorBackend` 的命令名和 payload 仍是字符串协议，例如 `scene_mouse_down` 使用逗号分隔文本。协议扩展时很容易出现字段顺序不一致、数字格式不一致或错误信息丢失。建议逐步改成带版本的结构化消息，并保留统一的序列化入口。

### P1：历史系统缺少失败语义

`EditorCommand::execute()` 的返回类型是 `void`，`EditorHistory::execute()` 执行后无论命令内部是否找到目标，都可能把命令压入 undo 栈。[EditorHistory.cpp:11-25]

建议把命令执行结果改成可表达成功/失败的结果；失败命令不能进入历史，复合命令失败时要回滚已经执行的子命令。

### P1：编辑器配置和硬编码 UI 并存

`EditorConfig` 能读取菜单、面板、布局和快捷键 JSON，但 `EditorWindow` 的菜单、工具栏和面板主要仍在 C++ 中手工创建。这样会出现“配置文件看起来支持某功能，但实际 UI 不使用它”的错觉。

建议明确边界：基础菜单由 C++ 保证，扩展菜单由配置驱动；快捷键统一注册；布局文件和用户布局状态有版本号和迁移策略。

### P1：视口挂载的状态门槛过宽

`EditorSession::attachSceneSurface()` 只拒绝 `Closing` 和 `Closed`，在 `Created` 状态也可能把原生窗口句柄交给后端。[EditorSession.cpp:143-168]

正常流程中 SceneSurface 在项目打开后才显示，因此不一定立即触发；但这个公共入口本身允许“尚未打开项目就启动 Runtime”。建议只允许 `Ready` 或 `Degraded` 状态挂载，并在状态变化时显式通知 UI。

### P2：选择和 Inspector 的扩展性不足

`EditorSelection` 已支持多个 UUID，但 Runtime Gizmo、Inspector 主路径仍以 `selected()` 的第一个对象为中心。多选编辑、mixed value、批量命令和多对象变换尚未形成闭环。

建议先定义 `SelectionSet` 和批量 Command，再扩展 UI；不要只在面板层添加 Ctrl/Shift 逻辑。

### P2：渲染辅助能力仍是原型级

点选当前主要依赖简化射线测试；`DebugDraw::Flush()` 仍然只是清空缓存。场景可视化、碰撞体、真实对象 ID picking 和调试绘制需要统一的 Runtime 渲染通道。

## 九、推荐的改进顺序

1. 先统一错误模型：Session、Backend、Command、Serializer 都必须能明确报告失败。
2. 建立文档可靠性：版本号、校验、原子保存、备份、dirty/save point。
3. 把资产扫描、缩略图和文件监听改成异步增量模型，避免阻塞 Qt 事件循环。
4. 让配置真正驱动快捷键、菜单、面板初始状态和布局恢复。
5. 完成 Hierarchy 多选、批量命令和 Gizmo 历史合并。
6. 用反射元数据覆盖 managed/native component、资源引用和字段过滤。
7. 最后扩展 Prefab、Tilemap、Play Mode 调试、Profiler 和自动化 UI 测试。

## 十、遇到问题时的排查路径

可以按用户动作从上到下定位：

```text
界面没有反应
  -> Qt 信号是否连接？
  -> Panel 是否调用了 WorkspaceContext？
  -> Session 状态是否 Ready/Degraded？
  -> Backend 是否返回 false？
  -> Console 是否有诊断？

数据显示不对
  -> DocumentModel 中的 JSON 是否正确？
  -> Selection UUID 是否仍然存在？
  -> Panel 是否在模型变化后刷新？
  -> Runtime 是否收到 document_changed？

视口黑屏或尺寸错误
  -> SceneSurface 是否 attach？
  -> native handle 和 pixel size 是否有效？
  -> Session 是否提交了新的 ViewportMetrics？
  -> Runtime 后端是否 booted 并创建 RenderViewTarget？
```

## 十一、术语表

| 术语 | 含义 |
|---|---|
| Qt event loop | Qt 持续处理窗口、输入、绘制和定时器事件的循环 |
| QObject parent | Qt 对象树中的父对象，通常负责子对象释放 |
| signal/slot | Qt 的发布/订阅机制 |
| QSS | Qt Style Sheet，用于控件样式 |
| Dock | 可停靠、浮动、隐藏的面板容器，Cakery 使用 Qt Advanced Docking System |
| Session | 一次编辑工作区的业务会话 |
| Backend | 把编辑器命令翻译成具体运行时操作的适配器 |
| DocumentModel | 编辑态场景的内存数据 |
| Command | 一个可执行、可撤销的编辑动作 |
| dirty | 内存文档与磁盘文件不一致 |
| safe point | Runtime 可以安全处理编辑器请求和推进一帧的位置 |
| native handle | Qt 控件对应的操作系统窗口句柄 |

## 十二、相关文档

- [editor.md](editor.md)：编辑器模块总览和更广泛的功能缺口分析。
- [core.md](core.md)：Runtime Core 基础对象和模块。
- [resource.md](resource.md)：资源、Asset 和导入链路。
- [render.md](render.md)：渲染系统和视图目标。
- [ui-input-window.md](ui-input-window.md)：Runtime UI、输入和窗口系统。
