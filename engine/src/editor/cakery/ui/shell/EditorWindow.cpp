// do@Redlive

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#endif

#include "EditorWindow.h"

#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/panels/HierarchyPanel.h"
#include "cakery/ui/panels/HistoryPanel.h"
#include "cakery/ui/panels/InspectorPanel.h"
#include "cakery/ui/panels/ProjectPanel.h"

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

#include <QAbstractButton>
#include <QApplication>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

namespace cakery {

namespace {

int toCameraButton(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) return 0;
    if (button == Qt::MiddleButton) return 1;
    return 2;
}

QIcon editorButtonIcon(const QString& fileName)
{
    const QString path = QDir(QApplication::applicationDirPath())
        .filePath(QStringLiteral("resources/pictures/Buttons/") + fileName);
    return QIcon(path);
}

QIcon editorToolIcon(const QString& fileName)
{
    const QString path = QDir(QApplication::applicationDirPath())
        .filePath(QStringLiteral("resources/editor/icons/") + fileName);
    return QIcon(path);
}

} // namespace

class SceneSurface final : public QWidget {
public:
    explicit SceneSurface(EditorWorkspaceContext& context, QWidget* parent = nullptr)
        : QWidget(parent), m_context(context)
    {
        setMinimumSize(480, 320);
        setAttribute(Qt::WA_NativeWindow);
        setFocusPolicy(Qt::StrongFocus);
        setAutoFillBackground(false);
        setMouseTracking(true);
    }

    void attach()
    {
        if (m_attached) return;
        SceneSurfaceDescriptor surface;
        surface.nativeHandle = static_cast<std::uintptr_t>(winId());
        m_attached = m_context.session().attachSceneSurface(surface);
        publishMetrics();
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        attach();
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        publishMetrics();
    }

    void paintEvent(QPaintEvent* event) override
    {
        if (m_context.capabilities().scenePreview) {
            QWidget::paintEvent(event);
            return;
        }
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#333333"));
        painter.setPen(QColor("#3f3f3f"));
        for (int x = 0; x < width(); x += 32) painter.drawLine(x, 0, x, height());
        for (int y = 0; y < height(); y += 32) painter.drawLine(0, y, width(), y);

        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPointSize(14);
        painter.setFont(titleFont);
        painter.setPen(QColor("#E8E8E8"));
        painter.drawText(rect().adjusted(20, height() / 2 - 34, -20, 0),
                         Qt::AlignHCenter | Qt::AlignTop, "Scene");

        painter.setFont(QFont());
        painter.setPen(QColor("#A0A0A0"));
        painter.drawText(rect().adjusted(20, height() / 2 + 2, -20, 0),
                         Qt::AlignHCenter | Qt::AlignTop,
                         "Scene preview unavailable in Editor-Only");
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (!m_context.capabilities().scenePreview) {
            QWidget::mousePressEvent(event);
            return;
        }
        const QString payload = QStringLiteral("%1,%2,%3,%4")
            .arg(event->position().x())
            .arg(event->position().y())
            .arg(toCameraButton(event->button()))
            .arg(event->modifiers().testFlag(Qt::AltModifier) ? 1 : 0);
        m_context.session().execute(EditorCommandMessage{"scene_mouse_down", payload.toStdString()});
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!m_context.capabilities().scenePreview) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        const QString payload = QStringLiteral("%1,%2")
            .arg(event->position().x())
            .arg(event->position().y());
        m_context.session().execute(EditorCommandMessage{"scene_mouse_move", payload.toStdString()});
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (!m_context.capabilities().scenePreview) {
            QWidget::mouseReleaseEvent(event);
            return;
        }
        const QString payload = QString::number(toCameraButton(event->button()));
        m_context.session().execute(EditorCommandMessage{"scene_mouse_up", payload.toStdString()});
        QWidget::mouseReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (!m_context.capabilities().scenePreview) {
            QWidget::wheelEvent(event);
            return;
        }
        const QString payload = QString::number(event->angleDelta().y() / 120.0);
        m_context.session().execute(EditorCommandMessage{"scene_mouse_wheel", payload.toStdString()});
        QWidget::wheelEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (!m_context.capabilities().scenePreview) {
            QWidget::keyPressEvent(event);
            return;
        }
        m_context.session().execute(EditorCommandMessage{"scene_key", std::to_string(event->key()) + ",1"});
        QWidget::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent* event) override
    {
        if (!m_context.capabilities().scenePreview) {
            QWidget::keyReleaseEvent(event);
            return;
        }
        m_context.session().execute(EditorCommandMessage{"scene_key", std::to_string(event->key()) + ",0"});
        QWidget::keyReleaseEvent(event);
    }

private:
    void publishMetrics()
    {
        if (!isVisible() || width() < 1 || height() < 1) return;
        const float dpr = static_cast<float>(devicePixelRatioF());
        ViewportMetrics metrics;
        metrics.logicalWidth = width();
        metrics.logicalHeight = height();
        metrics.devicePixelRatio = dpr;
        metrics.pixelWidth = static_cast<int>(std::lround(width() * dpr));
        metrics.pixelHeight = static_cast<int>(std::lround(height() * dpr));
        metrics.nativeHandle = static_cast<std::uintptr_t>(winId());
        metrics.sequence = ++m_sequence;
        m_context.session().submitViewportMetrics(metrics);
    }

    EditorWorkspaceContext& m_context;
    std::uint64_t m_sequence = 0;
    bool m_attached = false;
};

namespace {

QWidget* unavailablePanel(const QString& title, const QString& detail, QWidget* parent)
{
    auto* body = new QWidget(parent);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(12, 12, 12, 12);
    auto* label = new QLabel(title + "\n\n" + detail, body);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color: #A0A0A0;"));
    layout->addWidget(label);
    layout->addStretch();
    return body;
}

} // namespace

EditorWindow::EditorWindow(EditorWorkspaceContext& context, QWidget* parent)
    : QMainWindow(parent), m_context(context)
{
    if (m_context.capabilities().simulation) {
        setWindowTitle(QApplication::applicationName());
    } else {
        setWindowTitle(QStringLiteral("%1 - Preview").arg(QApplication::applicationName()));
    }
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    resize(1440, 900);
    setMinimumSize(900, 600);
    setStatusBar(nullptr);

    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DisableStylesheet, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::ActiveTabHasCloseButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::AllTabsHaveCloseButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasUndockButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DisableTabTextEliding, true);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::AutoHideFeatureEnabled, true);
    // Auto Hide is available from the dock title context menu only. Godot's
    // layout does not expose a permanent pin button in every title bar.
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::DockAreaHasAutoHideButton, false);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::AutoHideCloseOnOutsideMouseClick, true);
    m_dockManager = new ads::CDockManager(this);

    createTitleBar();
    createMenus();
    createToolbar();
    createDocks();
    createPanels();
    startSafePointTimer();

    m_historySubscription = m_context.session().history().subscribe([this]() { refreshUndoRedoActions(); });
    refreshUndoRedoActions();
}

EditorWindow::~EditorWindow()
{
    if (m_safePointTimer) {
        m_safePointTimer->stop();
    }
}

void EditorWindow::createMenus()
{
    auto* file = m_menuBar->addMenu(tr("File"));
    auto* open = file->addAction(tr("Open Project..."));
    connect(open, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, tr("Open Project"));
        if (!path.isEmpty()) enterWorkspace(path);
    });

    auto* save = file->addAction(tr("Save Scene"));
    save->setShortcut(QKeySequence::Save);
    connect(save, &QAction::triggered, this, [this]() {
        if (!m_context.session().documentModel().hasDocument()) {
            return;
        }
        m_context.session().saveDocument(std::string());
    });

    auto* saveAs = file->addAction(tr("Save Scene As..."));
    connect(saveAs, &QAction::triggered, this, [this]() {
        if (!m_context.session().documentModel().hasDocument()) {
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Save Scene As"), QString(), tr("Dodoe Scene (*.doscn)"));
        if (!path.isEmpty()) {
            m_context.session().saveDocument(path.toStdString());
        }
    });

    file->addSeparator();
    file->addAction(tr("Close"), this, &QWidget::close);

    auto* edit = m_menuBar->addMenu(tr("Edit"));
    m_undoAction = edit->addAction(tr("Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, [this]() { m_context.session().undo(); });
    m_redoAction = edit->addAction(tr("Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, this, [this]() { m_context.session().redo(); });

    auto* runtime = m_menuBar->addMenu(tr("Runtime"));
    const bool sim = m_context.capabilities().simulation;
    auto* play = runtime->addAction(tr("Play"));
    play->setEnabled(sim);
    connect(play, &QAction::triggered, this, [this]() {
        m_context.session().execute(EditorCommandMessage{"play", ""});
    });
    auto* pause = runtime->addAction(tr("Pause"));
    pause->setEnabled(sim);
    connect(pause, &QAction::triggered, this, [this]() {
        m_context.session().execute(EditorCommandMessage{"pause", ""});
    });
    auto* stop = runtime->addAction(tr("Stop"));
    stop->setEnabled(sim);
    connect(stop, &QAction::triggered, this, [this]() {
        m_context.session().execute(EditorCommandMessage{"stop", ""});
    });
    if (!sim) {
        for (QAction* action : runtime->actions()) {
            action->setToolTip(tr("Runtime backend is unavailable in Editor-Only mode"));
        }
    }

    auto* window = m_menuBar->addMenu(tr("Window"));
    window->addAction(tr("Reset Layout"), this, []() {});
}

void EditorWindow::createTitleBar()
{
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("editorTitleBar"));
    setMenuWidget(m_titleBar);
    m_titleBar->installEventFilter(this);

    auto* titleBarLayout = new QHBoxLayout(m_titleBar);
    titleBarLayout->setContentsMargins(10, 2, 0, 2);
    titleBarLayout->setSpacing(4);

    m_menuBar = new QMenuBar(m_titleBar);
    m_menuBar->installEventFilter(this);
    titleBarLayout->addWidget(m_menuBar);

    titleBarLayout->addStretch();

    auto* minButton = new QToolButton(m_titleBar);
    minButton->setObjectName(QStringLiteral("windowMinButton"));
    minButton->setText(QStringLiteral("—"));
    minButton->setToolTip(tr("Minimize"));
    connect(minButton, &QToolButton::clicked, this, &QWidget::showMinimized);
    titleBarLayout->addWidget(minButton);

    m_maxButton = new QToolButton(m_titleBar);
    m_maxButton->setObjectName(QStringLiteral("windowMaxButton"));
    m_maxButton->setText(QStringLiteral("□"));
    m_maxButton->setToolTip(tr("Maximize"));
    connect(m_maxButton, &QToolButton::clicked, this, &EditorWindow::toggleMaximize);
    titleBarLayout->addWidget(m_maxButton);

    auto* closeButton = new QToolButton(m_titleBar);
    closeButton->setObjectName(QStringLiteral("windowCloseButton"));
    closeButton->setText(QStringLiteral("✕"));
    closeButton->setToolTip(tr("Close"));
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);
    titleBarLayout->addWidget(closeButton);
}

void EditorWindow::createToolbar()
{
    m_editorToolbar = addToolBar(tr("Tools"));
    m_editorToolbar->setObjectName(QStringLiteral("editorToolbar"));
    m_editorToolbar->setMovable(false);

    auto* brand = new QLabel(QApplication::applicationName(), m_editorToolbar);
    brand->setObjectName(QStringLiteral("editorBrand"));
    brand->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_editorToolbar->addWidget(brand);

    const bool sim = m_context.capabilities().simulation;
    struct RuntimeButton {
        QString tooltip;
        QString icon;
        const char* command;
    };
    const RuntimeButton runtimeButtons[] = {
        {tr("Play"), QStringLiteral("PlayButton.png"), "play"},
        {tr("Pause"), QStringLiteral("PauseButton.png"), "pause"},
        {tr("Stop"), QStringLiteral("StopButton.png"), "stop"},
    };
    for (const RuntimeButton& runtimeButton : runtimeButtons) {
        auto* button = new QToolButton(m_editorToolbar);
        button->setObjectName(QStringLiteral("runtimeToolButton"));
        button->setIcon(editorButtonIcon(runtimeButton.icon));
        button->setIconSize(QSize(16, 16));
        button->setToolTip(runtimeButton.tooltip);
        button->setEnabled(sim);
        if (!sim) {
            button->setToolTip(tr("Runtime scene tools are unavailable in Editor-Only mode"));
        }
        connect(button, &QToolButton::clicked, this, [this, command = runtimeButton.command]() {
            m_context.session().execute(EditorCommandMessage{command, ""});
        });
        m_editorToolbar->addWidget(button);
    }

    auto* trailingSpacer = new QWidget(m_editorToolbar);
    trailingSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    trailingSpacer->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_editorToolbar->addWidget(trailingSpacer);
}

void EditorWindow::toggleMaximize()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void EditorWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && m_maxButton) {
        m_maxButton->setText(isMaximized() ? QStringLiteral("▣") : QStringLiteral("□"));
    }
    QMainWindow::changeEvent(event);
}

bool EditorWindow::eventFilter(QObject* watched, QEvent* event)
{
    const bool dragSurface = (watched == m_titleBar);
    const bool menuSurface = qobject_cast<QMenuBar*>(watched) != nullptr;
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const bool draggable = dragSurface
            || (menuSurface && !overMenuAction(qobject_cast<QMenuBar*>(watched), mouse->pos()));
        if (draggable && mouse->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemMove();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const bool draggable = dragSurface
            || (menuSurface && !overMenuAction(qobject_cast<QMenuBar*>(watched), mouse->pos()));
        if (draggable && mouse->button() == Qt::LeftButton) {
            toggleMaximize();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

bool EditorWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef _WIN32
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            const int x = GET_X_LPARAM(msg->lParam);
            const int y = GET_Y_LPARAM(msg->lParam);
            const QPoint pos = mapFromGlobal(QPoint(x, y));
            const int border = 6;
            const bool left = pos.x() < border;
            const bool right = pos.x() >= width() - border;
            const bool top = pos.y() < border;
            const bool bottom = pos.y() >= height() - border;
            if (top && left) { *result = HTTOPLEFT; return true; }
            if (top && right) { *result = HTTOPRIGHT; return true; }
            if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
            if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
            if (left) { *result = HTLEFT; return true; }
            if (right) { *result = HTRIGHT; return true; }
            if (top) { *result = HTTOP; return true; }
            if (bottom) { *result = HTBOTTOM; return true; }
            const bool inTopArea = (m_titleBar && m_titleBar->geometry().contains(pos));
            if (inTopArea && !overInteractiveChild(pos)) {
                *result = HTCAPTION;
                return true;
            }
        } else if (msg->message == WM_GETMINMAXINFO) {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
            RECT work{0, 0, 0, 0};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
            mmi->ptMaxPosition.x = work.left;
            mmi->ptMaxPosition.y = work.top;
            mmi->ptMaxSize.x = work.right - work.left;
            mmi->ptMaxSize.y = work.bottom - work.top;
            *result = 0;
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool EditorWindow::overInteractiveChild(const QPoint& pos) const
{
    QWidget* child = childAt(pos);
    while (child && child != this) {
        if (qobject_cast<QAbstractButton*>(child)) {
            return true;
        }
        if (auto* menuBar = qobject_cast<QMenuBar*>(child)) {
            return overMenuAction(menuBar, menuBar->mapFrom(this, pos));
        }
        child = child->parentWidget();
    }
    return false;
}

bool EditorWindow::overMenuAction(QMenuBar* menuBar, const QPoint& localPos) const
{
    for (QAction* action : menuBar->actions()) {
        if (menuBar->actionGeometry(action).contains(localPos)) {
            return true;
        }
    }
    return false;
}


void EditorWindow::createDocks()
{
    auto* sceneDock = new ads::CDockWidget(QStringLiteral("Scene"));
    sceneDock->setObjectName(QStringLiteral("Scene"));
    m_sceneDock = sceneDock;
    auto* sceneBody = new QWidget(sceneDock);
    auto* sceneLayout = new QVBoxLayout(sceneBody);
    sceneLayout->setContentsMargins(0, 0, 0, 0);
    sceneLayout->setSpacing(0);

    auto* sceneToolbar = new QToolBar(sceneBody);
    sceneToolbar->setObjectName(QStringLiteral("sceneToolbar"));
    sceneToolbar->setMovable(false);
    sceneToolbar->setIconSize(QSize(16, 16));
    sceneToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    auto* toolGroup = new QActionGroup(sceneToolbar);
    toolGroup->setExclusive(true);
    struct SceneTool {
        QString tooltip;
        QString icon;
        const char* mode;
    };
    const SceneTool sceneTools[] = {
        {tr("Hand"), QStringLiteral("hand.svg"), "none"},
        {tr("Move"), QStringLiteral("move.svg"), "translate"},
        {tr("Rotate"), QStringLiteral("rotate.svg"), "rotate"},
        {tr("Scale"), QStringLiteral("scale.svg"), "scale"},
    };
    const bool sim = m_context.capabilities().simulation;
    for (const SceneTool& sceneTool : sceneTools) {
        auto* action = sceneToolbar->addAction(editorToolIcon(sceneTool.icon), QString());
        action->setCheckable(true);
        action->setEnabled(sim);
        action->setToolTip(sceneTool.tooltip);
        toolGroup->addAction(action);
        if (sceneTool.mode[0] == 'n') action->setChecked(true);
        connect(action, &QAction::triggered, this, [this, mode = sceneTool.mode]() {
            m_context.session().execute(EditorCommandMessage{"gizmo_mode", mode});
        });
    }
    sceneLayout->addWidget(sceneToolbar);

    m_sceneSurface = new SceneSurface(m_context, sceneBody);
    sceneLayout->addWidget(m_sceneSurface, 1);
    sceneDock->setWidget(sceneBody);
    // Scene is the single central work area. Tool panels may be moved or
    // pinned, but the scene itself must never turn into a floating island.
    sceneDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
    sceneDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
    sceneDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
    sceneDock->setFeature(ads::CDockWidget::DockWidgetPinnable, false);
    m_dockManager->addDockWidget(ads::CenterDockWidgetArea, sceneDock);
}

void EditorWindow::createPanels()
{
    auto* hierarchy = new ads::CDockWidget(tr("Hierarchy"));
    hierarchy->setObjectName(QStringLiteral("Hierarchy"));
    m_hierarchy = new HierarchyPanel(m_context, hierarchy);
    hierarchy->setWidget(m_hierarchy);
    hierarchy->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::LeftDockWidgetArea, hierarchy);

    auto* inspector = new ads::CDockWidget(tr("Inspector"));
    inspector->setObjectName(QStringLiteral("Inspector"));
    m_inspector = new InspectorPanel(m_context, inspector);
    inspector->setWidget(m_inspector);
    inspector->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, inspector);

    auto* project = new ads::CDockWidget(tr("Project"));
    project->setObjectName(QStringLiteral("Project"));
    m_projectPanel = new ProjectPanel(m_context, project);
    project->setWidget(m_projectPanel);
    project->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, project, hierarchy->dockAreaWidget());

    auto* console = new ads::CDockWidget(tr("Console"));
    console->setObjectName(QStringLiteral("Console"));
    m_console = new QListWidget(console);
    m_console->addItem(tr("NullEditorBackend active"));
    m_console->addItem(tr("DodoeRuntime, renderer and runtime window are not loaded"));
    console->setWidget(m_console);
    console->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, console, m_sceneDock->dockAreaWidget());

    auto* terminal = new ads::CDockWidget(tr("Terminal"));
    terminal->setObjectName(QStringLiteral("Terminal"));
    terminal->setWidget(unavailablePanel(tr("Terminal unavailable"),
        tr("Runtime command services are disabled in Editor-Only mode."), terminal));
    terminal->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, terminal, console->dockAreaWidget());

    auto* history = new ads::CDockWidget(tr("History"));
    history->setObjectName(QStringLiteral("History"));
    m_historyPanel = new HistoryPanel(m_context, history);
    history->setWidget(m_historyPanel);
    history->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, history, console->dockAreaWidget());

    // Establish the single-Scene authoring layout before the user resizes any dock.
    m_dockManager->setSplitterSizes(inspector->dockAreaWidget(), QList<int>{260, 820, 320});
    m_dockManager->setSplitterSizes(hierarchy->dockAreaWidget(), QList<int>{610, 240});
    m_dockManager->setSplitterSizes(console->dockAreaWidget(), QList<int>{610, 240});
}

void EditorWindow::startSafePointTimer()
{
    m_safePointTimer = new QTimer(this);
    m_safePointTimer->setInterval(16);
    connect(m_safePointTimer, &QTimer::timeout, this, [this]() {
        m_context.session().tick();
    });
    m_safePointTimer->start();
}

void EditorWindow::refreshUndoRedoActions()
{
    if (m_undoAction) m_undoAction->setEnabled(m_context.session().history().canUndo());
    if (m_redoAction) m_redoAction->setEnabled(m_context.session().history().canRedo());
}

bool EditorWindow::enterWorkspace(const QString& projectPath)
{
    ProjectDescriptor project;
    const QFileInfo projectInfo(projectPath);
    project.rootPath = projectInfo.isDir()
        ? projectPath.toStdString()
        : projectInfo.absolutePath().toStdString();
    if (projectInfo.isFile()) {
        project.projectFile = projectPath.toStdString();
    }
    if (!m_context.session().openProject(project)) {
        return false;
    }
    m_context.resources().setProjectRoot(std::filesystem::path(project.rootPath));
    if (m_console) {
        m_console->addItem(QString::fromStdString(m_context.diagnostic()));
    }
    if (m_projectPanel) {
        m_projectPanel->refresh();
    }
    if (m_sceneSurface) {
        m_sceneSurface->attach();
    }
    return true;
}

void EditorWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
#ifdef _WIN32
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    static HMODULE dwmModule = LoadLibraryW(L"dwmapi.dll");
    if (dwmModule) {
        auto setWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
            GetProcAddress(dwmModule, "DwmSetWindowAttribute"));
        if (setWindowAttribute) {
            const DWORD cornerPreference = 2;
            setWindowAttribute(reinterpret_cast<HWND>(winId()), 33, &cornerPreference, sizeof(cornerPreference));
        }
    }
#endif
}

void EditorWindow::closeEvent(QCloseEvent* event)
{
    if (m_closed) {
        event->accept();
        return;
    }
    m_closed = true;
    if (m_safePointTimer) {
        m_safePointTimer->stop();
    }
    m_context.session().shutdown();
    QMainWindow::closeEvent(event);
}

} // namespace cakery
