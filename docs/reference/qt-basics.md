# Qt 入门：用 Cakery 的代码理解 Qt

这是一份给 Qt 初学者的入门说明。假设你只知道一点 C++，不要求你先掌握 Qt 的全部类。读完后，你应该能看懂 Cakery 中大多数窗口和面板代码，并知道修改界面时应该从哪里下手。

## 1. Qt 是什么

Qt 是一套跨平台的 C++ 应用开发框架。Cakery 使用 Qt 做编辑器的桌面界面，使用的主要模块是 `Qt6::Widgets`。

可以把 Qt 想成几组工具：

| Qt 概念 | 用途 | Cakery 中的例子 |
|---|---|---|
| `QApplication` | 管理整个 GUI 程序 | `EditorApplication` |
| `QWidget` | 一个界面对象的基类 | `InspectorPanel`、`ConsolePanel` |
| `QMainWindow` | 带菜单、工具栏和中心区域的主窗口 | `EditorWindow` |
| 控件 | 按钮、输入框、列表等 | `QPushButton`、`QLineEdit` |
| 布局 | 自动排列控件 | `QVBoxLayout`、`QHBoxLayout` |
| signal/slot | 控件之间传递事件 | 按钮点击后保存场景 |
| event | 鼠标、键盘、绘制、窗口变化 | `mousePressEvent`、`paintEvent` |
| QSS | Qt 的样式表 | `cakery-dark.qss` |

Qt 处理的是“窗口和用户操作”。Cakery 的场景、实体、资源和 Undo/Redo 仍然属于编辑器自己的代码。

## 2. 一个 Qt 程序的最小结构

下面是一个最小的 Qt Widgets 程序：

```cpp
#include <QApplication>
#include <QLabel>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QLabel label("Hello Qt");
    label.resize(300, 80);
    label.show();

    return app.exec();
}
```

逐行理解：

1. `QApplication app(...)` 创建整个 GUI 程序的管理对象。
2. `QLabel` 是一个显示文字的控件。
3. `resize()` 设置初始尺寸。
4. `show()` 让控件显示出来。
5. `app.exec()` 进入 Qt 事件循环。

没有 `exec()`，程序会很快结束，窗口也无法持续响应输入。事件循环会不断处理鼠标、键盘、绘制和定时器事件。

在 Cakery 中，`EditorApplication` 继承 `QApplication`，`EditorApplication::run()` 最终也会调用 `exec()`。

## 3. QWidget：界面对象的基类

大多数 Qt Widgets 都继承自 `QWidget`。例如：

```cpp
QWidget* panel = new QWidget();
QPushButton* button = new QPushButton("Save", panel);
```

这里有两个对象：一个普通面板和一个按钮。按钮的第二个参数 `panel` 是它的父对象。

### 3.1 parent 的意义

Qt 对象通常形成一棵父子树：

```text
EditorWindow
└── toolbar
    └── saveButton
```

当父对象销毁时，Qt 会销毁它的子对象。因此，界面代码经常写成：

```cpp
auto* button = new QPushButton(parentWidget);
```

这不是随意丢弃内存，而是把按钮的所有权交给 Qt 对象树。Cakery 的面板、按钮、布局和 Dock 控件大量使用这种方式。

注意：Qt 的父对象必须是 `QObject`，而 `QLayout` 不是 `QObject` 的子类。布局的生命周期由它安装到的控件管理，不要把布局当成普通控件使用。

### 3.2 QWidget 和 QObject

`QObject` 是 Qt 对象系统的基础。它提供：

- 父子对象关系；
- signal/slot；
- 属性和运行时类型信息；
- 定时器和事件处理。

`QWidget` 在此基础上增加了可显示的窗口行为。

## 4. 控件：用户能看到和操作的东西

常见 Widgets：

```cpp
QLabel*       label;       // 显示文字或图片
QPushButton*  button;      // 普通按钮
QToolButton*  toolButton;  // 适合工具栏的按钮
QLineEdit*    lineEdit;    // 单行输入框
QTextEdit*    textEdit;    // 多行文本编辑器
QCheckBox*    checkBox;    // 勾选框
QComboBox*    comboBox;    // 下拉框
QListWidget*  list;        // 简单列表
QTreeWidget*  tree;        // 简单树
QScrollArea*  scroll;      // 可滚动容器
```

Cakery 的 `ConsolePanel` 就是由 `QLineEdit`、`QComboBox`、`QToolButton` 和 `QListWidget` 组合出来的。

### 4.1 设置文字和读取文字

```cpp
auto* nameEdit = new QLineEdit(this);
nameEdit->setText("Main Camera");

QString name = nameEdit->text();
```

Qt 的文本类型通常是 `QString`，不是 `std::string`。Cakery 在边界处转换：

```cpp
std::string nativeName = nameEdit->text().toStdString();
QString displayName = QString::fromStdString(nativeName);
```

不要在每一行都来回转换。界面层使用 `QString`，编辑器核心使用 `std::string`，只在两层交界处转换更清楚。

### 4.2 给控件命名

```cpp
button->setObjectName("saveButton");
```

`objectName` 有两个用途：

1. 调试时更容易在对象树里找到控件；
2. QSS 可以使用它选择样式。

例如：

```css
#saveButton {
    color: white;
    background: #3478c9;
}
```

Cakery 使用了 `editorToolbar`、`inspectorSection`、`consoleList` 等对象名。

## 5. 布局：不要用坐标摆放控件

Qt 的布局会根据窗口大小、字体和子控件的最小尺寸自动安排位置。优先使用布局，不要给每个控件写固定坐标。

### 5.1 垂直布局

```cpp
auto* layout = new QVBoxLayout(panel);
layout->addWidget(new QLabel("Name", panel));
layout->addWidget(new QLineEdit(panel));
layout->addWidget(new QPushButton("Save", panel));
```

效果是从上到下排列：标签、输入框、按钮。

### 5.2 水平布局

```cpp
auto* row = new QHBoxLayout();
row->addWidget(new QLabel("Search", panel));
row->addWidget(new QLineEdit(panel), 1);
row->addWidget(new QPushButton("Clear", panel));
layout->addLayout(row);
```

第二个参数 `1` 是伸缩因子，表示输入框占用多余空间。

### 5.3 拉伸和边距

```cpp
layout->setContentsMargins(8, 8, 8, 8);
layout->setSpacing(6);
layout->addStretch();
```

- `setContentsMargins` 设置布局与边界之间的空白；
- `setSpacing` 设置子控件之间的空白；
- `addStretch` 添加可伸缩的空白，常用于把按钮推到一侧或底部。

### 5.4 Cakery 中的典型面板结构

`ConsolePanel` 的结构可以简化成：

```text
ConsolePanel
└── QVBoxLayout
    ├── tools QWidget
    │   └── QHBoxLayout
    │       ├── search QLineEdit
    │       ├── levelFilter QComboBox
    │       ├── copy QToolButton
    │       └── clear QToolButton
    └── list QListWidget
```

看懂这种树以后，就能顺着 `new ...`、`addWidget()` 和 `addLayout()` 读懂大多数 Qt 界面构造函数。

## 6. signal 和 slot：事件发生后做什么

Qt 控件不会直接调用你的业务函数，而是发出 signal。你用 `connect()` 把 signal 连接到 slot。slot 可以是普通成员函数、lambda 或 Qt 自带函数。

```cpp
connect(button, &QPushButton::clicked, this, [this]() {
    saveScene();
});
```

含义是：

```text
button 发出 clicked
        ↓
执行 lambda
        ↓
调用 saveScene()
```

### 6.1 连接到成员函数

```cpp
connect(lineEdit, &QLineEdit::textChanged,
        this, &MyPanel::onNameChanged);
```

成员函数声明通常是：

```cpp
private:
    void onNameChanged(const QString& text);
```

signal 的参数和接收函数的参数需要匹配。

### 6.2 lambda 捕获

```cpp
connect(button, &QPushButton::clicked, this, [this, id]() {
    openItem(id);
});
```

`[this, id]` 表示 lambda 使用当前对象和一个局部变量 `id`。如果捕获一个临时变量的引用，要特别小心它是否还活着。优先按值捕获简单数据。

把 `this` 作为接收者还有一个好处：当 `this` 被销毁时，Qt 会自动断开这条连接。

### 6.3 阻塞 signal

刷新控件时，有时不希望触发它们的 signal：

```cpp
QSignalBlocker blocker(comboBox);
comboBox->setCurrentIndex(2);
```

`blocker` 离开作用域后自动恢复。Cakery 在重建 Hierarchy 树时使用这个模式，避免“程序正在刷新树，却被当成用户选择”而产生循环。

## 7. `Q_OBJECT` 是什么

如果一个自定义类需要使用 Qt 的 signal、slot 或属性系统，通常在类声明中写：

```cpp
class MyPanel : public QWidget {
    Q_OBJECT
public:
    explicit MyPanel(QWidget* parent = nullptr);
};
```

`Q_OBJECT` 告诉 Qt：这个类需要进入 Qt 的元对象系统。Cakery 的 `EditorApplication`、`EditorWindow` 和各个 Panel 都使用了它。

初学阶段只需要记住：

- 继承 Qt 对象并声明 Qt signal/slot 时，加 `Q_OBJECT`；
- 类的声明放在头文件，成员函数实现放在 `.cpp`；
- 不要把 `Q_OBJECT` 写进普通的非 Qt 核心类。

## 8. 事件函数：处理鼠标、键盘和绘制

有些行为不适合用普通 signal 表达，例如自定义绘制和鼠标拖动。这时重写事件函数：

```cpp
class Canvas : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
};
```

### 8.1 绘制

```cpp
void Canvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#333333"));
    painter.drawText(rect(), Qt::AlignCenter, "Scene");
}
```

`paintEvent` 被 Qt 调用时，控件需要把自己的内容画出来。不要在构造函数里永久画一次，因为窗口被遮挡、缩放或主题切换后，Qt 还会再次要求重绘。

`SceneSurface` 在 Editor-Only 模式下就是这样绘制占位网格和文字的。

### 8.2 鼠标

```cpp
void Canvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // 记录开始位置
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}
```

`accept()` 表示事件已经处理。若不处理某个事件，调用基类实现，让 QWidget 继续执行默认行为。

在 Cakery 中，`SceneSurface` 收到鼠标事件后，会将坐标和按键转换为 `EditorCommandMessage`，交给 `EditorSession` 和 Runtime 后端。

### 8.3 键盘和滚轮

对应函数是 `keyPressEvent`、`keyReleaseEvent` 和 `wheelEvent`。处理方式与鼠标类似：读取事件内容，转换成编辑器需要的命令，然后决定是否调用基类。

## 9. QTimer：延迟执行和周期执行

### 9.1 周期执行

```cpp
auto* timer = new QTimer(this);
timer->setInterval(250);
connect(timer, &QTimer::timeout, this, [this]() {
    refreshLogs();
});
timer->start();
```

这段代码每隔约 250ms 调用一次 `refreshLogs()`。Cakery 的 `ConsolePanel` 用它定期同步 Runtime 日志，`EditorWindow` 用另一个定时器推进 Runtime 安全点。

### 9.2 延迟一次执行

```cpp
QTimer::singleShot(0, this, [this]() {
    refreshSelection();
});
```

`0` 不是“完全不等待”，而是把任务放到当前事件处理完成之后。它常用于避免在 Qt 正在发出 signal 的过程中立刻重建同一个控件。

### 9.3 定时器不是实时钟

系统繁忙时，Qt 定时器可能晚一点触发。它适合刷新界面、轮询状态和安排工作，不适合假设精确时间间隔的模拟逻辑。Runtime 后端会使用实际时间差计算每一帧的时间。

## 10. `deleteLater()` 和对象生命周期

```cpp
someWidget->deleteLater();
```

这表示“在事件循环合适的时候删除对象”，而不是立刻删除。它适合在 signal 回调或事件处理中销毁当前相关控件。

常见注意事项：

- 有父对象的 QWidget 通常不需要手动删除；
- `deleteLater()` 依赖事件循环继续运行；
- lambda 捕获 `this` 时，最好把 `this` 作为 `connect` 的接收者；
- 非 Qt 核心对象使用 `std::unique_ptr`，不要把它们硬塞进 QObject 父子树。

## 11. QMainWindow、菜单、工具栏和 Dock

`QMainWindow` 是适合应用主窗口的特殊 QWidget，提供菜单栏、工具栏、中心控件和状态栏等区域。

```cpp
auto* fileMenu = menuBar()->addMenu("File");
QAction* save = fileMenu->addAction("Save");
save->setShortcut(QKeySequence::Save);

auto* toolbar = addToolBar("Tools");
toolbar->addAction(save);
```

`QAction` 是“一个命令的界面表示”。同一个 action 可以同时出现在菜单和工具栏中。

Cakery 使用 Qt Advanced Docking System 的 `CDockManager` 和 `CDockWidget`，所以面板可以停靠、浮动、隐藏和恢复布局。理解方式仍然和普通 QWidget 一样：Dock 是容器，Panel 是它里面真正显示内容的 QWidget。

## 12. QSS：给界面换外观

QSS 的写法类似 CSS：

```css
QLineEdit {
    padding: 4px;
}

#consoleList {
    background: #202020;
}

QPushButton:hover {
    background: #3a78b8;
}
```

选择器常见写法：

- `QPushButton`：所有按钮；
- `#objectName`：指定对象名的控件；
- `QPushButton:hover`：鼠标悬停状态；
- `[property="value"]`：动态属性匹配。

Cakery 在 `EditorApplication::applyTheme()` 中加载 QSS，再用 `setStyleSheet()` 应用到整个应用。控件自身的 `setStyleSheet()` 会覆盖一部分全局主题，使用时要克制。

## 13. Qt 文本、容器和 C++ 类型

你会在 Cakery 里同时看到 Qt 和标准库类型：

| Qt 类型 | 标准 C++ 类型 | 用途 |
|---|---|---|
| `QString` | `std::string` | 文本 |
| `QList<T>` | `std::vector<T>` | 列表 |
| `QByteArray` | 字节数组或字符串 | 二进制数据、布局状态 |
| `QPair<A,B>` | `std::pair<A,B>` | 两个值 |
| `QHash<K,V>` | `std::unordered_map<K,V>` | 哈希表 |

选择原则：界面 API 使用 Qt 类型很自然；编辑器核心和 Runtime 接口已经使用标准库类型时，不必为了“统一”而大面积改动。边界转换即可。

## 14. 用 Qt 方式阅读 Cakery

阅读一个 Panel 时，可以按这个顺序：

1. 看头文件：它继承哪个 Qt 类？有哪些控件指针、signal 和私有函数？
2. 看构造函数：控件在哪里 `new`？布局如何 `addWidget`？
3. 找所有 `connect`：用户动作会触发哪些函数？
4. 找 `refresh` 或 `reload`：数据改变后界面如何重建？
5. 找 `event` 函数：是否有自定义绘制、拖拽、鼠标或键盘行为？
6. 找 `QTimer`：是否有周期刷新或延迟操作？
7. 再追踪 `EditorWorkspaceContext` 和 `EditorSession`，看 UI 动作如何进入编辑器核心。

例如阅读 `ConsolePanel.cpp`：

```text
QVBoxLayout
  -> 搜索框、级别下拉框、复制/清除按钮
  -> QListWidget
  -> connect(textChanged) -> refresh()
  -> QTimer.timeout -> syncBackendLogs()
```

这比一开始从 Runtime 深处追代码更容易建立正确的上下文。

## 15. 初学者最常见的错误

### 把控件写死在固定坐标上

窗口缩放、字体变化和翻译切换都会让固定坐标失效。使用布局和 size policy。

### 忘记设置父对象

没有父对象的 QWidget 需要明确的所有权，否则容易泄漏或生命周期不清楚。能放进现有布局的控件，通常直接把父 QWidget 传进去。

### 在 signal 回调中无限触发自己

例如 `refresh()` 修改列表，列表又触发选择 signal，选择 signal 再次调用 `refresh()`。重建控件时使用 `QSignalBlocker`，或增加明确的刷新状态。

### 在 UI 线程做大量工作

递归扫描目录、读取大量图片和复杂资源处理会阻塞事件循环。UI 线程只负责轻量更新；耗时任务完成后，再把结果交回界面。

### 用 UI 控件保存业务真相

`QTreeWidgetItem`、`QListWidgetItem` 会被重建，不能作为长期业务对象。Cakery 使用 UUID 和 `EditorDocumentModel` 保存真实状态，控件只是显示它。

### 不检查跨层操作结果

按钮点击后调用 Session 或 Backend 时，要处理 `bool` 返回值和诊断信息，否则用户会看到“界面没反应”，而 Console 没有线索。

## 16. 一个小型 Panel 示例

下面的例子展示一个输入框和按钮如何组成一个面板：

```cpp
class RenamePanel : public QWidget {
    Q_OBJECT
public:
    explicit RenamePanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        m_name = new QLineEdit(this);
        auto* save = new QPushButton("Apply", this);

        layout->addWidget(m_name);
        layout->addWidget(save);
        layout->addStretch();

        connect(save, &QPushButton::clicked, this, [this]() {
            const QString text = m_name->text().trimmed();
            if (!text.isEmpty()) {
                emit nameApplied(text);
            }
        });
    }

signals:
    void nameApplied(const QString& name);

private:
    QLineEdit* m_name = nullptr;
};
```

读这个例子时，注意四件事：

1. 类继承 `QWidget`，所以它可以放进其他布局或 Dock。
2. `Q_OBJECT` 让它可以声明 Qt signal。
3. 控件的父对象是 `this`，由面板管理生命周期。
4. 按钮只负责发出 `nameApplied`，真正的业务动作可以由外层连接到 Session。

这正是 Cakery 推荐的边界：Panel 处理输入，Session 处理编辑器规则。

## 17. Qt 与 Cakery 的对应关系

```text
Qt 用户操作
  -> QWidget / QAction / QTreeWidget / QLineEdit
  -> signal 或 event
  -> Panel 的回调
  -> EditorWorkspaceContext
  -> EditorSession
  -> EditorDocumentModel / EditorHistory
  -> RuntimeEditorBackend
  -> Dodoe Runtime
```

反方向的数据更新则是：

```text
DocumentModel 或 Backend 改变
  -> cakery::Signal 或 BackendEventMessage
  -> Panel 的 refresh()
  -> Qt 控件更新
```

如果你只记住一句话：Qt 控件是“显示和输入”，Cakery Session 是“编辑器业务”，Runtime Backend 是“把编辑结果应用到引擎运行时”。

## 18. 推荐的学习顺序

1. 先读 `QWidget`、布局和 `connect`。
2. 再读 `EditorApplication`，理解应用和窗口如何启动。
3. 阅读 `ConsolePanel`，它的控件结构相对简单。
4. 阅读 `HierarchyPanel`，理解树、选择和模型刷新。
5. 阅读 `InspectorPanel`，理解动态创建控件和 JSON 值编辑。
6. 最后阅读 `EditorWindow`，理解菜单、工具栏、Dock、事件过滤器和定时器如何组合。
7. 遇到编辑器业务时，再回到 [Cakery Qt 架构说明](cakery-qt-architecture.md)。

## 19. 相关文件

- `engine/src/editor/cakery/app/EditorApplication.h/.cpp`
- `engine/src/editor/cakery/ui/shell/EditorWindow.h/.cpp`
- `engine/src/editor/cakery/ui/panels/ConsolePanel.h/.cpp`
- `engine/src/editor/cakery/ui/panels/HierarchyPanel.h/.cpp`
- `engine/src/editor/cakery/ui/panels/InspectorPanel.h/.cpp`
- `engine/src/editor/cakery/ui/inspector/EditorJsonWidget.h/.cpp`
- `engine/res/editor/themes/cakery-dark.qss`

