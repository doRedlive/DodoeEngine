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
#include "core/document/EditorDocumentModel.h"
#include "cakery/ui/panels/ConsolePanel.h"
#include "cakery/ui/panels/HierarchyPanel.h"
#include "cakery/ui/panels/HistoryPanel.h"
#include "cakery/ui/panels/InspectorPanel.h"
#include "cakery/ui/panels/ProjectPanel.h"
#include "cakery/ui/panels/SettingsPanel.h"
#include "cakery/ui/panels/TileLayersPanel.h"
#include "cakery/ui/panels/TilePalettePanel.h"
#include "cakery/ui/inspector/EditorRemoteWidget.h"

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

int toCameraButton(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) return 0;
    if (button == Qt::MiddleButton) return 1;
    return 2;
}

class FloatingWindowTitleBar final : public QWidget
{
public:
    explicit FloatingWindowTitleBar(QWidget* window, bool allowMaximize = true)
        : QWidget(window), m_window(window), m_allowMaximize(allowMaximize)
    {
        setObjectName(QStringLiteral("editorTitleBar"));
        setAttribute(Qt::WA_StyledBackground, true);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 2, 0, 2);
        layout->setSpacing(4);

        m_title = new QLabel(window->windowTitle(), this);
        m_title->setObjectName(QStringLiteral("editorWindowTitle"));
        m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(m_title);
        layout->addStretch();

        auto addButton = [this, layout](const QString& objectName, const QString& icon,
                                        const QString& tooltip, auto callback) {
            auto* button = new QToolButton(this);
            button->setObjectName(objectName);
            button->setIcon(editorThemedIcon(icon));
            button->setIconSize(QSize(16, 16));
            button->setToolTip(tooltip);
            connect(button, &QToolButton::clicked, this, callback);
            layout->addWidget(button);
            return button;
        };

        if (m_allowMaximize) {
            addButton(QStringLiteral("windowMinButton"), QStringLiteral("minus.svg"),
                      tr("Minimize"), [this]() { m_window->showMinimized(); });
            m_maxButton = addButton(QStringLiteral("windowMaxButton"),
                                    QStringLiteral("maximize-2.svg"), tr("Maximize"),
                                    [this]() { toggleMaximize(); });
        }
        addButton(QStringLiteral("windowCloseButton"), QStringLiteral("x.svg"),
                  tr("Close"), [this]() { m_window->close(); });

        connect(window, &QWidget::windowTitleChanged, this, [this](const QString& title) {
            m_title->setText(title);
        });
        connect(window, &QObject::destroyed, this, [this]() { m_window = nullptr; });
        updateMaximizeButton();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_window && m_window->windowHandle()) {
            m_window->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (m_allowMaximize && event->button() == Qt::LeftButton) {
            toggleMaximize();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    void toggleMaximize()
    {
        if (!m_window) {
            return;
        }
        if (m_window->isMaximized()) {
            m_window->showNormal();
        } else {
            m_window->showMaximized();
        }
        updateMaximizeButton();
    }

    void updateMaximizeButton()
    {
        if (!m_maxButton || !m_window) {
            return;
        }
        const bool restore = m_window->isMaximized();
        m_maxButton->setIcon(editorThemedIcon(restore ? QStringLiteral("minimize-2.svg")
                                                   : QStringLiteral("maximize-2.svg")));
        m_maxButton->setToolTip(restore ? tr("Restore Down") : tr("Maximize"));
    }

    QWidget* m_window = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_maxButton = nullptr;
    bool m_allowMaximize = true;
};

class UnsavedChangesDialog final : public QDialog
{
public:
    explicit UnsavedChangesDialog(QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(tr("Unsaved Changes"));
        setObjectName(QStringLiteral("unsavedChangesDialog"));
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_StyledBackground, true);
        setModal(true);
        setMinimumWidth(430);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        root->addWidget(new FloatingWindowTitleBar(this, false));

        auto* body = new QWidget(this);
        body->setObjectName(QStringLiteral("unsavedChangesBody"));
        auto* bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(20, 18, 20, 18);
        bodyLayout->setSpacing(16);

        auto* messageRow = new QHBoxLayout();
        messageRow->setSpacing(16);
        auto* icon = new QLabel(body);
        icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(QSize(42, 42)));
        icon->setFixedSize(42, 42);
        messageRow->addWidget(icon, 0, Qt::AlignTop);
        auto* message = new QLabel(tr("The current scene has unsaved changes."), body);
        message->setObjectName(QStringLiteral("unsavedChangesMessage"));
        message->setWordWrap(true);
        messageRow->addWidget(message, 1, Qt::AlignVCenter);
        bodyLayout->addLayout(messageRow);

        auto* buttons = new QHBoxLayout();
        buttons->addStretch();
        auto addButton = [this, buttons](const QString& text, QMessageBox::StandardButton result) {
            auto* button = new QPushButton(text, this);
            button->setMinimumWidth(72);
            connect(button, &QPushButton::clicked, this, [this, result]() {
                m_result = result;
                accept();
            });
            buttons->addWidget(button);
        };
        addButton(tr("Save"), QMessageBox::Save);
        addButton(tr("Discard"), QMessageBox::Discard);
        addButton(tr("Cancel"), QMessageBox::Cancel);
        bodyLayout->addLayout(buttons);
        root->addWidget(body);
    }

    QMessageBox::StandardButton result() const { return m_result; }

private:
    QMessageBox::StandardButton m_result = QMessageBox::Cancel;
};

void setupFramelessMessageBox(QMessageBox* messageBox)
{
    if (!messageBox || messageBox->property("cakeryCustomTitleBar").toBool()) {
        return;
    }

    auto* grid = qobject_cast<QGridLayout*>(messageBox->layout());
    if (!grid) {
        return;
    }

    struct GridItem {
        QLayoutItem* item = nullptr;
        int row = 0;
        int column = 0;
        int rowSpan = 1;
        int columnSpan = 1;
    };
    std::vector<GridItem> items;
    items.reserve(static_cast<std::size_t>(grid->count()));
    int columnCount = 1;
    for (int index = 0; index < grid->count(); ++index) {
        int row = 0;
        int column = 0;
        int rowSpan = 1;
        int columnSpan = 1;
        grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
        items.push_back({nullptr, row, column, rowSpan, columnSpan});
        columnCount = qMax(columnCount, column + columnSpan);
    }
    for (int index = static_cast<int>(items.size()) - 1; index >= 0; --index) {
        items[static_cast<std::size_t>(index)].item = grid->takeAt(index);
    }

    messageBox->setProperty("cakeryCustomTitleBar", true);
    messageBox->setAttribute(Qt::WA_TranslucentBackground, true);
    messageBox->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    grid->addWidget(new FloatingWindowTitleBar(messageBox, false), 0, 0, 1, columnCount);
    for (const auto& entry : items) {
        grid->addItem(entry.item, entry.row + 1, entry.column,
                      entry.rowSpan, entry.columnSpan);
    }
    messageBox->show();
}

ads::CFloatingDockContainer* floatingWindowAncestor(QObject* object)
{
    auto* widget = qobject_cast<QWidget*>(object);
    while (widget) {
        if (auto* floating = qobject_cast<ads::CFloatingDockContainer*>(widget)) {
            return floating;
        }
        widget = widget->parentWidget();
    }
    return nullptr;
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
        setAcceptDrops(true);
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
        const QString payload = QStringLiteral("%1,%2,%3,%4,%5,%6")
            .arg(event->position().x())
            .arg(event->position().y())
            .arg(toCameraButton(event->button()))
            .arg(event->modifiers().testFlag(Qt::AltModifier) ? 1 : 0)
            .arg(event->modifiers().testFlag(Qt::ControlModifier) ? 1 : 0)
            .arg(event->modifiers().testFlag(Qt::ShiftModifier) ? 1 : 0);
        m_context.session().execute(EditorCommandMessage{"scene_mouse_down", payload.toStdString()});
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!m_context.capabilities().scenePreview) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        const QString payload = QStringLiteral("%1,%2,%3,%4,%5")
            .arg(event->position().x())
            .arg(event->position().y())
            .arg(event->modifiers().testFlag(Qt::ControlModifier) ? 1 : 0)
            .arg(event->modifiers().testFlag(Qt::ShiftModifier) ? 1 : 0)
            .arg(event->modifiers().testFlag(Qt::AltModifier) ? 1 : 0);
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

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        const QMimeData* mime = event->mimeData();
        if (mime->hasFormat(QStringLiteral("application/x-cakery-asset")) || mime->hasUrls()) {
            event->acceptProposedAction();
            return;
        }
        QWidget::dragEnterEvent(event);
    }

    void dropEvent(QDropEvent* event) override
    {
        const QMimeData* mime = event->mimeData();
        QString payload = QStringLiteral("%1,%2\n")
            .arg(event->position().x()).arg(event->position().y());
        if (mime->hasFormat(QStringLiteral("application/x-cakery-asset"))) {
            payload += QString::fromUtf8(mime->data(QStringLiteral("application/x-cakery-asset")));
        } else if (mime->hasUrls()) {
            const QUrl url = mime->urls().first();
            if (url.isLocalFile()) {
                payload += QStringLiteral("\n") + url.toLocalFile();
            }
        }
        m_context.session().execute(EditorCommandMessage{"scene.import_asset", payload.toStdString()});
        event->acceptProposedAction();
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
    m_defaultLayoutState = new QByteArray(m_dockManager->saveState(1));
    const QString safeName = QApplication::applicationName().toLower().replace(
        QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    m_layoutStatePath = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(safeName + QStringLiteral("_layout.state"));
    restoreLayoutState();
    startSafePointTimer();

    m_assetImportTimer = new QTimer(this);
    m_assetImportTimer->setInterval(100);
    connect(m_assetImportTimer, &QTimer::timeout, this, &EditorWindow::updateAssetImportProgress);
    m_assetImportTimer->start();

    m_historySubscription = m_context.session().history().subscribe([this]() { refreshUndoRedoActions(); });
    refreshUndoRedoActions();

    m_context.confirmUnsavedChanges = [this]() { return promptUnsavedChanges(); };

    m_playStateSubscription = ScopedConnection(
        m_context.session().playStateChanged,
        m_context.session().playStateChanged.connect([this](PlayState) {
            updateRuntimeControls();
        }));

    m_documentSubscription = m_context.session().documentModel().subscribe(
        [this]() { updateWindowTitle(); });
    updateWindowTitle();

    m_gizmoModeSubscription = ScopedConnection(
        m_context.session().gizmoModeChanged,
        m_context.session().gizmoModeChanged.connect([this](const std::string& mode) {
            const std::array<const char*, 4> modes = {"none", "translate", "rotate", "scale"};
            for (std::size_t i = 0; i < modes.size(); ++i) {
                if (m_sceneToolActions[i]) {
                    QSignalBlocker blocker(m_sceneToolActions[i]);
                    m_sceneToolActions[i]->setChecked(mode == modes[i]);
                }
            }
        }));

    updateRuntimeControls();

    m_cameraModeSubscription = ScopedConnection(
        m_context.session().cameraModeChanged,
        m_context.session().cameraModeChanged.connect([this](const std::string& mode) {
            const bool is2d = mode == "2d";
            if (m_camera2DAction) {
                m_camera2DAction->setChecked(is2d);
                m_camera2DAction->setIcon(editorThemedIcon(
                    is2d ? QStringLiteral("viewport-2d.svg") : QStringLiteral("viewport-3d.svg")));
                m_camera2DAction->setToolTip(is2d
                    ? tr("Switch to 3D view (perspective)")
                    : tr("Switch to 2D view (orthographic)"));
            }
        }));

    m_missingAssetRefsSubscription = ScopedConnection(
        m_context.session().missingAssetReferencesDetected,
        m_context.session().missingAssetReferencesDetected.connect([this](std::size_t count) {
            if (count == 0 || !m_console) {
                return;
            }
            m_console->append(ConsoleLogLevel::Warning,
                              tr("%1 scene asset reference(s) could not be resolved. "
                                 "Check the Inspector for missing references.").arg(count),
                              QStringLiteral("Assets"));
        }));
}

EditorWindow::~EditorWindow()
{
    qApp->removeEventFilter(this);
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
        if (!m_context.session().saveDocument(std::string())) {
            QMessageBox::warning(this, tr("Save Scene"),
                                 tr("Could not save the scene to '%1'.")
                                     .arg(QString::fromStdString(
                                         m_context.session().documentModel().path().string())));
            return;
        }
        updateWindowTitle();
    });

    auto* saveAs = file->addAction(tr("Save Scene As..."));
    connect(saveAs, &QAction::triggered, this, [this]() {
        if (!m_context.session().documentModel().hasDocument()) {
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Save Scene As"), QString(), tr("Dodoe Scene (*.doscn)"));
        if (path.isEmpty()) {
            return;
        }
        if (!m_context.session().saveDocument(path.toStdString())) {
            QMessageBox::warning(this, tr("Save Scene"),
                                 tr("Could not save the scene to '%1'.").arg(path));
            return;
        }
        updateWindowTitle();
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
    m_runtimeMenuActions[0] = runtime->addAction(tr("Play"));
    m_runtimeMenuActions[0]->setEnabled(sim);
    connect(m_runtimeMenuActions[0], &QAction::triggered, this, [this]() {
        m_context.session().execute(EditorCommandMessage{"play", ""});
    });
    m_runtimeMenuActions[1] = runtime->addAction(tr("Pause"));
    m_runtimeMenuActions[1]->setEnabled(sim);
    connect(m_runtimeMenuActions[1], &QAction::triggered, this, [this]() {
        if (m_context.session().playState() == PlayState::Paused) {
            m_context.session().execute(EditorCommandMessage{"resume", ""});
        } else {
            m_context.session().execute(EditorCommandMessage{"pause", ""});
        }
    });
    m_runtimeMenuActions[2] = runtime->addAction(tr("Stop"));
    m_runtimeMenuActions[2]->setEnabled(sim);
    connect(m_runtimeMenuActions[2], &QAction::triggered, this, &EditorWindow::stopPlayWithPrompt);
    if (!sim) {
        for (QAction* action : runtime->actions()) {
            action->setToolTip(tr("Runtime backend is unavailable in Editor-Only mode"));
        }
    }

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

    std::vector<std::string> paths;
    if (!m_context.session().listToolActions(paths)) {
        auto* unavailable = m_toolsMenu->addAction(tr("No script tools available"));
        unavailable->setEnabled(false);
        return;
    }

    struct Node {
        std::map<QString, Node> children;
        std::vector<QString> leaves;
    };
    Node root;
    for (const std::string& rawPath : paths) {
        const QString path = QString::fromStdString(rawPath);
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
                if (!m_context.session().invokeToolAction(fullPath.toStdString())) {
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

void EditorWindow::refreshEditorWindowsMenu()
{
    if (!m_editorWindowsMenu) {
        return;
    }
    m_editorWindowsMenu->clear();

    std::vector<std::pair<std::string, std::string>> windows;
    if (!m_context.session().listEditorWindows(windows) || windows.empty()) {
        auto* unavailable = m_editorWindowsMenu->addAction(tr("No script windows available"));
        unavailable->setEnabled(false);
        return;
    }

    for (const auto& [id, title] : windows) {
        auto* action = m_editorWindowsMenu->addAction(QString::fromStdString(title));
        connect(action, &QAction::triggered, this, [this, id, title]() {
            openEditorWindow(QString::fromStdString(id), QString::fromStdString(title));
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
    const std::string windowId = id.toStdString();
    auto* remote = new EditorRemoteWidget(
        [this, windowId](nlohmann::json& out) {
            return m_context.session().getEditorWindowUI(windowId, out);
        },
        [this, windowId](const std::string& controlId, const std::string& eventName,
                         const nlohmann::json& value) {
            m_context.session().dispatchEditorEvent("window", windowId, controlId, eventName, value);
        },
        dock);
    dock->setWidget(remote);
    dock->setFeature(ads::CDockWidget::DockWidgetPinnable, true);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, dock);
    m_editorWindowDocks.emplace_back(id, dock);

    connect(dock, &QObject::destroyed, this, [this, id]() {
        m_editorWindowDocks.erase(
            std::remove_if(m_editorWindowDocks.begin(), m_editorWindowDocks.end(),
                           [&id](const std::pair<QString, ads::CDockWidget*>& entry) {
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
            if (std::string(command) == "stop") {
                stopPlayWithPrompt();
                return;
            }
            if (std::string(command) == "pause") {
                if (m_context.session().playState() == PlayState::Paused) {
                    m_context.session().execute(EditorCommandMessage{"resume", ""});
                } else {
                    m_context.session().execute(EditorCommandMessage{"pause", ""});
                }
                return;
            }
            m_context.session().execute(EditorCommandMessage{command, ""});
        });
        if (std::string(runtimeButton.command) == "play") {
            m_playButton = button;
        } else if (std::string(runtimeButton.command) == "pause") {
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
#ifdef _WIN32
        UpdateWindowsFrameAttributes(this);
#endif
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
    int toolIndex = 0;
    for (const SceneTool& sceneTool : sceneTools) {
        auto* action = sceneToolbar->addAction(editorThemedIcon(sceneTool.icon), QString());
        if (auto* toolButton = sceneToolbar->widgetForAction(action)) {
            toolButton->setProperty("sceneTool", QString::fromLatin1(sceneTool.mode));
        }
        action->setCheckable(true);
        action->setEnabled(sim);
        action->setToolTip(sceneTool.tooltip);
        toolGroup->addAction(action);
        if (toolIndex < 4) {
            m_sceneToolActions[toolIndex] = action;
        }
        if (std::string(sceneTool.mode) == "translate") action->setChecked(true);
        connect(action, &QAction::triggered, this, [this, mode = sceneTool.mode]() {
            m_context.session().execute(EditorCommandMessage{"gizmo_mode", mode});
        });
        ++toolIndex;
    }

    sceneToolbar->addSeparator();

    auto* snapAction = sceneToolbar->addAction(editorThemedIcon(QStringLiteral("grid.svg")), QString());
    snapAction->setCheckable(true);
    snapAction->setEnabled(sim);
    snapAction->setToolTip(tr("Toggle snapping (hold Ctrl while dragging to snap temporarily)"));
    connect(snapAction, &QAction::triggered, this, [this](bool checked) {
        m_context.session().execute(EditorCommandMessage{"gizmo_snap", checked ? "1" : "0"});
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
        box->setEnabled(sim);
        return box;
    };
    QDoubleSpinBox* translateStep = makeStepBox(QStringLiteral("T"), 0.25, 0.05, tr("Translate snap step"));
    QDoubleSpinBox* rotateStep = makeStepBox(QStringLiteral("R"), 15.0, 1.0, tr("Rotate snap step (degrees)"));
    QDoubleSpinBox* scaleStep = makeStepBox(QStringLiteral("S"), 0.1, 0.05, tr("Scale snap step"));
    sceneToolbar->addWidget(translateStep);
    sceneToolbar->addWidget(rotateStep);
    sceneToolbar->addWidget(scaleStep);

    const auto pushSnapSteps = [this, translateStep, rotateStep, scaleStep]() {
        const QString payload = QStringLiteral("%1,%2,%3")
            .arg(translateStep->value())
            .arg(rotateStep->value())
            .arg(scaleStep->value());
        m_context.session().execute(EditorCommandMessage{"gizmo_snap_step", payload.toStdString()});
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
    connect(m_projectPanel, &ProjectPanel::assetSelected, m_inspector,
            [this](const AssetBrowserEntry& asset) {
        m_context.session().selection().setAsset(asset.uuid);
    });
    connect(m_projectPanel, &ProjectPanel::assetSelectionCleared, m_inspector, [this]() {
        m_context.session().selection().clear();
    });
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

void EditorWindow::stopPlayWithPrompt()
{
    EditorSession& session = m_context.session();
    if (session.playState() == PlayState::Edit) {
        return;
    }
    bool keepPlayChanges = false;
    if (session.playDocumentEdited()) {
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
    session.stopPlay(keepPlayChanges);
}

void EditorWindow::updateRuntimeControls()
{
    const bool sim = m_context.capabilities().simulation;
    const PlayState state = m_context.session().playState();
    if (m_playButton) {
        m_playButton->setEnabled(sim && state == PlayState::Edit);
        m_playButton->setToolTip(sim
            ? tr("Play") : tr("Runtime scene tools are unavailable in Editor-Only mode"));
    }
    if (m_pauseButton) {
        const bool paused = state == PlayState::Paused;
        m_pauseButton->setEnabled(sim && state != PlayState::Edit);
        m_pauseButton->setIcon(editorIcon(
            paused ? QStringLiteral("play.svg") : QStringLiteral("pause.svg")));
        m_pauseButton->setToolTip(sim
            ? (paused ? tr("Resume") : tr("Pause"))
            : tr("Runtime scene tools are unavailable in Editor-Only mode"));
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(sim && state != PlayState::Edit);
        m_stopButton->setToolTip(sim
            ? tr("Stop") : tr("Runtime scene tools are unavailable in Editor-Only mode"));
    }
    if (m_runtimeMenuActions[0]) {
        m_runtimeMenuActions[0]->setEnabled(sim && state == PlayState::Edit);
    }
    if (m_runtimeMenuActions[1]) {
        const bool paused = state == PlayState::Paused;
        m_runtimeMenuActions[1]->setText(paused ? tr("Resume") : tr("Pause"));
        m_runtimeMenuActions[1]->setEnabled(sim && state != PlayState::Edit);
    }
    if (m_runtimeMenuActions[2]) {
        m_runtimeMenuActions[2]->setEnabled(sim && state != PlayState::Edit);
    }
}

void EditorWindow::updateWindowTitle()
{
    const EditorDocumentModel& model = m_context.session().documentModel();
    if (!model.hasDocument()) {
        setWindowTitle(QApplication::applicationName());
        return;
    }
    const QString scene = QString::fromStdString(model.name());
    const QString marker = model.isDirty() ? QStringLiteral(" *") : QString();
    setWindowTitle(QStringLiteral("%1 - %2%3")
                       .arg(QApplication::applicationName(), scene, marker));
}

bool EditorWindow::promptUnsavedChanges()
{
    EditorSession& session = m_context.session();
    if (!session.documentModel().hasDocument() || !session.documentModel().isDirty()) {
        return true;
    }
    UnsavedChangesDialog dialog(this);
    dialog.adjustSize();
    dialog.move(frameGeometry().center() - dialog.rect().center());
    dialog.exec();
    const auto choice = dialog.result();
    if (choice == QMessageBox::Cancel) {
        return false;
    }
    if (choice == QMessageBox::Save) {
        std::string target;
        if (session.documentModel().path().empty()) {
            const QString path = QFileDialog::getSaveFileName(
                this, tr("Save Scene As"), QString(), tr("Dodoe Scene (*.doscn)"));
            if (path.isEmpty()) {
                return false;
            }
            target = path.toStdString();
        }
        if (!session.saveDocument(target)) {
            QMessageBox::warning(this, tr("Save Scene"),
                                 tr("Could not save the scene to '%1'.")
                                     .arg(QString::fromStdString(
                                         session.documentModel().path().string())));
            return false;
        }
        updateWindowTitle();
    }
    return true;
}

void EditorWindow::refreshUndoRedoActions()
{
    if (m_undoAction) m_undoAction->setEnabled(m_context.session().history().canUndo());
    if (m_redoAction) m_redoAction->setEnabled(m_context.session().history().canRedo());
}

void EditorWindow::updateAssetImportProgress()
{
    const bool pending = m_context.session().isAssetRefreshPending();
    std::size_t done = 0;
    std::size_t total = 0;
    m_context.session().assetRefreshProgress(done, total);

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
        if (!m_context.session().bootEngine()) {
            if (m_console) {
                m_console->append(ConsoleLogLevel::Error,
                                  QString::fromStdString(m_context.diagnostic()),
                                  QStringLiteral("Backend"));
            }
            return false;
        }
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
    if (!promptUnsavedChanges()) {
        event->ignore();
        return;
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
