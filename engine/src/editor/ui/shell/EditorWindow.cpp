// do@Redlive

#include "dopch.h"

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

#include "FramelessChrome.h"
#include "SceneSurface.h"

#include "presentation/shell/ShellViewModel.h"
#include "ui/shell/EditorLayoutController.h"
#include "ui/common/QtStringInterop.h"
#include "ui/common/EditorWorkspaceContext.h"
#include "ui/common/EditorIcons.h"
#include "core/document/EditorDocumentModel.h"
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/HierarchyPanel.h"
#include "ui/panels/HistoryPanel.h"
#include "ui/panels/WorkflowPanel.h"
#include "ui/panels/InspectorPanel.h"
#include "ui/panels/ProjectPanel.h"
#include "ui/panels/SettingsPanel.h"
#include "ui/panels/TileLayersPanel.h"
#include "ui/panels/TilePalettePanel.h"
#include "ui/panels/ScriptPanel.h"
#include "ui/inspector/EditorRemoteWidget.h"

#include <algorithm>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <FloatingDockContainer.h>

#include <QAbstractButton>
#include <QApplication>
#include <QActionGroup>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QByteArray>
#include <QDir>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
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
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QProgressDialog>
#include <QPushButton>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QGridLayout>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QStandardPaths>
#include <QUrl>
#include <QStringList>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cakery {

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
    : QMainWindow(parent),
      m_context(context),
      m_shellViewModel(create_scope<ShellViewModel>(
          context.documents(), context.playMode(), context.view(), context.tools(),
          context.scriptWindows(), context.assets(), context.runtime(),
          ToDodoeString(QApplication::applicationName())))
{
    setWindowTitle(QApplication::applicationName());
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    resize(1440, 900);
    setMinimumSize(900, 600);
    setStatusBar(nullptr);

    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DisableStylesheet, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::ActiveTabHasCloseButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::AllTabsHaveCloseButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasUndockButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DisableTabTextEliding, true);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::AutoHideFeatureEnabled, true);
    // Auto Hide is available from the dock title context menu only. Godot's
    // layout does not expose a permanent pin button in every title bar.
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::DockAreaHasAutoHideButton, false);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::AutoHideCloseOnOutsideMouseClick, true);
    m_dockManager = new ads::CDockManager(this);
    connect(m_dockManager, &ads::CDockManager::floatingWidgetCreated,
            this, &EditorWindow::setupFloatingDockWindow);
    qApp->installEventFilter(this);

    createTitleBar();
    createMenus();
    createToolbar();
    createDocks();
    createPanels();
    createWindowMenu();
    const QString safeName = QApplication::applicationName().toLower().replace(
        QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    m_layoutController = create_scope<EditorLayoutController>(
        m_dockManager, safeName + QStringLiteral("_layout.state"));
    m_layoutController->captureDefault();
    m_layoutController->restore();
    startSafePointTimer();

    m_assetImportTimer = new QTimer(this);
    m_assetImportTimer->setInterval(100);
    connect(m_assetImportTimer, &QTimer::timeout, this, &EditorWindow::updateAssetImportProgress);
    m_assetImportTimer->start();

    m_historyViewSubscription = ScopedConnection(
        m_shellViewModel->historyChanged,
        m_shellViewModel->historyChanged.connect([this]() { refreshUndoRedoActions(); }));
    m_runtimeViewSubscription = ScopedConnection(
        m_shellViewModel->runtimeChanged,
        m_shellViewModel->runtimeChanged.connect([this]() { updateRuntimeControls(); }));
    m_cameraViewSubscription = ScopedConnection(
        m_shellViewModel->cameraChanged,
        m_shellViewModel->cameraChanged.connect([this]() { updateCameraAction(); }));
    m_gizmoViewSubscription = ScopedConnection(
        m_shellViewModel->gizmoChanged,
        m_shellViewModel->gizmoChanged.connect([this](GizmoMode mode) {
            updateGizmoActions(mode);
        }));
    m_documentViewSubscription = ScopedConnection(
        m_shellViewModel->documentChanged,
        m_shellViewModel->documentChanged.connect([this]() { updateWindowTitle(); }));
    m_missingRefsViewSubscription = ScopedConnection(
        m_shellViewModel->missingAssetReferences,
        m_shellViewModel->missingAssetReferences.connect([this](Size_t count) {
            showMissingAssetReferences(count);
        }));

    m_context.confirmUnsavedChanges = [this]() { return promptUnsavedChanges(); };

    refreshUndoRedoActions();
    updateRuntimeControls();
    updateCameraAction();
    updateGizmoActions(m_shellViewModel->gizmoMode());
    updateWindowTitle();
}

EditorWindow::~EditorWindow()
{
    m_context.confirmUnsavedChanges = {};
    qApp->removeEventFilter(this);
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
        if (!m_shellViewModel->hasDocument()) {
            return;
        }
        if (!m_shellViewModel->saveDocument(String())) {
            QMessageBox::warning(this, tr("Save Scene"),
                                 tr("Could not save the scene to '%1'.")
                                     .arg(ToQString(m_shellViewModel->documentPath())));
        }
    });

    auto* saveAs = file->addAction(tr("Save Scene As..."));
    connect(saveAs, &QAction::triggered, this, [this]() {
        if (!m_shellViewModel->hasDocument()) {
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Save Scene As"), QString(), tr("Dodoe Scene (*.doscn)"));
        if (path.isEmpty()) {
            return;
        }
        if (!m_shellViewModel->saveDocument(ToDodoeString(path))) {
            QMessageBox::warning(this, tr("Save Scene"),
                                 tr("Could not save the scene to '%1'.").arg(path));
        }
    });

    file->addSeparator();
    auto* back = file->addAction(tr("Return to Project Manager"));
    connect(back, &QAction::triggered, this, &EditorWindow::returnToProjectManagerRequested);

    auto* close = file->addAction(tr("Close"));
    connect(close, &QAction::triggered, this, &QWidget::close);

    auto* edit = m_menuBar->addMenu(tr("Edit"));
    m_undoAction = edit->addAction(tr("Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, [this]() { m_shellViewModel->undo(); });
    m_redoAction = edit->addAction(tr("Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, this, [this]() { m_shellViewModel->redo(); });

    auto* runtime = m_menuBar->addMenu(tr("Runtime"));
    m_runtimeMenuActions[0] = runtime->addAction(tr("Play"));
    connect(m_runtimeMenuActions[0], &QAction::triggered, this, [this]() {
        m_shellViewModel->play();
    });
    m_runtimeMenuActions[1] = runtime->addAction(tr("Pause"));
    connect(m_runtimeMenuActions[1], &QAction::triggered, this, [this]() {
        m_shellViewModel->pauseOrResume();
    });
    m_runtimeMenuActions[2] = runtime->addAction(tr("Stop"));
    connect(m_runtimeMenuActions[2], &QAction::triggered, this, &EditorWindow::stopPlayWithPrompt);

    m_settingsMenu = m_menuBar->addMenu(tr("Settings"));

    createToolsMenu();
}

void EditorWindow::createToolsMenu()
{
    m_toolsMenu = m_menuBar->addMenu(tr("Tools"));
    connect(m_toolsMenu, &QMenu::aboutToShow, this, [this]() { refreshToolsMenu(); });
}

void EditorWindow::refreshToolsMenu()
{
    if (!m_toolsMenu) {
        return;
    }
    m_toolsMenu->clear();

    DynamicArray<String> paths;
    if (!m_shellViewModel->listToolActions(paths)) {
        auto* unavailable = m_toolsMenu->addAction(tr("No script tools available"));
        unavailable->setEnabled(false);
        return;
    }

    struct Node {
        OrderedMap<QString, Node> children;
        DynamicArray<QString> leaves;
    };
    Node root;
    for (const String& rawPath : paths) {
        const QString path = ToQString(rawPath);
        const QStringList segments = path.split(QLatin1Char('/'));
        Node* node = &root;
        for (int i = 0; i + 1 < segments.size(); ++i) {
            node = &node->children[segments.at(i)];
        }
        node->leaves.push_back(segments.constLast());
    }

    const auto buildMenu = [&](const auto& self, QMenu* parent, Node& node,
                               const QString& prefix) -> void {
        for (auto& [name, child] : node.children) {
            const QString childPrefix = prefix.isEmpty() ? name : prefix + QLatin1Char('/') + name;
            QMenu* submenu = parent->addMenu(name);
            self(self, submenu, child, childPrefix);
        }
        for (const QString& leaf : node.leaves) {
            const QString fullPath = prefix.isEmpty() ? leaf : prefix + QLatin1Char('/') + leaf;
            auto* action = parent->addAction(leaf);
            connect(action, &QAction::triggered, this, [this, fullPath]() {
                if (!m_shellViewModel->invokeToolAction(ToDodoeString(fullPath))) {
                    if (m_console) {
                        m_console->append(ConsoleLogLevel::Error,
                                          QStringLiteral("Tool action failed: %1").arg(fullPath),
                                          QStringLiteral("Tools"));
                    }
                }
            });
        }
    };
    buildMenu(buildMenu, m_toolsMenu, root, QString());
}

void EditorWindow::createWindowMenu()
{
    m_windowMenu = m_menuBar->addMenu(tr("Window"));
    m_resetLayoutAction = m_windowMenu->addAction(tr("Reset Layout"));
    connect(m_resetLayoutAction, &QAction::triggered, this, &EditorWindow::resetLayout);

    auto* themes = m_windowMenu->addMenu(tr("Theme"));
    const StaticArray<Pair<const char*, const char*>, 3> themeChoices = {{
        {"cakery-light", QT_TR_NOOP("Light")},
        {"cakery-dark", QT_TR_NOOP("Dark")},
        {"cakery-color", QT_TR_NOOP("Color")},
    }};
    auto* themeGroup = new QActionGroup(themes);
    themeGroup->setExclusive(true);
    const String activeTheme = m_context.config().themeName();
    for (const auto& [themeName, themeLabel] : themeChoices) {
        auto* action = themes->addAction(tr(themeLabel));
        action->setCheckable(true);
        action->setChecked(activeTheme == themeName);
        themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, themeName]() {
            m_context.config().setThemeName(themeName);
            m_context.theme().apply(String(themeName));
        });
    }
    m_editorWindowsMenu = m_windowMenu->addMenu(tr("Editor Windows"));
    connect(m_editorWindowsMenu, &QMenu::aboutToShow, this, [this]() {
        refreshEditorWindowsMenu();
    });

    m_editorWindowTimer = new QTimer(this);
    m_editorWindowTimer->setInterval(150);
    connect(m_editorWindowTimer, &QTimer::timeout, this, &EditorWindow::refreshEditorWindows);
    m_editorWindowTimer->start();

    populatePanelMenus();
}

void EditorWindow::populatePanelMenus()
{
    const StaticArray<ads::CDockWidget*, 12> panelDocks = {
        m_hierarchyDock, m_inspectorDock, m_projectDock, m_consoleDock,
        m_terminalDock, m_historyDock, m_workflowDock, m_gameSettingsDock,
        m_engineSettingsDock, m_tilePaletteDock, m_tileLayersDock, m_scriptDock,
    };
    QList<QAction*> toggleActions;
    for (ads::CDockWidget* dock : panelDocks) {
        if (!dock) {
            continue;
        }
        setupPanelToggle(dock);
        toggleActions.push_back(dock->toggleViewAction());
    }
    if (m_windowMenu && m_resetLayoutAction) {
        m_windowMenu->insertActions(m_resetLayoutAction, toggleActions);
        m_windowMenu->insertSeparator(m_resetLayoutAction);
    }
    if (m_settingsMenu) {
        if (m_gameSettingsDock) m_settingsMenu->addAction(m_gameSettingsDock->toggleViewAction());
        if (m_engineSettingsDock) m_settingsMenu->addAction(m_engineSettingsDock->toggleViewAction());
    }
}

void EditorWindow::refreshEditorWindowsMenu()
{
    if (!m_editorWindowsMenu) {
        return;
    }
    m_editorWindowsMenu->clear();

    DynamicArray<Pair<String, String>> windows;
    if (!m_shellViewModel->listEditorWindows(windows) || windows.empty()) {
        auto* unavailable = m_editorWindowsMenu->addAction(tr("No script windows available"));
        unavailable->setEnabled(false);
        return;
    }

    for (const auto& [id, title] : windows) {
        auto* action = m_editorWindowsMenu->addAction(ToQString(title));
        connect(action, &QAction::triggered, this, [this, id, title]() {
            openEditorWindow(ToQString(id), ToQString(title));
        });
    }
}

void EditorWindow::openEditorWindow(const QString& id, const QString& title)
{
    for (auto& [existingId, dock] : m_editorWindowDocks) {
        if (existingId == id && dock) {
            dock->toggleView(true);
            return;
        }
    }

    auto* dock = new ads::CDockWidget(title);
    dock->setObjectName(QStringLiteral("ScriptWindow_") + id);
    const String windowId = ToDodoeString(id);
    auto* remote = new EditorRemoteWidget(
        [this, windowId](Json& out) {
            return m_shellViewModel->getEditorWindowUI(windowId, out);
        },
        [this, windowId](const String& controlId, const String& eventName,
                         const Json& value) {
            m_shellViewModel->dispatchEditorEvent("window", windowId, controlId, eventName, value);
        },
        dock);
    dock->setWidget(remote);
    dock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, dock);
    m_editorWindowDocks.emplace_back(id, dock);

    connect(dock, &QObject::destroyed, this, [this, id]() {
        m_editorWindowDocks.erase(
            std::remove_if(m_editorWindowDocks.begin(), m_editorWindowDocks.end(),
                           [&id](const Pair<QString, ads::CDockWidget*>& entry) {
                               return entry.first == id;
                           }),
            m_editorWindowDocks.end());
    });
}

void EditorWindow::refreshEditorWindows()
{
    for (auto& [id, dock] : m_editorWindowDocks) {
        if (!dock) {
            continue;
        }
        auto* remote = qobject_cast<EditorRemoteWidget*>(dock->widget());
        if (remote) {
            remote->rebuildIfChanged();
        }
    }
}

void EditorWindow::setupPanelToggle(ads::CDockWidget* dock)
{
    QAction* action = dock->toggleViewAction();
    const QIcon checkIcon = editorThemedIcon(QStringLiteral("check.svg"));
    QPixmap blankPixmap(16, 16);
    blankPixmap.fill(Qt::transparent);
    const QIcon blankIcon(blankPixmap);
    const auto applyIcon = [action, checkIcon, blankIcon](bool checked) {
        action->setIcon(checked ? checkIcon : blankIcon);
    };
    applyIcon(action->isChecked());
    connect(dock, &ads::CDockWidget::viewToggled, this, applyIcon);
}

void EditorWindow::setupFloatingDockWindow(ads::CFloatingDockContainer* floating)
{
    if (!floating || floating->property("cakeryCustomTitleBar").toBool()) {
        return;
    }

    auto* layout = qobject_cast<QBoxLayout*>(floating->layout());
    if (!layout) {
        return;
    }

    floating->setProperty("cakeryCustomTitleBar", true);
    floating->setAttribute(Qt::WA_TranslucentBackground, true);
    floating->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    layout->insertWidget(0, new FloatingWindowTitleBar(floating));
}

void EditorWindow::resetLayout()
{
    if (m_layoutController) {
        m_layoutController->reset();
    }
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

    auto* titleIcon = new QLabel(m_titleBar);
    titleIcon->setObjectName(QStringLiteral("editorTitleIcon"));
    titleIcon->setFixedSize(22, 22);
    titleIcon->setAlignment(Qt::AlignCenter);
    titleIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleIcon->setPixmap(windowIcon().pixmap(QSize(18, 18)));
    titleBarLayout->addWidget(titleIcon);

    m_menuBar = new QMenuBar(m_titleBar);
    m_menuBar->installEventFilter(this);
    titleBarLayout->addWidget(m_menuBar);

    titleBarLayout->addStretch();

    auto* minButton = new QToolButton(m_titleBar);
    minButton->setObjectName(QStringLiteral("windowMinButton"));
    minButton->setIcon(editorIcon(QStringLiteral("minus.svg")));
    minButton->setIconSize(QSize(16, 16));
    minButton->setToolTip(tr("Minimize"));
    connect(minButton, &QToolButton::clicked, this, &QWidget::showMinimized);
    titleBarLayout->addWidget(minButton);

    m_maxButton = new QToolButton(m_titleBar);
    m_maxButton->setObjectName(QStringLiteral("windowMaxButton"));
    m_maxButton->setIcon(editorIcon(QStringLiteral("maximize-2.svg")));
    m_maxButton->setIconSize(QSize(16, 16));
    m_maxButton->setToolTip(tr("Maximize"));
    connect(m_maxButton, &QToolButton::clicked, this, &EditorWindow::toggleMaximize);
    titleBarLayout->addWidget(m_maxButton);

    auto* closeButton = new QToolButton(m_titleBar);
    closeButton->setObjectName(QStringLiteral("windowCloseButton"));
    closeButton->setIcon(editorIcon(QStringLiteral("x.svg")));
    closeButton->setIconSize(QSize(16, 16));
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

    struct RuntimeButton {
        QString tooltip;
        QString icon;
        PlayAction action;
        const char* name;
    };
    const RuntimeButton runtimeButtons[] = {
        {tr("Play"), QStringLiteral("play.svg"), PlayAction::Play, "play"},
        {tr("Pause"), QStringLiteral("pause.svg"), PlayAction::Pause, "pause"},
        {tr("Stop"), QStringLiteral("square.svg"), PlayAction::Stop, "stop"},
    };
    for (const RuntimeButton& runtimeButton : runtimeButtons) {
        auto* button = new QToolButton(m_editorToolbar);
        button->setObjectName(QStringLiteral("runtimeToolButton"));
        button->setProperty("runtimeCommand", QString::fromLatin1(runtimeButton.name));
        button->setIcon(editorIcon(runtimeButton.icon));
        button->setIconSize(QSize(16, 16));
        button->setToolTip(runtimeButton.tooltip);
        connect(button, &QToolButton::clicked, this, [this, action = runtimeButton.action]() {
            if (action == PlayAction::Stop) {
                stopPlayWithPrompt();
                return;
            }
            if (action == PlayAction::Pause) {
                m_shellViewModel->pauseOrResume();
                return;
            }
            m_shellViewModel->play();
        });
        if (runtimeButton.action == PlayAction::Play) {
            m_playButton = button;
        } else if (runtimeButton.action == PlayAction::Pause) {
            m_pauseButton = button;
        } else {
            m_stopButton = button;
        }
        m_editorToolbar->addWidget(button);
    }

    auto* trailingSpacer = new QWidget(m_editorToolbar);
    trailingSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    trailingSpacer->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_editorToolbar->addWidget(trailingSpacer);
}

void EditorWindow::toggleMaximize()
{
    if (isMaximized() || isFullScreen()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void EditorWindow::updateMaximizeButton()
{
    if (!m_maxButton) {
        return;
    }
    const bool restore = isMaximized() || isFullScreen();
    m_maxButton->setIcon(editorIcon(restore ? QStringLiteral("minimize-2.svg")
                                             : QStringLiteral("maximize-2.svg")));
    m_maxButton->setToolTip(restore ? tr("Restore Down") : tr("Maximize"));
}

void EditorWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && m_maxButton) {
        updateMaximizeButton();
        applyWindowsFrameAttributes(this);
    }
    QMainWindow::changeEvent(event);
}

bool EditorWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        if (auto* messageBox = qobject_cast<QMessageBox*>(watched)) {
            setupFramelessMessageBox(messageBox);
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* floating = floatingWindowAncestor(watched);
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (floating && mouse->button() == Qt::LeftButton && floating->windowHandle()
            && !floating->isMaximized()) {
            const QPoint pos = floating->mapFromGlobal(mouse->globalPosition().toPoint());
            constexpr int border = 6;
            Qt::Edges edges;
            if (pos.x() < border) edges |= Qt::LeftEdge;
            if (pos.x() >= floating->width() - border) edges |= Qt::RightEdge;
            if (pos.y() >= floating->height() - border) edges |= Qt::BottomEdge;
            if (edges != Qt::Edges() && floating->windowHandle()->startSystemResize(edges)) {
                return true;
            }
        }
    }

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
            const qreal dpr = devicePixelRatioF();
            const QPoint pos = mapFromGlobal(QPoint(
                static_cast<int>(std::lround(static_cast<qreal>(x) / dpr)),
                static_cast<int>(std::lround(static_cast<qreal>(y) / dpr))));
            const int border = 6;
            const bool resizable = !isMaximized() && !isFullScreen();
            const bool left = pos.x() < border;
            const bool right = pos.x() >= width() - border;
            const bool top = pos.y() < border;
            const bool bottom = pos.y() >= height() - border;
            if (resizable) {
                if (top && left) { *result = HTTOPLEFT; return true; }
                if (top && right) { *result = HTTOPRIGHT; return true; }
                if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
                if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
                if (left) { *result = HTLEFT; return true; }
                if (right) { *result = HTRIGHT; return true; }
                if (top) { *result = HTTOP; return true; }
                if (bottom) { *result = HTBOTTOM; return true; }
            }
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
    auto* sceneDock = new ads::CDockWidget(tr("Scene"));
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

    m_camera2DAction = sceneToolbar->addAction(
        editorThemedIcon(QStringLiteral("viewport-3d.svg")), QString());
    m_camera2DAction->setCheckable(true);
    m_camera2DAction->setChecked(false);
    m_camera2DAction->setToolTip(tr("Switch to 2D view (orthographic)"));
    connect(m_camera2DAction, &QAction::triggered, this, [this](bool checked) {
        m_shellViewModel->setCamera2D(checked);
    });

    auto* toolGroup = new QActionGroup(sceneToolbar);
    toolGroup->setExclusive(true);
    struct SceneTool {
        QString tooltip;
        QString icon;
        GizmoMode mode;
    };
    const SceneTool sceneTools[] = {
        {tr("Hand"), QStringLiteral("hand.svg"), GizmoMode::None},
        {tr("Move"), QStringLiteral("move-3d.svg"), GizmoMode::Translate},
        {tr("Rotate"), QStringLiteral("rotate-3d.svg"), GizmoMode::Rotate},
        {tr("Scale"), QStringLiteral("scale-3d.svg"), GizmoMode::Scale},
    };
    int toolIndex = 0;
    for (const SceneTool& sceneTool : sceneTools) {
        auto* action = sceneToolbar->addAction(editorThemedIcon(sceneTool.icon), QString());
        if (auto* toolButton = sceneToolbar->widgetForAction(action)) {
            toolButton->setProperty("sceneTool", QString::fromLatin1(gizmoModeName(sceneTool.mode)));
        }
        action->setCheckable(true);
        action->setToolTip(sceneTool.tooltip);
        toolGroup->addAction(action);
        if (toolIndex < 4) {
            m_sceneToolActions[toolIndex] = action;
        }
        if (sceneTool.mode == GizmoMode::Translate) action->setChecked(true);
        connect(action, &QAction::triggered, this, [this, mode = sceneTool.mode]() {
            m_shellViewModel->setGizmoMode(mode);
        });
        ++toolIndex;
    }

    sceneToolbar->addSeparator();

    auto* snapAction = sceneToolbar->addAction(editorThemedIcon(QStringLiteral("grid.svg")), QString());
    snapAction->setCheckable(true);
    snapAction->setToolTip(tr("Toggle snapping (hold Ctrl while dragging to snap temporarily)"));
    connect(snapAction, &QAction::triggered, this, [this](bool checked) {
        m_shellViewModel->setGizmoSnap(checked);
    });

    const auto makeStepBox = [&](const QString& prefix, double value, double step, const QString& tooltip) {
        auto* box = new QDoubleSpinBox(sceneToolbar);
        box->setToolTip(tooltip);
        box->setPrefix(prefix + QLatin1Char(' '));
        box->setRange(0.0, 1000.0);
        box->setDecimals(3);
        box->setSingleStep(step);
        box->setValue(value);
        box->setFixedWidth(78);
        return box;
    };
    QDoubleSpinBox* translateStep = makeStepBox(QStringLiteral("T"), 0.25, 0.05, tr("Translate snap step"));
    QDoubleSpinBox* rotateStep = makeStepBox(QStringLiteral("R"), 15.0, 1.0, tr("Rotate snap step (degrees)"));
    QDoubleSpinBox* scaleStep = makeStepBox(QStringLiteral("S"), 0.1, 0.05, tr("Scale snap step"));
    sceneToolbar->addWidget(translateStep);
    sceneToolbar->addWidget(rotateStep);
    sceneToolbar->addWidget(scaleStep);

    const auto pushSnapSteps = [this, translateStep, rotateStep, scaleStep]() {
        m_shellViewModel->setGizmoSnapStep(
            static_cast<float>(translateStep->value()),
            static_cast<float>(rotateStep->value()),
            static_cast<float>(scaleStep->value()));
    };
    for (QDoubleSpinBox* box : {translateStep, rotateStep, scaleStep}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [pushSnapSteps](double) { pushSnapSteps(); });
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
    m_hierarchyDock = new ads::CDockWidget(tr("Hierarchy"));
    m_hierarchyDock->setObjectName(QStringLiteral("Hierarchy"));
    m_hierarchy = new HierarchyPanel(m_context, m_hierarchyDock);
    m_hierarchyDock->setWidget(m_hierarchy);
    m_hierarchyDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::LeftDockWidgetArea, m_hierarchyDock);

    m_inspectorDock = new ads::CDockWidget(tr("Inspector"));
    m_inspectorDock->setObjectName(QStringLiteral("Inspector"));
    m_inspector = new InspectorPanel(m_context, m_inspectorDock);
    m_inspectorDock->setWidget(m_inspector);
    m_inspectorDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_inspectorDock);

    m_projectDock = new ads::CDockWidget(tr("Project"));
    m_projectDock->setObjectName(QStringLiteral("Project"));
    m_projectPanel = new ProjectPanel(m_context, m_projectDock);
    m_projectDock->setWidget(m_projectPanel);
    m_projectDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_projectDock, m_hierarchyDock->dockAreaWidget());

    m_scriptDock = new ads::CDockWidget(tr("Script"));
    m_scriptDock->setObjectName(QStringLiteral("Script"));
    m_scriptPanel = new ScriptPanel(m_context, m_scriptDock);
    m_scriptDock->setWidget(m_scriptPanel);
    m_scriptDock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
    m_scriptDock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
    m_scriptDock->setFeature(ads::CDockWidget::DockWidgetMovable, true);
    m_scriptDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_scriptDock, m_inspectorDock->dockAreaWidget());
    m_scriptDock->toggleView(false);
    connect(m_projectPanel, &ProjectPanel::scriptOpenRequested, this, [this](const QString& path) {
        m_scriptDock->toggleView(true);
        m_scriptDock->raise();
        m_scriptPanel->openFile(path);
    });

    m_consoleDock = new ads::CDockWidget(tr("Console"));
    m_consoleDock->setObjectName(QStringLiteral("Console"));
    m_console = new ConsolePanel(m_context, m_consoleDock);
    m_console->append(ConsoleLogLevel::Info, tr("Editor console ready"),
                      QStringLiteral("Cakery"));
    m_console->append(ConsoleLogLevel::Info, ToQString(m_shellViewModel->diagnostic()),
                      QStringLiteral("Backend"));
    m_consoleDock->setWidget(m_console);
    m_consoleDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_consoleDock, m_sceneDock->dockAreaWidget());

    m_terminalDock = new ads::CDockWidget(tr("Terminal"));
    m_terminalDock->setObjectName(QStringLiteral("Terminal"));
    m_terminalDock->setWidget(unavailablePanel(tr("Terminal unavailable"),
        tr("The runtime terminal panel is not implemented yet."), m_terminalDock));
    m_terminalDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_terminalDock, m_consoleDock->dockAreaWidget());

    m_historyDock = new ads::CDockWidget(tr("History"));
    m_historyDock->setObjectName(QStringLiteral("History"));
    m_historyPanel = new HistoryPanel(m_context, m_historyDock);
    m_historyDock->setWidget(m_historyPanel);
    m_historyDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_historyDock, m_consoleDock->dockAreaWidget());

    m_workflowDock = new ads::CDockWidget(tr("Workflow"));
    m_workflowDock->setObjectName(QStringLiteral("Workflow"));
    m_workflowPanel = new WorkflowPanel(m_context, m_workflowDock);
    m_workflowDock->setWidget(m_workflowPanel);
    m_workflowDock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
    m_workflowDock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
    m_workflowDock->setFeature(ads::CDockWidget::DockWidgetMovable, true);
    m_workflowDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_workflowDock, m_historyDock->dockAreaWidget());
    m_workflowDock->toggleView(false);

    m_tilePaletteDock = new ads::CDockWidget(tr("Tile Palette"));
    m_tilePaletteDock->setObjectName(QStringLiteral("Tile Palette"));
    m_tilePalette = new TilePalettePanel(m_context, m_tilePaletteDock);
    m_tilePaletteDock->setWidget(m_tilePalette);
    m_tilePaletteDock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
    m_tilePaletteDock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
    m_tilePaletteDock->setFeature(ads::CDockWidget::DockWidgetMovable, true);
    m_tilePaletteDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_tilePaletteDock, m_historyDock->dockAreaWidget());

    m_tileLayersDock = new ads::CDockWidget(tr("Tile Layers"));
    m_tileLayersDock->setObjectName(QStringLiteral("Tile Layers"));
    m_tileLayers = new TileLayersPanel(m_context, m_tileLayersDock);
    m_tileLayersDock->setWidget(m_tileLayers);
    m_tileLayersDock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
    m_tileLayersDock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
    m_tileLayersDock->setFeature(ads::CDockWidget::DockWidgetMovable, true);
    m_tileLayersDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_tileLayersDock, m_tilePaletteDock->dockAreaWidget());

    if (m_hierarchy && m_tilePalette) {
        connect(m_hierarchy, &HierarchyPanel::newTilemapRequested,
                m_tilePalette, &TilePalettePanel::onNewTilemap);
    }

    m_gameSettingsPanel = new SettingsPanel(nullptr);
    m_gameSettingsDock = new ads::CDockWidget(tr("Game Settings"));
    m_gameSettingsDock->setObjectName(QStringLiteral("Game Settings"));
    m_gameSettingsDock->setWidget(m_gameSettingsPanel);
    m_gameSettingsDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_gameSettingsDock, m_consoleDock->dockAreaWidget());
    m_gameSettingsDock->toggleView(false);
    connect(m_gameSettingsPanel, &SettingsPanel::saved, this, [this]() {
        const auto& config = m_gameSettingsPanel->value();
        const auto renderIt = config.find("render_settings");
        if (renderIt == config.end() || !renderIt->is_object()) {
            return;
        }
        m_shellViewModel->applyRenderSettingsOverlay(*renderIt);
    });

    m_engineSettingsPanel = new SettingsPanel(nullptr);
    m_engineSettingsDock = new ads::CDockWidget(tr("Engine Settings"));
    m_engineSettingsDock->setObjectName(QStringLiteral("Engine Settings"));
    m_engineSettingsDock->setWidget(m_engineSettingsPanel);
    m_engineSettingsDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_engineSettingsDock, m_consoleDock->dockAreaWidget());
    m_engineSettingsDock->toggleView(false);
    connect(m_engineSettingsPanel, &SettingsPanel::saved, this, [this]() {
        m_context.config().reload();
        m_context.theme().apply(m_context.config().themeName());
    });

    // Establish the single-Scene authoring layout before the user resizes any dock.
    m_dockManager->setSplitterSizes(m_inspectorDock->dockAreaWidget(), QList<int>{260, 820, 320});
    m_dockManager->setSplitterSizes(m_hierarchyDock->dockAreaWidget(), QList<int>{610, 240});
    m_dockManager->setSplitterSizes(m_consoleDock->dockAreaWidget(), QList<int>{610, 240});
}

void EditorWindow::startSafePointTimer()
{
    m_safePointTimer = new QTimer(this);
    m_safePointTimer->setInterval(16);
    connect(m_safePointTimer, &QTimer::timeout, this, [this]() {
        m_shellViewModel->tick();
    });
    m_safePointTimer->start();
}

void EditorWindow::stopPlayWithPrompt()
{
    if (m_shellViewModel->playState() == PlayState::Edit) {
        return;
    }
    bool keepPlayChanges = false;
    if (m_shellViewModel->playDocumentEdited()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Stop Play Mode"));
        box.setText(tr("The scene was modified while playing."));
        box.setInformativeText(tr("Discard runtime changes or keep them in the scene?"));
        QPushButton* discard = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        QPushButton* keep = box.addButton(tr("Keep"), QMessageBox::AcceptRole);
        box.addButton(tr("Cancel"), QMessageBox::RejectRole);
        box.setDefaultButton(keep);
        box.exec();
        if (box.clickedButton() == discard) {
            keepPlayChanges = false;
        } else if (box.clickedButton() == keep) {
            keepPlayChanges = true;
        } else {
            return;
        }
    }
    m_shellViewModel->stopPlay(keepPlayChanges);
}

void EditorWindow::updateRuntimeControls()
{
    const PlayState state = m_shellViewModel->playState();
    if (m_playButton) {
        m_playButton->setEnabled(state == PlayState::Edit);
        m_playButton->setToolTip(tr("Play"));
    }
    if (m_pauseButton) {
        const bool paused = state == PlayState::Paused;
        m_pauseButton->setEnabled(state != PlayState::Edit);
        m_pauseButton->setIcon(editorIcon(
            paused ? QStringLiteral("play.svg") : QStringLiteral("pause.svg")));
        m_pauseButton->setToolTip(paused ? tr("Resume") : tr("Pause"));
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(state != PlayState::Edit);
        m_stopButton->setToolTip(tr("Stop"));
    }
    if (m_runtimeMenuActions[0]) {
        m_runtimeMenuActions[0]->setEnabled(state == PlayState::Edit);
    }
    if (m_runtimeMenuActions[1]) {
        const bool paused = state == PlayState::Paused;
        m_runtimeMenuActions[1]->setText(paused ? tr("Resume") : tr("Pause"));
        m_runtimeMenuActions[1]->setEnabled(state != PlayState::Edit);
    }
    if (m_runtimeMenuActions[2]) {
        m_runtimeMenuActions[2]->setEnabled(state != PlayState::Edit);
    }
}

void EditorWindow::updateCameraAction()
{
    const bool is2d = m_shellViewModel->is2D();
    if (!m_camera2DAction) {
        return;
    }
    m_camera2DAction->setChecked(is2d);
    m_camera2DAction->setIcon(editorThemedIcon(
        is2d ? QStringLiteral("viewport-2d.svg") : QStringLiteral("viewport-3d.svg")));
    m_camera2DAction->setToolTip(is2d
        ? tr("Switch to 3D view (perspective)")
        : tr("Switch to 2D view (orthographic)"));
}

void EditorWindow::updateGizmoActions(GizmoMode mode)
{
    const StaticArray<GizmoMode, 4> modes = {
        GizmoMode::None, GizmoMode::Translate, GizmoMode::Rotate, GizmoMode::Scale};
    for (Size_t i = 0; i < modes.size(); ++i) {
        if (m_sceneToolActions[i]) {
            QSignalBlocker blocker(m_sceneToolActions[i]);
            m_sceneToolActions[i]->setChecked(modes[i] == mode);
        }
    }
}

void EditorWindow::showMissingAssetReferences(Size_t count)
{
    if (count == 0 || !m_console) {
        return;
    }
    m_console->append(ConsoleLogLevel::Warning,
                      tr("%1 scene asset reference(s) could not be resolved. "
                         "Check the Inspector for missing references.").arg(count),
                      QStringLiteral("Assets"));
}

void EditorWindow::updateWindowTitle()
{
    setWindowTitle(ToQString(m_shellViewModel->windowTitle()));
}

bool EditorWindow::promptUnsavedChanges()
{
    if (m_shellViewModel->hasDocument() && m_shellViewModel->isDirty()) {
        UnsavedChangesDialog dialog(this);
        dialog.adjustSize();
        dialog.move(frameGeometry().center() - dialog.rect().center());
        dialog.exec();
        const auto choice = dialog.result();
        if (choice == QMessageBox::Cancel) {
            return false;
        }
        if (choice == QMessageBox::Save) {
            String target;
            if (m_shellViewModel->documentPath().empty()) {
                const QString path = QFileDialog::getSaveFileName(
                    this, tr("Save Scene As"), QString(), tr("Dodoe Scene (*.doscn)"));
                if (path.isEmpty()) {
                    return false;
                }
                target = ToDodoeString(path);
            }
            if (!m_shellViewModel->saveDocument(target)) {
                QMessageBox::warning(this, tr("Save Scene"),
                                     tr("Could not save the scene to '%1'.")
                                         .arg(ToQString(m_shellViewModel->documentPath())));
                return false;
            }
        }
    }
    if (m_scriptPanel && m_scriptPanel->hasUnsavedChanges()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Unsaved Scripts"));
        box.setText(tr("Open scripts have unsaved changes. Save them before continuing?"));
        QPushButton* save = box.addButton(tr("Save All"), QMessageBox::AcceptRole);
        QPushButton* discard = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        QPushButton* cancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == cancel || box.clickedButton() == nullptr) {
            return false;
        }
        if (box.clickedButton() == save) {
            m_scriptPanel->saveAll();
        } else if (box.clickedButton() == discard) {
            m_scriptPanel->discardAll();
        }
    }
    return true;
}

void EditorWindow::refreshUndoRedoActions()
{
    if (m_undoAction) m_undoAction->setEnabled(m_shellViewModel->canUndo());
    if (m_redoAction) m_redoAction->setEnabled(m_shellViewModel->canRedo());
}

void EditorWindow::updateAssetImportProgress()
{
    const bool pending = m_shellViewModel->assetRefreshPending();
    Size_t done = 0;
    Size_t total = 0;
    m_shellViewModel->assetRefreshProgress(done, total);

    if (!pending) {
        if (m_assetImportDialog) {
            m_assetImportDialog->close();
            m_assetImportDialog->deleteLater();
            m_assetImportDialog = nullptr;
        }
        return;
    }

    if (!m_assetImportDialog) {
        m_assetImportDialog = new QProgressDialog(tr("Importing assets..."), QString(), 0, 100, this);
        m_assetImportDialog->setWindowModality(Qt::WindowModal);
        m_assetImportDialog->setWindowTitle(tr("Importing Assets"));
        m_assetImportDialog->setCancelButton(nullptr);
        m_assetImportDialog->setAutoClose(false);
        m_assetImportDialog->setAutoReset(false);
        m_assetImportDialog->setMinimumDuration(400);
        m_assetImportDialog->setMinimumWidth(320);
    }
    if (total > 0) {
        m_assetImportDialog->setMaximum(static_cast<int>(total));
        m_assetImportDialog->setValue(static_cast<int>(std::min(done, total)));
    }
}

bool EditorWindow::enterWorkspace(const QString& projectPath)
{
    ProjectDescriptor project;
    const QFileInfo projectInfo(projectPath);
    project.rootPath = projectInfo.isDir()
        ? ToDodoeString(projectPath)
        : ToDodoeString(projectInfo.absolutePath());
    if (projectInfo.isFile()) {
        project.projectFile = ToDodoeString(projectPath);
    }
    if (!m_shellViewModel->openProject(project)) {
        return false;
    }
    m_context.resources().setProjectRoot(FsPath(project.rootPath));
    if (m_gameSettingsPanel) {
        m_gameSettingsPanel->setFilePath(
            QDir(ToQString(project.rootPath)).filePath(QStringLiteral("app_config.json")));
    }
    if (m_engineSettingsPanel) {
        m_engineSettingsPanel->setFallback([this]() { return m_context.config().editorJson(); });
        m_engineSettingsPanel->setFilePath(
            QDir(ToQString(project.rootPath))
                .filePath(QStringLiteral("ProjectSettings/Editor/editor.json")));
    }
    if (m_console) {
        m_console->append(ConsoleLogLevel::Info, ToQString(m_shellViewModel->diagnostic()),
                          QStringLiteral("Backend"));
    }
    if (m_sceneSurface) {
        m_sceneSurface->attach();
        if (!m_shellViewModel->bootEngine()) {
            if (m_console) {
                m_console->append(ConsoleLogLevel::Error,
                                  ToQString(m_shellViewModel->diagnostic()),
                                  QStringLiteral("Backend"));
            }
            return false;
        }
    }
    if (m_projectPanel) {
        m_projectPanel->refresh();
    }
    if (m_scriptPanel) {
        m_scriptPanel->reopenProject();
    }
    return true;
}

void EditorWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    applyWindowsFrameAttributes(this);
}

void EditorWindow::closeEvent(QCloseEvent* event)
{
    if (m_closed) {
        event->accept();
        return;
    }
    if (!promptUnsavedChanges()) {
        event->ignore();
        return;
    }
    m_closed = true;
    if (m_layoutController) {
        m_layoutController->save();
    }
    if (m_safePointTimer) {
        m_safePointTimer->stop();
    }
    m_shellViewModel->shutdown();
    QMainWindow::closeEvent(event);
}

} // namespace cakery
