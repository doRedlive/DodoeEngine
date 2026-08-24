// do@Redlive

#include "EditorWindow.h"

#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/panels/HierarchyPanel.h"
#include "cakery/ui/panels/InspectorPanel.h"
#include "cakery/ui/panels/ProjectPanel.h"

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

#include <QApplication>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
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
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

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
    resize(1440, 900);
    setMinimumSize(900, 600);

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

    createMenus();
    createToolbar();
    createDocks();
    createPanels();
    startSafePointTimer();

    m_context.session().history().subscribe([this]() { refreshUndoRedoActions(); });
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
    auto* file = menuBar()->addMenu(tr("File"));
    auto* open = file->addAction(tr("Open Project..."));
    connect(open, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, tr("Open Project"));
        if (!path.isEmpty()) enterWorkspace(path);
    });

    auto* save = file->addAction(tr("Save Scene"));
    save->setShortcut(QKeySequence::Save);
    connect(save, &QAction::triggered, this, [this]() {
        if (!m_context.session().documentModel().hasDocument()) {
            statusBar()->showMessage(tr("No scene document is open"), 2500);
            return;
        }
        if (m_context.session().saveDocument(std::string())) {
            statusBar()->showMessage(tr("Scene saved"), 2500);
        } else {
            statusBar()->showMessage(tr("Scene could not be saved"), 2500);
        }
    });

    auto* saveAs = file->addAction(tr("Save Scene As..."));
    connect(saveAs, &QAction::triggered, this, [this]() {
        if (!m_context.session().documentModel().hasDocument()) {
            statusBar()->showMessage(tr("No scene document is open"), 2500);
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Save Scene As"), QString(), tr("Dodoe Scene (*.doscn)"));
        if (!path.isEmpty()) {
            m_context.session().saveDocument(path.toStdString());
            statusBar()->showMessage(tr("Scene saved"), 2500);
        }
    });

    file->addSeparator();
    file->addAction(tr("Close"), this, &QWidget::close);

    auto* edit = menuBar()->addMenu(tr("Edit"));
    m_undoAction = edit->addAction(tr("Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, [this]() { m_context.session().undo(); });
    m_redoAction = edit->addAction(tr("Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, this, [this]() { m_context.session().redo(); });

    auto* runtime = menuBar()->addMenu(tr("Runtime"));
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

    auto* window = menuBar()->addMenu(tr("Window"));
    window->addAction(tr("Reset Layout"), this, [this]() {
        statusBar()->showMessage(tr("Single Scene layout is fixed in Editor-Only mode"), 2500);
    });
}

void EditorWindow::createToolbar()
{
    auto* toolbar = addToolBar(tr("Editor"));
    toolbar->setObjectName(QStringLiteral("editorToolbar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(16, 16));

    auto* leadingSpacer = new QWidget(toolbar);
    leadingSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(leadingSpacer);

    const bool sim = m_context.capabilities().simulation;
    struct RuntimeAction {
        QString tooltip;
        QString icon;
        const char* command;
    };
    const RuntimeAction runtimeActions[] = {
        {tr("Play"), QStringLiteral("PlayButton.png"), "play"},
        {tr("Pause"), QStringLiteral("PauseButton.png"), "pause"},
        {tr("Stop"), QStringLiteral("StopButton.png"), "stop"},
    };
    for (const RuntimeAction& runtimeAction : runtimeActions) {
        auto* action = toolbar->addAction(editorButtonIcon(runtimeAction.icon), QString());
        action->setToolTip(runtimeAction.tooltip);
        action->setEnabled(sim);
        if (!sim) {
            action->setToolTip(tr("Runtime scene tools are unavailable in Editor-Only mode"));
        }
        connect(action, &QAction::triggered, this, [this, command = runtimeAction.command]() {
            m_context.session().execute(EditorCommandMessage{command, ""});
        });
    }

    auto* trailingSpacer = new QWidget(toolbar);
    trailingSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(trailingSpacer);

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

void EditorWindow::enterWorkspace(const QString& projectPath)
{
    ProjectDescriptor project;
    project.rootPath = QFileInfo(projectPath).isDir()
        ? projectPath.toStdString()
        : QFileInfo(projectPath).absolutePath().toStdString();
    if (!m_context.session().openProject(project)) {
        statusBar()->showMessage(tr("Project could not be opened"), 3000);
        return;
    }
    m_context.resources().setProjectRoot(std::filesystem::path(project.rootPath));
    if (m_console) {
        m_console->addItem(QString::fromStdString(m_context.diagnostic()));
    }
    if (m_projectPanel) {
        m_projectPanel->refresh();
    }
    statusBar()->showMessage(tr("Project opened"), 2500);
    if (m_sceneSurface) {
        m_sceneSurface->attach();
    }
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
