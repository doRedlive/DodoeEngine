// do@Redlive

#include "ScenePanel.h"
#include "framework/EditorContext.h"
#include "framework/camera/EditorCamera.h"
#include "framework/picking/PickingService.h"
#include "framework/gizmo/GizmoService.h"
#include "framework/selection/SelectionManager.h"
#include "framework/tilemap/TilePaintService.h"
#include "framework/tilemap/TileCoord.h"
#include "framework/core/UuidResolve.h"

#include "runtime/service/world/scene_importer.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/components/transform_component.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
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

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    buildToolbar();
    mainLayout->addStretch(1);
}

void ScenePanel::buildToolbar()
{
    auto* bar = new QHBoxLayout();
    bar->setContentsMargins(4, 2, 4, 2);

    m_shadingCombo = new QComboBox(this);
    m_shadingCombo->addItems({"Shaded", "Wireframe", "Albedo", "Normal", "Unlit"});
    bar->addWidget(m_shadingCombo);

    m_2dCheck = new QCheckBox("2D", this);
    bar->addWidget(m_2dCheck);

    m_gizmoCheck = new QCheckBox("Gizmos", this);
    m_gizmoCheck->setChecked(true);
    bar->addWidget(m_gizmoCheck);

    bar->addStretch();

    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        layout->insertLayout(0, bar);
    }
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

static bool tryTilemapInput(EditorContext& ctx, float x, float y, bool isDown, bool isDrag, bool isUp)
{
    auto tilemapUuid = ctx.selection().primary();
    if (!tilemapUuid.isValid()) return false;

    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto tilemapEntity = ResolveEntity(scene, tilemapUuid);
    if (!tilemapEntity.valid()) return false;

    auto* tm = tilemapEntity.tryGetComponent<dodoe::TilemapComponent>();
    if (!tm) return false;

    auto* tf = tilemapEntity.tryGetComponent<dodoe::TransformComponent>();
    dodoe::Matrix4f mapWorld(1.0f);
    if (tf) {
        mapWorld = glm::translate(dodoe::Matrix4f(1.0f), tf->position);
    }

    dodoe::Vector3f origin, dir;
    ctx.camera().screenToRay(x, y, origin, dir);

    dodoe::Vector3f worldPos = origin;
    if (std::abs(dir.z) > 0.0001f) {
        float t = -origin.z / dir.z;
        worldPos = origin + dir * t;
    }

    int cx = 0, cy = 0;
    if (!TileCoord::worldToCell(*tm, mapWorld, worldPos, cx, cy)) {
        return true;
    }

    if (isDown) {
        ctx.tilePaint().onCellDown(cx, cy);
    } else if (isDrag) {
        ctx.tilePaint().onCellDrag(cx, cy);
    } else if (isUp) {
        ctx.tilePaint().onCellUp();
    }
    return true;
}

void ScenePanel::mousePressEvent(QMouseEvent* event)
{
    float x = event->position().x();
    float y = event->position().y();

    if (event->button() == Qt::LeftButton) {
        if (tryTilemapInput(m_ctx, x, y, true, false, false)) {
            return;
        }
    }

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
    float x = event->position().x();
    float y = event->position().y();

    if (event->button() == Qt::LeftButton) {
        if (tryTilemapInput(m_ctx, x, y, false, false, true)) {
            m_ctx.camera().onMouseUp(0);
            return;
        }
    }

    m_ctx.gizmos().onMouseUp();

    int button = 0;
    if (event->button() == Qt::MiddleButton) button = 1;
    m_ctx.camera().onMouseUp(button);
}

void ScenePanel::mouseMoveEvent(QMouseEvent* event)
{
    float x = event->position().x();
    float y = event->position().y();

    if (event->buttons() & Qt::LeftButton) {
        if (tryTilemapInput(m_ctx, x, y, false, true, false)) {
            return;
        }
    }

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
