// do@Redlive

#include "dopch.h"

#include "SceneSurface.h"

#include "ui/common/EditorWorkspaceContext.h"
#include "ui/common/QtStringInterop.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
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
    setMinimumSize(480, 320);
    setAttribute(Qt::WA_NativeWindow);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setAcceptDrops(true);

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
    m_context.session().execute(SceneMouseDownCommand{
        static_cast<Float>(event->position().x()),
        static_cast<Float>(event->position().y()),
        toCameraButton(event->button()),
        event->modifiers().testFlag(Qt::AltModifier),
        event->modifiers().testFlag(Qt::ControlModifier),
        event->modifiers().testFlag(Qt::ShiftModifier)});
    QWidget::mousePressEvent(event);
}

void SceneSurface::mouseMoveEvent(QMouseEvent* event)
{
    m_context.session().execute(SceneMouseMoveCommand{
        static_cast<Float>(event->position().x()),
        static_cast<Float>(event->position().y()),
        event->modifiers().testFlag(Qt::ControlModifier),
        event->modifiers().testFlag(Qt::ShiftModifier),
        event->modifiers().testFlag(Qt::AltModifier)});
    QWidget::mouseMoveEvent(event);
}

void SceneSurface::mouseReleaseEvent(QMouseEvent* event)
{
    m_context.session().execute(SceneMouseUpCommand{toCameraButton(event->button())});
    QWidget::mouseReleaseEvent(event);
}

void SceneSurface::wheelEvent(QWheelEvent* event)
{
    m_context.session().execute(
        SceneMouseWheelCommand{static_cast<float>(event->angleDelta().y() / 120.0)});
    QWidget::wheelEvent(event);
}

void SceneSurface::keyPressEvent(QKeyEvent* event)
{
    m_context.session().execute(SceneKeyCommand{event->key(), true});
    QWidget::keyPressEvent(event);
}

void SceneSurface::keyReleaseEvent(QKeyEvent* event)
{
    m_context.session().execute(SceneKeyCommand{event->key(), false});
    QWidget::keyReleaseEvent(event);
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
        " color: #E8E8E8;"
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
        m_fpsLabel->setText(QStringLiteral("%1 FPS").arg(fps));
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
