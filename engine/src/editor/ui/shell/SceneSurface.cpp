// do@Redlive

#include "dopch.h"

#include "SceneSurface.h"

#include "ui/common/EditorWorkspaceContext.h"
#include "ui/common/QtStringInterop.h"

#include <QDragEnterEvent>
#include <QApplication>
#include <QAction>
#include <QFocusEvent>
#include <QDropEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace cakery {

namespace {

int toCameraButton(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) return 0;
    if (button == Qt::MiddleButton) return 1;
    return 2;
}

} // namespace

SceneSurface::SceneSurface(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    setMinimumSize(280, 200);
    setAttribute(Qt::WA_NativeWindow);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setAcceptDrops(true);
    m_gizmoSubscription = ScopedConnection(m_context.view().gizmoChanged,
        m_context.view().gizmoChanged.connect([this](GizmoMode mode) {
            setCursor(mode == GizmoMode::None ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }));

    initFpsOverlay();
}

void SceneSurface::attach()
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

void SceneSurface::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    attach();
    positionFpsLabel();
}

void SceneSurface::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    publishMetrics();
    positionFpsLabel();
}

void SceneSurface::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::LeftButton) {
        m_handPanning = m_context.view().gizmoMode() == GizmoMode::None
            && !event->modifiers().testFlag(Qt::AltModifier);
    }
    if (event->button() == Qt::RightButton) {
        m_rightPressPosition = event->position();
        m_rightNavigated = event->modifiers().testFlag(Qt::AltModifier);
    }
    if (m_handPanning) setCursor(Qt::ClosedHandCursor);
    m_context.session().execute(SceneMouseDownCommand{
        static_cast<Float>(event->position().x()),
        static_cast<Float>(event->position().y()),
        m_handPanning && event->button() == Qt::LeftButton ? 1 : toCameraButton(event->button()),
        event->modifiers().testFlag(Qt::AltModifier),
        event->modifiers().testFlag(Qt::ControlModifier),
        event->modifiers().testFlag(Qt::ShiftModifier)});
    event->accept();
}

void SceneSurface::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons().testFlag(Qt::RightButton)
        && (event->position() - m_rightPressPosition).manhattanLength() > 4.0) m_rightNavigated = true;
    m_context.session().execute(SceneMouseMoveCommand{
        static_cast<Float>(event->position().x()),
        static_cast<Float>(event->position().y()),
        event->modifiers().testFlag(Qt::ControlModifier),
        event->modifiers().testFlag(Qt::ShiftModifier),
        event->modifiers().testFlag(Qt::AltModifier)});
    event->accept();
}

void SceneSurface::mouseReleaseEvent(QMouseEvent* event)
{
    const bool endPan = m_handPanning && event->button() == Qt::LeftButton;
    m_context.session().execute(SceneMouseUpCommand{endPan ? 1 : toCameraButton(event->button())});
    if (endPan) {
        m_handPanning = false;
        setCursor(m_context.view().gizmoMode() == GizmoMode::None
            ? Qt::OpenHandCursor : Qt::ArrowCursor);
    }
    if (event->button() == Qt::RightButton && !m_rightNavigated) {
        QMenu menu(this);
        auto* frame = menu.addAction(tr("Frame Selected"));
        frame->setEnabled(m_context.selection().target() == EditorSelection::Target::Entity
            && m_context.selection().hasSelection());
        connect(frame, &QAction::triggered, this, [this]() {
            m_context.session().execute(SceneKeyCommand{Qt::Key_F, true});
        });
        menu.addSeparator();
        const StaticArray<Pair<const char*, GizmoMode>, 4> modes = {{
            {QT_TR_NOOP("Hand"), GizmoMode::None}, {QT_TR_NOOP("Move"), GizmoMode::Translate},
            {QT_TR_NOOP("Rotate"), GizmoMode::Rotate}, {QT_TR_NOOP("Scale"), GizmoMode::Scale}}};
        for (const auto& [name, mode] : modes) {
            auto* action = menu.addAction(tr(name));
            action->setCheckable(true);
            action->setChecked(m_context.view().gizmoMode() == mode);
            connect(action, &QAction::triggered, this, [this, mode]() { m_context.view().setGizmoMode(mode); });
        }
        menu.addSeparator();
        auto* mode2D = menu.addAction(tr("2D View"));
        mode2D->setCheckable(true);
        mode2D->setChecked(m_context.view().is2D());
        connect(mode2D, &QAction::triggered, this, [this](bool checked) { m_context.view().setCamera2D(checked); });
        menu.exec(event->globalPosition().toPoint());
    }
    event->accept();
}

void SceneSurface::wheelEvent(QWheelEvent* event)
{
    m_context.session().execute(
        SceneMouseWheelCommand{static_cast<float>(event->angleDelta().y() / 120.0)});
    event->accept();
}

void SceneSurface::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }
    if (QApplication::mouseButtons().testFlag(Qt::RightButton)) m_rightNavigated = true;
    // Tool hotkeys are local to the viewport; right-mouse navigation keeps WASDQE.
    if (event->modifiers() == Qt::NoModifier && QApplication::mouseButtons() == Qt::NoButton) {
        switch (event->key()) {
        case Qt::Key_Q: m_context.view().setGizmoMode(GizmoMode::None); break;
        case Qt::Key_W: m_context.view().setGizmoMode(GizmoMode::Translate); break;
        case Qt::Key_E: m_context.view().setGizmoMode(GizmoMode::Rotate); break;
        case Qt::Key_R: m_context.view().setGizmoMode(GizmoMode::Scale); break;
        case Qt::Key_F:
            m_context.session().execute(SceneKeyCommand{Qt::Key_F, true});
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
        }
        event->accept();
        return;
    }
    if (event->modifiers().testFlag(Qt::ControlModifier)
        || event->modifiers().testFlag(Qt::AltModifier)) {
        QWidget::keyPressEvent(event);
        return;
    }
    m_context.session().execute(SceneKeyCommand{event->key(), true});
    event->accept();
}

void SceneSurface::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;
    m_context.session().execute(SceneKeyCommand{event->key(), false});
    QWidget::keyReleaseEvent(event);
}

void SceneSurface::focusOutEvent(QFocusEvent* event)
{
    for (const int key : {Qt::Key_W, Qt::Key_A, Qt::Key_S, Qt::Key_D, Qt::Key_Q, Qt::Key_E, Qt::Key_Shift}) {
        m_context.session().execute(SceneKeyCommand{key, false});
    }
    for (int button = 0; button < 3; ++button) {
        m_context.session().execute(SceneMouseUpCommand{button});
    }
    m_handPanning = false;
    setCursor(m_context.view().gizmoMode() == GizmoMode::None
        ? Qt::OpenHandCursor : Qt::ArrowCursor);
    QWidget::focusOutEvent(event);
}

void SceneSurface::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (mime->hasFormat(QStringLiteral("application/x-cakery-asset")) || mime->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void SceneSurface::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    SceneImportAssetCommand command;
    command.x = event->position().x();
    command.y = event->position().y();
    if (mime->hasFormat(QStringLiteral("application/x-cakery-asset"))) {
        const QStringList entries = QString::fromUtf8(
            mime->data(QStringLiteral("application/x-cakery-asset"))).split('\n');
        for (int i = 1; i < entries.size(); i += 2) {
            command.paths.push_back(ToDodoeString(entries.at(i)));
        }
    } else if (mime->hasUrls()) {
        const QUrl url = mime->urls().first();
        if (url.isLocalFile()) {
            command.paths.push_back(ToDodoeString(url.toLocalFile()));
        }
    }
    m_context.session().execute(std::move(command));
    event->acceptProposedAction();
}

void SceneSurface::initFpsOverlay()
{
    m_fpsLabel = new QLabel(this);
    m_fpsLabel->setObjectName(QStringLiteral("sceneFpsOverlay"));
    m_fpsLabel->setAttribute(Qt::WA_NativeWindow);
    m_fpsLabel->setAttribute(Qt::WA_DontCreateNativeAncestors);
    m_fpsLabel->setAttribute(Qt::WA_TranslucentBackground);
    m_fpsLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_fpsLabel->setAutoFillBackground(false);
    m_fpsLabel->setStyleSheet(QStringLiteral(
        "QLabel#sceneFpsOverlay {"
        " color: #9b9db4; background: #21222c;"
        " border: none; border-radius: 2px; padding: 2px 5px;"
        " font-family: Consolas;"
        " font-size: 11px;"
        "}"));
    m_fpsLabel->hide();

    m_fpsTimer = new QTimer(this);
    m_fpsTimer->setInterval(250);
    connect(m_fpsTimer, &QTimer::timeout, this, [this]() { refreshFps(); });
    m_fpsTimer->start();
}

void SceneSurface::positionFpsLabel()
{
    if (!m_fpsLabel) return;
    const QSize hint = m_fpsLabel->sizeHint();
    const int x = width() - hint.width() - 12;
    const int y = height() - hint.height() - 12;
    m_fpsLabel->setGeometry(std::max(0, x), std::max(0, y), hint.width(), hint.height());
    m_fpsLabel->raise();
}

void SceneSurface::refreshFps()
{
    int fps = 0;
    if (m_context.session().queryFps(fps) && fps > 0) {
        m_fpsLabel->setText(QStringLiteral("%1 FPS  |  %2 x %3").arg(fps)
            .arg(qRound(width() * devicePixelRatioF())).arg(qRound(height() * devicePixelRatioF())));
        m_fpsLabel->adjustSize();
        m_fpsLabel->show();
        positionFpsLabel();
        return;
    }
    Json stats;
    if (!m_context.session().queryFrameStats(stats) ||
        !stats.value("available", false)) {
        m_fpsLabel->hide();
        return;
    }
    const auto framesIt = stats.find("frames");
    double deltaSum = 0.0;
    int samples = 0;
    if (framesIt != stats.end() && framesIt->is_array()) {
        for (auto it = framesIt->rbegin(); it != framesIt->rend() && samples < 16; ++it) {
            const auto deltaIt = it->find("delta_ms");
            if (deltaIt == it->end() || !deltaIt->is_number()) continue;
            const double delta = deltaIt->get<double>();
            if (delta > 0.0) {
                deltaSum += delta;
                ++samples;
            }
        }
    }
    if (samples == 0) {
        m_fpsLabel->hide();
        return;
    }
    const double avgDelta = deltaSum / static_cast<double>(samples);
    const double telemetryFps = 1000.0 / avgDelta;
    m_fpsLabel->setText(QStringLiteral("%1 FPS  |  %2 ms")
        .arg(telemetryFps, 0, 'f', 1)
        .arg(avgDelta, 0, 'f', 2));
    m_fpsLabel->adjustSize();
    m_fpsLabel->show();
    positionFpsLabel();
}

void SceneSurface::publishMetrics()
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

} // namespace cakery
