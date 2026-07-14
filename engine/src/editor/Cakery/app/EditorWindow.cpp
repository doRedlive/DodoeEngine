// do@Redlive

#include "EditorWindow.h"
#include "LayoutManager.h"
#include "PanelRegistry.h"
#include "framework/EditorContext.h"
#include "framework/config/EditorConfig.h"
#include "framework/command/CommandStack.h"
#include "framework/document/SceneDocument.h"
#include "framework/playmode/PlayModeController.h"
#include "framework/gizmo/GizmoService.h"
#include "framework/camera/EditorCamera.h"
#include "framework/viewport/ViewportService.h"
#include "framework/command/commands/CreateEntityCommand.h"
#include "framework/command/commands/DeleteEntityCommand.h"
#include "framework/console/CommandRegistry.h"
#include "Cakery/panels/ScenePanel.h"
#include "Cakery/panels/GamePanel.h"
#include "Cakery/panels/HierarchyPanel.h"
#include "Cakery/panels/InspectorPanel.h"
#include "Cakery/panels/ConsolePanel.h"
#include "Cakery/panels/ProjectPanel.h"
#include "Cakery/panels/TerminalPanel.h"
#include "Cakery/panels/TilePalettePanel.h"

#include "runtime/core/project/project.h"
#include "runtime/core/utils/json.h"
#include "runtime/function/world/world.h"
#include "runtime/function/log/log_system.h"

#include <DockManager.h>
#include <DockWidget.h>
#include <DockAreaWidget.h>

#include <functional>
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

    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasUndockButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHideDisabledButtons, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaDynamicTabsMenuButtonVisibility, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::TabCloseButtonIsTabBarScrollButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::EqualSplitOnInsertion, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetIcon, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetTitle, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DragPreviewIsDynamic, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DragPreviewShowsContentPixmap, false);
    m_dockManager = new ads::CDockManager(this);

    PanelRegistry::self().registerBuiltinPanels();
    m_layoutManager = std::make_unique<LayoutManager>(m_dockManager, m_ctx);
    setupDockWidgets();

    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    connectActions();
    setupFrameTimer();
}

EditorWindow::~EditorWindow()
{
    m_frameTimer.stop();
    m_dockManager->deleteLater();
}

void EditorWindow::setupMenuBar()
{
    menuBar()->clear();

    auto& menusJson = EditorConfig::self().menusJson();
    std::unordered_map<std::string, QMenu*> menuMap;

    std::function<QMenu*(const std::string&)> getOrCreateMenu = [&](const std::string& path) -> QMenu* {
        auto it = menuMap.find(path);
        if (it != menuMap.end()) return it->second;

        auto sep = path.rfind('/');
        if (sep == std::string::npos) {
            auto* menu = menuBar()->addMenu(QString::fromStdString(path));
            menuMap[path] = menu;
            return menu;
        }

        std::string parentPath = path.substr(0, sep);
        std::string name = path.substr(sep + 1);
        QMenu* parent = getOrCreateMenu(parentPath);
        auto* menu = parent->addMenu(QString::fromStdString(name));
        menuMap[path] = menu;
        return menu;
    };

    if (menusJson.contains("menus") && menusJson["menus"].is_array()) {
        for (auto& item : menusJson["menus"]) {
            std::string path = item.value("path", "");
            std::string command = item.value("command", "");
            std::string shortcut = item.value("shortcut", "");
            dodoe::Json args = item.value("args", dodoe::Json::object());

            auto sep = path.rfind('/');
            if (sep == std::string::npos) continue;
            std::string menuPath = path.substr(0, sep);
            std::string actionName = path.substr(sep + 1);

            QMenu* parent = getOrCreateMenu(menuPath);

            QAction* action = parent->addAction(QString::fromStdString(actionName));
            if (!shortcut.empty()) {
                action->setShortcut(QKeySequence(QString::fromStdString(shortcut)));
            }
            QObject::connect(action, &QAction::triggered, this, [this, command, args]() {
                CommandRegistry::self().executeStructured(m_ctx, command, args);
            });
        }
    }

    if (!menuMap.count("Edit")) {
        menuBar()->addMenu(tr("Edit"));
    }

    auto* windowMenu = menuBar()->addMenu(tr("Window"));
    auto* layoutMenu = windowMenu->addMenu(tr("Layouts"));
    auto layouts = m_layoutManager->namedLayouts();
    for (auto& name : layouts) {
        layoutMenu->addAction(name, this, [this, name]() {
            m_layoutManager->loadNamed(name);
        });
    }
    layoutMenu->addSeparator();
    layoutMenu->addAction(tr("Save Layout..."), this, [this]() {
        m_layoutManager->saveNamed("Custom");
    });
    layoutMenu->addAction(tr("Reset to Default"), this, [this]() {
        m_layoutManager->applyDefault();
    });
    windowMenu->addSeparator();
    for (auto* dw : m_dockManager->dockWidgetsMap()) {
        windowMenu->addAction(dw->toggleViewAction());
    }
}

void EditorWindow::setupToolBar()
{
    auto* toolbar = addToolBar(tr("Tools"));
    toolbar->setObjectName(QStringLiteral("mainToolBar"));
    toolbar->setMovable(true);

    auto& menusJson = EditorConfig::self().menusJson();
    if (!menusJson.contains("toolbar") || !menusJson["toolbar"].is_array()) return;

    std::unordered_map<std::string, std::vector<QToolButton*>> groups;

    for (auto& item : menusJson["toolbar"]) {
        if (item.value("separator", false)) {
            toolbar->addSeparator();
            continue;
        }

        std::string id = item.value("id", "");
        std::string command = item.value("command", "");
        std::string shortcut = item.value("shortcut", "");
        std::string group = item.value("group", "");
        bool checkable = item.value("checkable", false);
        dodoe::Json args = item.value("args", dodoe::Json::object());

        auto* btn = new QToolButton(this);
        btn->setText(QString::fromStdString(id));
        btn->setToolTip(QString::fromStdString(id));
        if (!shortcut.empty()) {
            btn->setToolTip(QString("%1 (%2)").arg(QString::fromStdString(id), QString::fromStdString(shortcut)));
        }
        btn->setCheckable(checkable);
        btn->setMinimumWidth(50);
        toolbar->addWidget(btn);

        if (!group.empty()) {
            groups[group].push_back(btn);
        }

        QObject::connect(btn, &QToolButton::clicked, this, [this, btn, command, args, checkable, &groups, group]() {
            if (checkable && !group.empty()) {
                auto it = groups.find(group);
                if (it != groups.end()) {
                    for (auto* b : it->second) {
                        if (b != btn) b->setChecked(false);
                    }
                    btn->setChecked(true);
                }
            }
            CommandRegistry::self().executeStructured(m_ctx, command, args);
        });

        if (id == "Play")  m_btnPlay = btn;
        if (id == "Pause") m_btnPause = btn;
        if (id == "Stop")  m_btnStop = btn;
        if (id == "Hand")   m_toolHand = btn;
        if (id == "Move")   m_toolMove = btn;
        if (id == "Rotate") m_toolRotate = btn;
        if (id == "Scale")  m_toolScale = btn;
    }
}

void EditorWindow::setupDockWidgets()
{
    m_layoutManager->applyDefault();

    auto findPanel = [this](const char* dockName) {
        auto* dw = m_dockManager->findDockWidget(QString(dockName));
        if (dw) return qobject_cast<Panel*>(dw->widget());
        return static_cast<Panel*>(nullptr);
    };

    m_scenePanel      = qobject_cast<ScenePanel*>(findPanel("Scene"));
    m_gamePanel       = qobject_cast<GamePanel*>(findPanel("Game"));
    m_hierarchyPanel  = qobject_cast<HierarchyPanel*>(findPanel("Hierarchy"));
    m_inspectorPanel  = qobject_cast<InspectorPanel*>(findPanel("Inspector"));
    m_projectPanel    = qobject_cast<ProjectPanel*>(findPanel("Project"));
    m_consolePanel    = qobject_cast<ConsolePanel*>(findPanel("Console"));
    m_terminalPanel   = qobject_cast<TerminalPanel*>(findPanel("Terminal"));
    m_tilePalettePanel= qobject_cast<TilePalettePanel*>(findPanel("TilePalette"));

    if (auto* sceneDw = m_dockManager->findDockWidget("Scene")) {
        sceneDw->setFeature(ads::CDockWidget::DockWidgetClosable, false);
        sceneDw->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
        sceneDw->setFeature(ads::CDockWidget::DockWidgetMovable, false);
    }
}

void EditorWindow::setupStatusBar()
{
    m_titleLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_titleLabel);

    m_entityCountLabel = new QLabel(this);
    m_entityCountLabel->setStyleSheet("color: #8C8C8C; font-size: 12px; padding: 0 8px;");
    statusBar()->addPermanentWidget(m_entityCountLabel);

    m_fpsLabel = new QLabel("0 FPS", this);
    m_fpsLabel->setStyleSheet("color: #8C8C8C; font-size: 12px; padding-right: 8px;");
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

        int fps = dt > 0 ? static_cast<int>(1.0f / dt) : 0;
        m_fpsLabel->setText(QString("%1 FPS").arg(fps));

        auto* scene = m_ctx.activeScene();
        if (scene && m_entityCountLabel) {
            auto entities = scene->getEntities();
            m_entityCountLabel->setText(QString("Entities: %1").arg(entities.size()));
        }
    });
    m_frameTimer.start(0);
}

void EditorWindow::connectActions()
{
    m_ctx.document().dirtyChanged.connect([this]() {
        setWindowTitle(QString::fromStdString(m_ctx.document().displayTitle() + " — Cakery"));
    });

    m_ctx.playMode().stateChanged.connect([this](PlayState state) {
        m_isPlaying = (state == PlayState::Playing);
        m_isPaused  = (state == PlayState::Paused);
        if (m_btnPlay)  m_btnPlay->setChecked(m_isPlaying);
        if (m_btnPause) m_btnPause->setChecked(m_isPaused);
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
