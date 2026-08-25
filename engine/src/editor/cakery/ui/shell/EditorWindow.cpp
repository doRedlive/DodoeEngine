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

#include "cakery/app/EditorApplication.h"
#include "services/EditorConfig.h"
#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/EditorIcons.h"
#include "cakery/ui/panels/ConsolePanel.h"
#include "cakery/ui/panels/HierarchyPanel.h"
#include "cakery/ui/panels/HistoryPanel.h"
#include "cakery/ui/panels/InspectorPanel.h"
#include "cakery/ui/panels/ProjectPanel.h"
#include "cakery/ui/panels/SettingsPanel.h"
#include "cakery/ui/panels/TileLayersPanel.h"
#include "cakery/ui/panels/TilePalettePanel.h"

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

#include <QAbstractButton>
#include <QApplication>
#include <QActionGroup>
#include <QCloseEvent>
#include <QByteArray>
#include <QDir>
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
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <cmath>
#include <cstdint>
#include <array>
#include <filesystem>
#include <string>
#include <utility>

namespace cakery {

namespace {

int toCameraButton(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) return 0;
    if (button == Qt::MiddleButton) return 1;
    return 2;
}

#ifdef _WIN32
void UpdateWindowsFrameAttributes(QWidget* widget)
{
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    constexpr DWORD kDwmWindowCornerPreference = 33;
    constexpr DWORD kDwmBorderColor = 34;
    constexpr DWORD kDwmCornerDoNotRound = 1;
    constexpr DWORD kDwmCornerRound = 2;
    constexpr DWORD kDwmColorNone = 0xFFFFFFFE;

    static HMODULE dwmModule = LoadLibraryW(L"dwmapi.dll");
    if (!dwmModule) {
        return;
    }
    const auto setWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(dwmModule, "DwmSetWindowAttribute"));
    if (!setWindowAttribute) {
        return;
    }

    const bool edgeToEdge = widget->isMaximized() || widget->isFullScreen();
    const DWORD cornerPreference = edgeToEdge ? kDwmCornerDoNotRound : kDwmCornerRound;
    const DWORD borderColor = edgeToEdge ? kDwmColorNone : RGB(21, 21, 21);
    const HWND window = reinterpret_cast<HWND>(widget->winId());
    setWindowAttribute(window, kDwmWindowCornerPreference, &cornerPreference, sizeof(cornerPreference));
    setWindowAttribute(window, kDwmBorderColor, &borderColor, sizeof(borderColor));
}
#endif

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
        const float dpr = static_cast<float>(devicePixelRatioF());
        surface.logicalWidth = width();
        surface.logicalHeight = height();
        surface.devicePixelRatio = dpr;
        surface.pixelWidth = static_cast<int>(std::lround(width() * dpr));
        surface.pixelHeight = static_cast<int>(std::lround(height() * dpr));
        // The backend must see the first real surface size before creating its
        // host swapchain; otherwise the first frame is built at a fallback size.
        m_context.session().submitViewportMetrics(ViewportMetrics{
            surface.logicalWidth, surface.logicalHeight, surface.devicePixelRatio,
            surface.pixelWidth, surface.pixelHeight, surface.nativeHandle, ++m_sequence});
        m_attached = m_context.session().attachSceneSurface(surface);
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
                         Qt::AlignHCenter | Qt::AlignTop, tr("Scene"));

        painter.setFont(QFont());
        painter.setPen(QColor("#A0A0A0"));
        painter.drawText(rect().adjusted(20, height() / 2 + 2, -20, 0),
                         Qt::AlignHCenter | Qt::AlignTop,
                         tr("Scene preview unavailable in Editor-Only"));
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
    createWindowMenu();
    m_defaultLayoutState = new QByteArray(m_dockManager->saveState(1));
    const QString safeName = QApplication::applicationName().toLower().replace(
        QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    m_layoutStatePath = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(safeName + QStringLiteral("_layout.state"));
    restoreLayoutState();
    startSafePointTimer();

    m_historySubscription = m_context.session().history().subscribe([this]() { refreshUndoRedoActions(); });
    refreshUndoRedoActions();

    m_cameraModeSubscription = ScopedConnection(
        m_context.session().cameraModeChanged,
        m_context.session().cameraModeChanged.connect([this](const std::string& mode) {
            const bool is2d = mode == "2d";
            if (m_camera2DAction) {
                m_camera2DAction->setChecked(is2d);
                m_camera2DAction->setIcon(editorIcon(is2d ? QStringLiteral("viewport-2d.svg")
                                                          : QStringLiteral("viewport-3d.svg")));
            }
        }));
    m_tileModeSubscription = ScopedConnection(
        m_context.session().tileEditModeChanged,
        m_context.session().tileEditModeChanged.connect([this](bool active) {
            updateTileToolbar(active);
        }));
}

EditorWindow::~EditorWindow()
{
    if (m_safePointTimer) {
        m_safePointTimer->stop();
    }
    delete m_defaultLayoutState;
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
    auto* close = file->addAction(tr("Close"));
    connect(close, &QAction::triggered, this, &QWidget::close);

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

    auto* tile = m_menuBar->addMenu(tr("Tile"));
    auto* createTilemap = tile->addAction(tr("Create Tilemap..."));
    connect(createTilemap, &QAction::triggered, this, [this]() {
        if (m_tilePalette) {
            m_tilePalette->onNewTilemap();
        }
    });

    m_settingsMenu = m_menuBar->addMenu(tr("Settings"));
}

void EditorWindow::createWindowMenu()
{
    m_windowMenu = m_menuBar->addMenu(tr("Window"));
    m_resetLayoutAction = m_windowMenu->addAction(tr("Reset Layout"));
    connect(m_resetLayoutAction, &QAction::triggered, this, &EditorWindow::resetLayout);

    auto* themes = m_windowMenu->addMenu(tr("Theme"));
    const std::array<std::pair<const char*, const char*>, 3> themeChoices = {{
        {"cakery-light", QT_TR_NOOP("Light")},
        {"cakery-dark", QT_TR_NOOP("Dark")},
        {"cakery-color", QT_TR_NOOP("Color")},
    }};
    auto* themeGroup = new QActionGroup(themes);
    themeGroup->setExclusive(true);
    const std::string activeTheme = EditorConfig::self().themeName();
    for (const auto& [themeName, themeLabel] : themeChoices) {
        auto* action = themes->addAction(tr(themeLabel));
        action->setCheckable(true);
        action->setChecked(activeTheme == themeName);
        themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [themeName]() {
            if (auto* application = qobject_cast<EditorApplication*>(qApp)) {
                EditorConfig::self().setThemeName(themeName);
                application->applyTheme(QString::fromLatin1(themeName));
            }
        });
    }
    populatePanelMenus();
}

void EditorWindow::populatePanelMenus()
{
    const std::array<ads::CDockWidget*, 10> panelDocks = {
        m_hierarchyDock, m_inspectorDock, m_projectDock, m_consoleDock,
        m_terminalDock, m_historyDock, m_gameSettingsDock, m_engineSettingsDock,
        m_tilePaletteDock, m_tileLayersDock,
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

void EditorWindow::setupPanelToggle(ads::CDockWidget* dock)
{
    QAction* action = dock->toggleViewAction();
    const QIcon checkIcon = editorIcon(QStringLiteral("check.svg"));
    QPixmap blankPixmap(16, 16);
    blankPixmap.fill(Qt::transparent);
    const QIcon blankIcon(blankPixmap);
    const auto applyIcon = [action, checkIcon, blankIcon](bool checked) {
        action->setIcon(checked ? checkIcon : blankIcon);
    };
    applyIcon(action->isChecked());
    connect(dock, &ads::CDockWidget::viewToggled, this, applyIcon);
}

void EditorWindow::resetLayout()
{
    if (m_defaultLayoutState && !m_defaultLayoutState->isEmpty()) {
        m_dockManager->restoreState(*m_defaultLayoutState, 1);
    }
}

void EditorWindow::restoreLayoutState()
{
    if (m_layoutStatePath.isEmpty()) {
        return;
    }
    QFile file(m_layoutStatePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QByteArray state = file.readAll();
    if (!state.isEmpty() && !m_dockManager->restoreState(state, 1)) {
        resetLayout();
    }
}

void EditorWindow::saveLayoutState() const
{
    if (m_layoutStatePath.isEmpty() || !m_dockManager) {
        return;
    }
    QDir().mkpath(QFileInfo(m_layoutStatePath).absolutePath());
    QFile file(m_layoutStatePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(m_dockManager->saveState(1));
        file.close();
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

    const bool sim = m_context.capabilities().simulation;
    struct RuntimeButton {
        QString tooltip;
        QString icon;
        const char* command;
    };
    const RuntimeButton runtimeButtons[] = {
        {tr("Play"), QStringLiteral("play.svg"), "play"},
        {tr("Pause"), QStringLiteral("pause.svg"), "pause"},
        {tr("Stop"), QStringLiteral("square.svg"), "stop"},
    };
    for (const RuntimeButton& runtimeButton : runtimeButtons) {
        auto* button = new QToolButton(m_editorToolbar);
        button->setObjectName(QStringLiteral("runtimeToolButton"));
        button->setProperty("runtimeCommand", QString::fromLatin1(runtimeButton.command));
        button->setIcon(editorIcon(runtimeButton.icon));
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
#ifdef _WIN32
        UpdateWindowsFrameAttributes(this);
#endif
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

    m_camera2DAction = sceneToolbar->addAction(editorIcon(QStringLiteral("viewport-3d.svg")), QString());
    m_camera2DAction->setCheckable(true);
    m_camera2DAction->setChecked(false);
    m_camera2DAction->setToolTip(tr("Toggle 2D/3D view mode"));
    connect(m_camera2DAction, &QAction::triggered, this, [this](bool checked) {
        m_context.session().execute(EditorCommandMessage{
            "camera_mode", checked ? std::string("2d") : std::string("3d")});
    });

    auto* toolGroup = new QActionGroup(sceneToolbar);
    toolGroup->setExclusive(true);
    struct SceneTool {
        QString tooltip;
        QString icon;
        const char* mode;
    };
    const SceneTool sceneTools[] = {
        {tr("Hand"), QStringLiteral("hand.svg"), "none"},
        {tr("Move"), QStringLiteral("move-3d.svg"), "translate"},
        {tr("Rotate"), QStringLiteral("rotate-3d.svg"), "rotate"},
        {tr("Scale"), QStringLiteral("scale-3d.svg"), "scale"},
    };
    const bool sim = m_context.capabilities().simulation;
    for (const SceneTool& sceneTool : sceneTools) {
        auto* action = sceneToolbar->addAction(editorIcon(sceneTool.icon), QString());
        if (auto* toolButton = sceneToolbar->widgetForAction(action)) {
            toolButton->setProperty("sceneTool", QString::fromLatin1(sceneTool.mode));
        }
        action->setCheckable(true);
        action->setEnabled(sim);
        action->setToolTip(sceneTool.tooltip);
        toolGroup->addAction(action);
        if (sceneTool.mode[0] == 'n') action->setChecked(true);
        connect(action, &QAction::triggered, this, [this, mode = sceneTool.mode]() {
            m_context.session().execute(EditorCommandMessage{"gizmo_mode", mode});
        });
    }

    sceneToolbar->addSeparator();
    m_tileToolGroup = new QActionGroup(sceneToolbar);
    m_tileToolGroup->setExclusive(true);
    struct TileToolEntry {
        const char* tooltip;
        const char* icon;
        const char* mode;
    };
    const TileToolEntry tileTools[] = {
        {QT_TR_NOOP("Select"), "tile-select.svg", "select"},
        {QT_TR_NOOP("Brush"), "tile-brush.svg", "brush"},
        {QT_TR_NOOP("Fill"), "tile-fill.svg", "fill"},
        {QT_TR_NOOP("Eraser"), "tile-eraser.svg", "erase"},
        {QT_TR_NOOP("Rectangle"), "tile-rect.svg", "rect"},
        {QT_TR_NOOP("Line"), "tile-line.svg", "line"},
        {QT_TR_NOOP("Picker"), "tile-picker.svg", "picker"},
    };
    for (const TileToolEntry& tileTool : tileTools) {
        auto* action = sceneToolbar->addAction(
            editorIcon(QString::fromLatin1(tileTool.icon)), QString());
        action->setCheckable(true);
        action->setEnabled(false);
        action->setToolTip(tr(tileTool.tooltip));
        m_tileToolGroup->addAction(action);
        if (tileTool.mode[0] == 's') action->setChecked(true);
        connect(action, &QAction::triggered, this, [this, mode = tileTool.mode]() {
            m_context.session().execute(EditorCommandMessage{"tilemap.tool", mode});
        });
    }
    updateTileToolbar(false);
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
    connect(m_projectPanel, &ProjectPanel::assetSelected,
            m_inspector, &InspectorPanel::setSelectedAsset);
    connect(m_projectPanel, &ProjectPanel::assetSelectionCleared,
            m_inspector, &InspectorPanel::clearSelectedAsset);
    m_projectDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_projectDock, m_hierarchyDock->dockAreaWidget());

    m_consoleDock = new ads::CDockWidget(tr("Console"));
    m_consoleDock->setObjectName(QStringLiteral("Console"));
    m_console = new ConsolePanel(m_context, m_consoleDock);
    m_console->append(ConsoleLogLevel::Info, tr("Editor console ready"),
                      QStringLiteral("Cakery"));
    m_console->append(ConsoleLogLevel::Info, QString::fromStdString(m_context.diagnostic()),
                      QStringLiteral("Backend"));
    m_consoleDock->setWidget(m_console);
    m_consoleDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_consoleDock, m_sceneDock->dockAreaWidget());

    m_terminalDock = new ads::CDockWidget(tr("Terminal"));
    m_terminalDock->setObjectName(QStringLiteral("Terminal"));
    m_terminalDock->setWidget(unavailablePanel(tr("Terminal unavailable"),
        tr("Runtime command services are disabled in Editor-Only mode."), m_terminalDock));
    m_terminalDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_terminalDock, m_consoleDock->dockAreaWidget());

    m_historyDock = new ads::CDockWidget(tr("History"));
    m_historyDock->setObjectName(QStringLiteral("History"));
    m_historyPanel = new HistoryPanel(m_context, m_historyDock);
    m_historyDock->setWidget(m_historyPanel);
    m_historyDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_historyDock, m_consoleDock->dockAreaWidget());

    m_tilePaletteDock = new ads::CDockWidget(tr("Tile Palette"));
    m_tilePaletteDock->setObjectName(QStringLiteral("Tile Palette"));
    m_tilePalette = new TilePalettePanel(m_context, m_tilePaletteDock);
    m_tilePaletteDock->setWidget(m_tilePalette);
    m_tilePaletteDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_tilePaletteDock, m_historyDock->dockAreaWidget());

    m_tileLayersDock = new ads::CDockWidget(tr("Tile Layers"));
    m_tileLayersDock->setObjectName(QStringLiteral("Tile Layers"));
    m_tileLayers = new TileLayersPanel(m_context, m_tileLayersDock);
    m_tileLayersDock->setWidget(m_tileLayers);
    m_tileLayersDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_tileLayersDock, m_tilePaletteDock->dockAreaWidget());

    m_gameSettingsPanel = new SettingsPanel(nullptr);
    m_gameSettingsDock = new ads::CDockWidget(tr("Game Settings"));
    m_gameSettingsDock->setObjectName(QStringLiteral("Game Settings"));
    m_gameSettingsDock->setWidget(m_gameSettingsPanel);
    m_gameSettingsDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_gameSettingsDock, m_consoleDock->dockAreaWidget());
    m_gameSettingsDock->toggleView(false);

    m_engineSettingsPanel = new SettingsPanel(nullptr);
    m_engineSettingsDock = new ads::CDockWidget(tr("Engine Settings"));
    m_engineSettingsDock->setObjectName(QStringLiteral("Engine Settings"));
    m_engineSettingsDock->setWidget(m_engineSettingsPanel);
    m_engineSettingsDock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_engineSettingsDock, m_consoleDock->dockAreaWidget());
    m_engineSettingsDock->toggleView(false);
    connect(m_engineSettingsPanel, &SettingsPanel::saved, this, []() {
        EditorConfig::self().reload();
        if (auto* application = qobject_cast<EditorApplication*>(qApp)) {
            application->applyTheme(QString::fromStdString(EditorConfig::self().themeName()));
        }
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
        m_context.session().tick();
    });
    m_safePointTimer->start();
}

void EditorWindow::refreshUndoRedoActions()
{
    if (m_undoAction) m_undoAction->setEnabled(m_context.session().history().canUndo());
    if (m_redoAction) m_redoAction->setEnabled(m_context.session().history().canRedo());
}

void EditorWindow::updateTileToolbar(bool active)
{
    if (!m_tileToolGroup) {
        return;
    }
    for (QAction* action : m_tileToolGroup->actions()) {
        action->setEnabled(active);
    }
    if (active && m_tileToolGroup->checkedAction() == nullptr) {
        m_tileToolGroup->actions().first()->setChecked(true);
    }
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
    if (m_gameSettingsPanel) {
        m_gameSettingsPanel->setFilePath(
            QDir(QString::fromStdString(project.rootPath)).filePath(QStringLiteral("app_config.json")));
    }
    if (m_engineSettingsPanel) {
        m_engineSettingsPanel->setFallback([this]() { return EditorConfig::self().editorJson(); });
        m_engineSettingsPanel->setFilePath(
            QDir(QString::fromStdString(project.rootPath))
                .filePath(QStringLiteral("ProjectSettings/Editor/editor.json")));
    }
    if (m_console) {
        m_console->append(ConsoleLogLevel::Info, QString::fromStdString(m_context.diagnostic()),
                          QStringLiteral("Backend"));
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
    UpdateWindowsFrameAttributes(this);
#endif
}

void EditorWindow::closeEvent(QCloseEvent* event)
{
    if (m_closed) {
        event->accept();
        return;
    }
    if (m_context.session().documentModel().isDirty()) {
        const auto choice = QMessageBox::warning(
            this,
            tr("Unsaved Changes"),
            tr("The current scene has unsaved changes."),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (choice == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (choice == QMessageBox::Save) {
            std::string target;
            if (m_context.session().documentModel().path().empty()) {
                const QString path = QFileDialog::getSaveFileName(
                    this, tr("Save Scene As"), QString(), tr("Dodoe Scene (*.doscn)"));
                if (path.isEmpty()) {
                    event->ignore();
                    return;
                }
                target = path.toStdString();
            }
            if (!m_context.session().saveDocument(target)) {
                event->ignore();
                return;
            }
        }
    }
    m_closed = true;
    saveLayoutState();
    if (m_safePointTimer) {
        m_safePointTimer->stop();
    }
    m_context.session().shutdown();
    QMainWindow::closeEvent(event);
}

} // namespace cakery
