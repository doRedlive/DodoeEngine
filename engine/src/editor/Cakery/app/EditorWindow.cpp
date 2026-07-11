// do@Redlive

#include "EditorWindow.h"
#include "LayoutManager.h"
#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"
#include "framework/document/SceneDocument.h"
#include "framework/playmode/PlayModeController.h"
#include "framework/gizmo/GizmoService.h"
#include "framework/camera/EditorCamera.h"
#include "framework/viewport/ViewportService.h"
#include "Cakery/panels/ScenePanel.h"
#include "Cakery/panels/GamePanel.h"
#include "Cakery/panels/HierarchyPanel.h"
#include "Cakery/panels/InspectorPanel.h"
#include "Cakery/panels/ConsolePanel.h"
#include "Cakery/panels/ProjectPanel.h"
#include "Cakery/panels/TerminalPanel.h"

#include "runtime/core/project/project.h"
#include "runtime/function/world/world.h"
#include "runtime/function/log/log_system.h"

#include <DockManager.h>
#include <DockWidget.h>
#include <DockAreaWidget.h>

#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QLabel>
#include <QStatusBar>
#include <QAction>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

namespace cakery {

EditorWindow::EditorWindow(EditorContext& ctx, QWidget* parent)
    : QMainWindow(parent)
    , m_ctx(ctx)
{
    setWindowTitle("Cakery");
    resize(1440, 900);
    setMinimumSize(800, 600);
    setDockNestingEnabled(true);

    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DisableStylesheet, true);
    m_dockManager = new ads::CDockManager(this);

    setupDockWidgets();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    connectActions();
    setupFrameTimer();

    m_layoutManager = std::make_unique<LayoutManager>(m_dockManager);
}

EditorWindow::~EditorWindow()
{
    m_frameTimer.stop();
    m_dockManager->deleteLater();
}

void EditorWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu(tr("File"));
    fileMenu->addAction(tr("New Scene"), this, &EditorWindow::onNewScene, QKeySequence("Ctrl+N"));
    fileMenu->addAction(tr("Open Scene..."), this, &EditorWindow::onOpenScene, QKeySequence("Ctrl+O"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Save"), this, &EditorWindow::onSaveScene, QKeySequence("Ctrl+S"));
    fileMenu->addAction(tr("Save As..."), this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Save Scene As"));
        if (!path.isEmpty()) m_ctx.document().saveAs(path.toStdString());
    });

    auto* editMenu = menuBar()->addMenu(tr("Edit"));
    m_actionUndo = editMenu->addAction(tr("Undo"), this, &EditorWindow::onUndo, QKeySequence("Ctrl+Z"));
    m_actionRedo = editMenu->addAction(tr("Redo"), this, &EditorWindow::onRedo, QKeySequence("Ctrl+Y"));

    menuBar()->addMenu(tr("Assets"));
    menuBar()->addMenu(tr("GameObject"));
    menuBar()->addMenu(tr("Component"));

    auto* windowMenu = menuBar()->addMenu(tr("Window"));
    for (auto* dw : m_dockManager->dockWidgetsMap()) {
        windowMenu->addAction(dw->toggleViewAction());
    }
}

void EditorWindow::setupToolBar()
{
    auto* toolbar = addToolBar(tr("Tools"));
    toolbar->setObjectName("mainToolBar");
    toolbar->setMovable(true);

    toolbar->addWidget(new QWidget(this)); // spacer
    toolbar->addWidget(new QWidget(this)); // spacer
    ((QWidget*)nullptr);

    m_btnPlay = new QToolButton(this);
    m_btnPlay->setText("Play");
    m_btnPlay->setToolTip(tr("Play"));
    m_btnPlay->setCheckable(true);
    m_btnPlay->setMinimumWidth(50);
    toolbar->addWidget(m_btnPlay);

    m_btnPause = new QToolButton(this);
    m_btnPause->setText("Pause");
    m_btnPause->setToolTip(tr("Pause"));
    m_btnPause->setCheckable(true);
    m_btnPause->setMinimumWidth(50);
    toolbar->addWidget(m_btnPause);

    m_btnStop = new QToolButton(this);
    m_btnStop->setText("Stop");
    m_btnStop->setToolTip(tr("Stop"));
    m_btnStop->setMinimumWidth(50);
    toolbar->addWidget(m_btnStop);

    toolbar->addSeparator();

    m_toolMove = new QToolButton(this);
    m_toolMove->setText("W");
    m_toolMove->setToolTip(tr("Move (W)"));
    m_toolMove->setCheckable(true);
    m_toolMove->setChecked(true);
    toolbar->addWidget(m_toolMove);

    m_toolRotate = new QToolButton(this);
    m_toolRotate->setText("E");
    m_toolRotate->setToolTip(tr("Rotate (E)"));
    m_toolRotate->setCheckable(true);
    toolbar->addWidget(m_toolRotate);

    m_toolScale = new QToolButton(this);
    m_toolScale->setText("R");
    m_toolScale->setToolTip(tr("Scale (R)"));
    m_toolScale->setCheckable(true);
    toolbar->addWidget(m_toolScale);

    m_toolHand = new QToolButton(this);
    m_toolHand->setText("Q");
    m_toolHand->setToolTip(tr("Hand (Q)"));
    m_toolHand->setCheckable(true);
    toolbar->addWidget(m_toolHand);
}

void EditorWindow::setupDockWidgets()
{
    m_scenePanel = new ScenePanel(m_ctx, this);
    auto* sceneDock = new ads::CDockWidget(tr("Scene"));
    sceneDock->setWidget(m_scenePanel);

    m_gamePanel = new GamePanel(m_ctx, this);
    auto* gameDock = new ads::CDockWidget(tr("Game"));
    gameDock->setWidget(m_gamePanel);

    auto* centralArea = m_dockManager->setCentralWidget(sceneDock);
    m_dockManager->addDockWidget(ads::CenterDockWidgetArea, gameDock, centralArea);

    m_hierarchyPanel = new HierarchyPanel(m_ctx, this);
    auto* hierarchyDock = new ads::CDockWidget(tr("Hierarchy"));
    hierarchyDock->setWidget(m_hierarchyPanel);
    m_dockManager->addDockWidget(ads::LeftDockWidgetArea, hierarchyDock, centralArea);

    m_inspectorPanel = new InspectorPanel(m_ctx, this);
    auto* inspectorDock = new ads::CDockWidget(tr("Inspector"));
    inspectorDock->setWidget(m_inspectorPanel);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, inspectorDock, centralArea);

    m_projectPanel = new ProjectPanel(m_ctx, this);
    auto* projectDock = new ads::CDockWidget(tr("Project"));
    projectDock->setWidget(m_projectPanel);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, projectDock, hierarchyDock->dockAreaWidget());

    m_consolePanel = new ConsolePanel(m_ctx, this);
    auto* consoleDock = new ads::CDockWidget(tr("Console"));
    consoleDock->setWidget(m_consolePanel);

    m_terminalPanel = new TerminalPanel(m_ctx, this);
    auto* terminalDock = new ads::CDockWidget(tr("Terminal"));
    terminalDock->setWidget(m_terminalPanel);

    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, consoleDock, inspectorDock->dockAreaWidget());
    m_dockManager->addDockWidget(ads::CenterDockWidgetArea, terminalDock, consoleDock->dockAreaWidget());
}

void EditorWindow::setupStatusBar()
{
    m_titleLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_titleLabel);

    m_fpsLabel = new QLabel("0 FPS", this);
    m_fpsLabel->setStyleSheet("color: #6272A4; font-size: 12px; padding-right: 8px;");
    statusBar()->addPermanentWidget(m_fpsLabel);
}

void EditorWindow::setupFrameTimer()
{
    m_frameTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_frameTimer, &QTimer::timeout, this, [this]() {
        float dt = static_cast<float>(m_frameClock.restart()) / 1000.0f;
        if (dt > 0.5f) dt = 0.016f;
        m_ctx.tick(dt);
        m_ctx.camera().update(dt);
        m_ctx.gizmos().update();
        m_ctx.viewports().updateAndRenderAll(dt);
        m_scenePanel->update();
        m_gamePanel->update();
    });
    m_frameTimer.start(0);
}

void EditorWindow::connectActions()
{
    connect(m_btnPlay, &QToolButton::clicked, this, &EditorWindow::onPlay);
    connect(m_btnPause, &QToolButton::clicked, this, &EditorWindow::onPause);
    connect(m_btnStop, &QToolButton::clicked, this, &EditorWindow::onStop);

    auto makeToolGroup = [this](QToolButton* btn, GizmoMode mode) {
        connect(btn, &QToolButton::clicked, this, [this, btn, mode]() {
            for (auto* b : {m_toolHand, m_toolMove, m_toolRotate, m_toolScale}) {
                if (b != btn) b->setChecked(false);
            }
            btn->setChecked(true);
            m_ctx.gizmos().setMode(mode);
        });
    };
    makeToolGroup(m_toolHand,  GizmoMode::None);
    makeToolGroup(m_toolMove,  GizmoMode::Translate);
    makeToolGroup(m_toolRotate, GizmoMode::Rotate);
    makeToolGroup(m_toolScale, GizmoMode::Scale);

    m_ctx.document().dirtyChanged.connect([this]() {
        setWindowTitle(QString::fromStdString(m_ctx.document().displayTitle() + " — Cakery"));
    });

    m_ctx.playMode().stateChanged.connect([this](PlayState state) {
        m_isPlaying = (state == PlayState::Playing);
        m_isPaused  = (state == PlayState::Paused);
        m_btnPlay->setChecked(m_isPlaying);
        m_btnPause->setChecked(m_isPaused);
    });
}

void EditorWindow::enterWorkspace(const QString& projectPath)
{
    m_titleLabel->setText("Initializing...");

    float dpr = m_scenePanel->devicePixelRatioF();
    void* hwnd = reinterpret_cast<void*>(m_scenePanel->winId());
    int pixelW = static_cast<int>(m_scenePanel->width() * dpr);
    int pixelH = static_cast<int>(m_scenePanel->height() * dpr);

    EditorBootConfig cfg;
    cfg.projectPath       = projectPath.toStdString();
    cfg.hostWindowHandle  = hwnd;
    cfg.width             = pixelW;
    cfg.height            = pixelH;
    cfg.devicePixelRatio  = dpr;

    bool ok = m_ctx.boot(cfg);
    if (!ok) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to initialize engine."));
        return;
    }

    m_frameClock.start();
    m_titleLabel->setText("Ready");
}

void EditorWindow::closeEvent(QCloseEvent* event)
{
    m_frameTimer.stop();
    if (m_ctx.isBooted()) {
        m_ctx.shutdown();
    }
    QMainWindow::closeEvent(event);
}

void EditorWindow::onPlay()
{
    auto& pm = m_ctx.playMode();
    if (pm.state() == PlayState::Edit) {
        pm.play();
    } else if (pm.state() == PlayState::Playing) {
        pm.stop();
    }
}

void EditorWindow::onPause()
{
    auto& pm = m_ctx.playMode();
    if (pm.state() == PlayState::Playing) {
        pm.pause();
    } else if (pm.state() == PlayState::Paused) {
        pm.resume();
    }
}

void EditorWindow::onStop()
{
    m_ctx.playMode().stop();
}

void EditorWindow::onUndo()     { m_ctx.commands().undo(); }
void EditorWindow::onRedo()     { m_ctx.commands().redo(); }

void EditorWindow::onNewScene() { m_ctx.document().newScene(); }
void EditorWindow::onOpenScene()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Scene"));
    if (!path.isEmpty()) m_ctx.document().openScene(path.toStdString());
}

void EditorWindow::onSaveScene()
{
    if (!m_ctx.document().save()) {
        QString path = QFileDialog::getSaveFileName(this, tr("Save Scene As"));
        if (!path.isEmpty()) m_ctx.document().saveAs(path.toStdString());
    }
}

} // namespace cakery
