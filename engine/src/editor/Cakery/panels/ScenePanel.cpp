// do@Redlive

#include "ScenePanel.h"
#include "framework/EditorContext.h"
#include "framework/camera/EditorCamera.h"
#include "framework/picking/PickingService.h"
#include "framework/gizmo/GizmoService.h"
#include "framework/selection/SelectionManager.h"

#include "runtime/service/world/scene_importer.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cakery {

ScenePanel::ScenePanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setMinimumSize(320, 240);
}

ScenePanel::~ScenePanel() = default;

void ScenePanel::showEvent(QShowEvent* event)
{
    Panel::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        setupViewport();
    }
}

void ScenePanel::setupViewport()
{
    float dpr = devicePixelRatioF();
    m_ctx.camera().setViewportSize(static_cast<float>(width() * dpr),
                                    static_cast<float>(height() * dpr));
}

void ScenePanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
}

void ScenePanel::resizeEvent(QResizeEvent* event)
{
    Panel::resizeEvent(event);
    float dpr = devicePixelRatioF();
    m_ctx.camera().setViewportSize(static_cast<float>(event->size().width() * dpr),
                                    static_cast<float>(event->size().height() * dpr));
    m_ctx.onViewportResized(event->size().width(), event->size().height(), dpr);
}

void ScenePanel::mousePressEvent(QMouseEvent* event)
{
    float x = event->position().x();
    float y = event->position().y();

    if (m_ctx.gizmos().onMouseDown(x, y)) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        auto picked = m_ctx.picking().pick(x, y);
        if (picked.has_value()) {
            m_ctx.selection().select(*picked);
            return;
        }
    }

    int button = 0;
    if (event->button() == Qt::MiddleButton) button = 1;
    else if (event->button() == Qt::RightButton) button = 2;

    bool alt = (event->modifiers() & Qt::AltModifier) != 0;
    m_ctx.camera().onMouseDown(x, y, button, alt);
}

void ScenePanel::mouseReleaseEvent(QMouseEvent* event)
{
    m_ctx.gizmos().onMouseUp();

    int button = 0;
    if (event->button() == Qt::MiddleButton) button = 1;
    m_ctx.camera().onMouseUp(button);
}

void ScenePanel::mouseMoveEvent(QMouseEvent* event)
{
    float x = event->position().x();
    float y = event->position().y();

    if (m_ctx.gizmos().onMouseMove(x, y)) {
        return;
    }

    m_ctx.camera().onMouseMove(x, y);
}

void ScenePanel::wheelEvent(QWheelEvent* event)
{
    m_ctx.camera().onScroll(static_cast<float>(event->angleDelta().y()));
}

void ScenePanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void ScenePanel::dropEvent(QDropEvent* event)
{
    for (const auto& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            dodoe::SceneImporter::ImportAsset(url.toLocalFile().toStdString());
        }
    }
    event->acceptProposedAction();
}

} // namespace cakery
