#include "MainWindow.h"
#include "services/EngineManager.h"
#include "widgets/SceneWidget.h"
#include "widgets/HierarchyWidget.h"
#include "widgets/InspectorWidget.h"
#include "widgets/ConsoleWidget.h"
#include "widgets/ProjectBrowserWidget.h"

#include "runtime/function/world/world.h"
#include "runtime/core/project/project.h"

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
#include <QComboBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QCoreApplication>
#include <QCloseEvent>

namespace cakery {

static QString resPath(const QString& relative)
{
    QStringList candidates;
    candidates << QCoreApplication::applicationDirPath() + "/resources/" + relative;
    candidates << QCoreApplication::applicationDirPath() + "/../engine/res/" + relative;
    for (const auto& p : candidates) {
        if (QFileInfo::exists(p)) return p;
    }
    return QString();
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("MyGame - Unity-like Editor");
    resize(1440, 900);
    setMinimumSize(800, 600);
    setDockNestingEnabled(true);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DisableStylesheet, true);
    m_dockManager = new ads::CDockManager(this);

    setupDockWidgets();
    setupMenuBar();
    setupMainToolBar();
    setupStatusBar();
    connectSignals();

    auto* centralArea = m_dockManager->setCentralWidget(m_viewportDock);
    centralArea->setAllowedAreas(ads::DockWidgetArea::OuterDockAreas);

    m_dockManager->addDockWidget(ads::LeftDockWidgetArea, m_hierarchyDock, centralArea);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_inspectorDock, centralArea);

    auto* hierarchyArea = m_hierarchyDock->dockAreaWidget();
    auto* inspectorArea = m_inspectorDock->dockAreaWidget();
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_projectDock, hierarchyArea);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_consoleDock, inspectorArea);

    m_dockManager->setSplitterSizes(centralArea, {260, 600, 320});
    m_dockManager->setSplitterSizes(hierarchyArea, {320, 200});
    m_dockManager->setSplitterSizes(inspectorArea, {320, 200});

    auto& engine = EngineManager::getInstance();
    connect(&engine, &EngineManager::fpsUpdated, this, [this](const QString& fps) {
        if (m_fpsLabel) m_fpsLabel->setText(fps);
    });
    connect(&engine, &EngineManager::engineError, this, [this](const QString& msg) {
        m_statusLabel->setText("Error: " + msg);
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_dockManager->deleteLater();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu(tr("File"));
    m_actionNewScene = fileMenu->addAction(tr("New Scene"));
    m_actionNewScene->setShortcut(QKeySequence("Ctrl+N"));
    m_actionOpenScene = fileMenu->addAction(tr("Open Scene..."));
    m_actionOpenScene->setShortcut(QKeySequence("Ctrl+O"));
    fileMenu->addSeparator();
    m_actionSaveScene = fileMenu->addAction(tr("Save"));
    m_actionSaveScene->setShortcut(QKeySequence("Ctrl+S"));
    m_actionSaveSceneAs = fileMenu->addAction(tr("Save As..."));
    m_actionSaveSceneAs->setShortcut(QKeySequence("Ctrl+Shift+S"));

    auto* editMenu = menuBar()->addMenu(tr("Edit"));
    m_actionUndo = editMenu->addAction(tr("Undo"));
    m_actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    m_actionRedo = editMenu->addAction(tr("Redo"));
    m_actionRedo->setShortcut(QKeySequence("Ctrl+Y"));
    editMenu->addSeparator();
    m_actionProjectSettings = editMenu->addAction(tr("Project Settings..."));

    menuBar()->addMenu(tr("Assets"));
    menuBar()->addMenu(tr("GameObject"));
    menuBar()->addMenu(tr("Component"));

    auto* windowMenu = menuBar()->addMenu(tr("Window"));
    windowMenu->addAction(m_viewportDock->toggleViewAction());
    windowMenu->addSeparator();
    windowMenu->addAction(m_hierarchyDock->toggleViewAction());
    windowMenu->addAction(m_inspectorDock->toggleViewAction());
    windowMenu->addAction(m_projectDock->toggleViewAction());
    windowMenu->addAction(m_consoleDock->toggleViewAction());

    menuBar()->addMenu(tr("Help"));
}

void MainWindow::setupMainToolBar()
{
    m_mainToolBar = addToolBar(tr("Playback"));
    m_mainToolBar->setObjectName("mainToolBar");
    m_mainToolBar->setMovable(true);

    auto* leftSpacer = new QWidget(this);
    leftSpacer->setMinimumWidth(120);
    m_mainToolBar->addWidget(leftSpacer);

    m_btnPlay = new QToolButton(this);
    m_btnPlay->setIcon(QIcon(resPath("pictures/Buttons/PlayButton.png")));
    m_btnPlay->setToolTip(tr("Play (Ctrl+P)"));
    m_btnPlay->setCheckable(true);
    m_btnPlay->setMinimumWidth(40);
    m_mainToolBar->addWidget(m_btnPlay);

    m_btnPause = new QToolButton(this);
    m_btnPause->setIcon(QIcon(resPath("pictures/Buttons/PauseButton.png")));
    m_btnPause->setToolTip(tr("Pause (Ctrl+Shift+P)"));
    m_btnPause->setCheckable(true);
    m_btnPause->setMinimumWidth(40);
    m_mainToolBar->addWidget(m_btnPause);

    m_btnStop = new QToolButton(this);
    m_btnStop->setIcon(QIcon(resPath("pictures/Buttons/StopButton.png")));
    m_btnStop->setToolTip(tr("Stop"));
    m_btnStop->setMinimumWidth(40);
    m_mainToolBar->addWidget(m_btnStop);

    auto* rightSpacer = new QWidget(this);
    rightSpacer->setMinimumWidth(120);
    m_mainToolBar->addWidget(rightSpacer);
}

void MainWindow::setupDockWidgets()
{
    m_hierarchyWidget = new HierarchyWidget(this);
    m_hierarchyDock = new ads::CDockWidget(tr("  Hierarchy"));
    m_hierarchyDock->setWidget(m_hierarchyWidget);

    m_inspectorWidget = new InspectorWidget(this);
    m_inspectorDock = new ads::CDockWidget(tr("  Inspector"));
    m_inspectorDock->setWidget(m_inspectorWidget);

    m_projectWidget = new ProjectBrowserWidget(this);
    m_projectDock = new ads::CDockWidget(tr("  Project"));
    m_projectDock->setWidget(m_projectWidget);

    m_consoleWidget = new ConsoleWidget(this);
    m_consoleDock = new ads::CDockWidget(tr("  Console"));
    m_consoleDock->setWidget(m_consoleWidget);

    auto* sceneTab = new QWidget();
    sceneTab->setObjectName("tabScene");
    auto* sceneLayout = new QVBoxLayout(sceneTab);
    sceneLayout->setSpacing(2);
    sceneLayout->setContentsMargins(4, 4, 4, 4);

    auto* sceneToolBar = new QHBoxLayout();
    sceneToolBar->setSpacing(3);

    m_toolHand = new QToolButton();
    m_toolHand->setText("Hand");
    m_toolHand->setToolTip(tr("View Pan Tool (Q)"));
    m_toolHand->setCheckable(true);
    sceneToolBar->addWidget(m_toolHand);

    m_toolMove = new QToolButton();
    m_toolMove->setText("Move");
    m_toolMove->setToolTip(tr("Move Tool (W)"));
    m_toolMove->setCheckable(true);
    m_toolMove->setChecked(true);
    sceneToolBar->addWidget(m_toolMove);

    m_toolRotate = new QToolButton();
    m_toolRotate->setText("Rotate");
    m_toolRotate->setToolTip(tr("Rotate Tool (E)"));
    m_toolRotate->setCheckable(true);
    sceneToolBar->addWidget(m_toolRotate);

    m_toolScale = new QToolButton();
    m_toolScale->setText("Scale");
    m_toolScale->setToolTip(tr("Scale Tool (R)"));
    m_toolScale->setCheckable(true);
    sceneToolBar->addWidget(m_toolScale);

    m_toolRect = new QToolButton();
    m_toolRect->setText("Rect");
    m_toolRect->setToolTip(tr("Rect Transform Tool (T)"));
    m_toolRect->setCheckable(true);
    sceneToolBar->addWidget(m_toolRect);

    auto* toolSep = new QFrame();
    toolSep->setFrameShape(QFrame::VLine);
    toolSep->setStyleSheet("color: #44475A;");
    sceneToolBar->addWidget(toolSep);

    m_shadingCombo = new QComboBox();
    m_shadingCombo->addItems({"Shaded", "Wireframe", "Shaded Wireframe"});
    sceneToolBar->addWidget(m_shadingCombo);

    m_btn2D = new QToolButton();
    m_btn2D->setText("2D");
    m_btn2D->setCheckable(true);
    m_btn2D->setToolTip(tr("2D View"));
    sceneToolBar->addWidget(m_btn2D);

    m_btnGizmos = new QToolButton();
    m_btnGizmos->setText("Gizmos");
    m_btnGizmos->setCheckable(true);
    m_btnGizmos->setChecked(true);
    sceneToolBar->addWidget(m_btnGizmos);

    sceneToolBar->addStretch(1);

    m_sceneSearch = new QLineEdit();
    m_sceneSearch->setPlaceholderText(tr("Search..."));
    m_sceneSearch->setMaximumWidth(180);
    sceneToolBar->addWidget(m_sceneSearch);

    sceneLayout->addLayout(sceneToolBar);

    m_sceneWidget = new SceneWidget(this);
    m_sceneWidget->setObjectName("sceneViewport");
    m_sceneWidget->setMinimumSize(400, 300);
    sceneLayout->addWidget(m_sceneWidget, 1);

    m_viewportDock = new ads::CDockWidget(tr("Scene"));
    m_viewportDock->setWidget(sceneTab);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_statusLabel);

    m_fpsLabel = new QLabel("0 FPS", this);
    m_fpsLabel->setStyleSheet("color: #6272A4; font-size: 12px; padding-right: 8px;");
    statusBar()->addPermanentWidget(m_fpsLabel);
}

void MainWindow::connectSignals()
{
    connect(m_hierarchyWidget, &HierarchyWidget::entitySelected,
            m_inspectorWidget, &InspectorWidget::onEntitySelected);
    connect(m_hierarchyWidget, &HierarchyWidget::entityDeselected,
            m_inspectorWidget, &InspectorWidget::onEntityDeselected);

    auto makeToolGroup = [this](QToolButton* btn) {
        connect(btn, &QToolButton::clicked, this, [this, btn]() {
            for (auto* b : {m_toolHand, m_toolMove, m_toolRotate, m_toolScale, m_toolRect}) {
                if (b != btn) b->setChecked(false);
            }
            btn->setChecked(true);
        });
    };
    makeToolGroup(m_toolHand);
    makeToolGroup(m_toolMove);
    makeToolGroup(m_toolRotate);
    makeToolGroup(m_toolScale);
    makeToolGroup(m_toolRect);

    connect(m_btnPlay, &QToolButton::clicked, this, &MainWindow::onPlay);
    connect(m_btnPause, &QToolButton::clicked, this, &MainWindow::onPause);
    connect(m_btnStop, &QToolButton::clicked, this, &MainWindow::onStop);
}

void MainWindow::enterWorkspace(const QString& projectPath)
{
    LOG_INFO("[MainWindow] Entering workspace: {}", projectPath.toStdString());
    m_statusLabel->setText("Initializing engine...");

    float dpr = m_sceneWidget->devicePixelRatioF();
    void* hwnd = reinterpret_cast<void*>(m_sceneWidget->winId());
    int pixelW = static_cast<int>(m_sceneWidget->width() * dpr);
    int pixelH = static_cast<int>(m_sceneWidget->height() * dpr);

    bool ok = EngineManager::getInstance().initialize(projectPath.toStdString(), hwnd, pixelW, pixelH);
    if (!ok) {
        QMessageBox::critical(this, tr("Engine Error"), tr("Failed to initialize the engine."));
        return;
    }

    m_statusLabel->setText("Workspace ready");
    m_workspaceActive = true;

    auto assetDir = dodoe::Project::AssetDirectory();
    m_projectWidget->setBasePath(QString::fromStdWString(assetDir.wstring()));

    emit workspaceEntered(projectPath);
}

void MainWindow::onPlay()
{
    auto* world = EngineManager::getInstance().getWorld();
    if (!world) return;

    if (m_isPlaying && !m_isPaused) {
        world->setState(dodoe::WorldState::Simulation);
        m_isPlaying = false;
        m_isPaused = false;
        m_btnPlay->setChecked(false);
        m_btnPause->setChecked(false);
        m_statusLabel->setText("Stopped");
    } else {
        world->setState(dodoe::WorldState::Runtime);
        m_isPlaying = true;
        m_isPaused = false;
        m_btnPlay->setChecked(true);
        m_btnPause->setChecked(false);
        m_statusLabel->setText("Playing");
    }
}

void MainWindow::onPause()
{
    if (!m_isPlaying) return;
    auto* world = EngineManager::getInstance().getWorld();
    if (!world) return;

    m_isPaused = !m_isPaused;
    world->setState(m_isPaused ? dodoe::WorldState::Pause : dodoe::WorldState::Runtime);
    m_btnPause->setChecked(m_isPaused);
    m_statusLabel->setText(m_isPaused ? "Paused" : "Playing");
}

void MainWindow::onStop()
{
    if (!m_isPlaying) return;
    auto* world = EngineManager::getInstance().getWorld();
    if (world) {
        world->setState(dodoe::WorldState::Simulation);
        m_isPlaying = false;
        m_isPaused = false;
        m_btnPlay->setChecked(false);
        m_btnPause->setChecked(false);
        m_statusLabel->setText("Stopped");
    }
}

} // namespace cakery
