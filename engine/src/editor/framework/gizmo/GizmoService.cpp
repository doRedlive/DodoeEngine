// do@Redlive

#include "GizmoService.h"
#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"
#include "framework/selection/SelectionManager.h"

#include "runtime/service/editor/debug_draw.h"

namespace cakery {

void GizmoService::update()
{
    if (m_mode == GizmoMode::None) return;

    auto& sel = m_ctx.selection();
    if (sel.empty()) return;

    // TODO: draw gizmo handles at selection centroid using DebugDraw
}

bool GizmoService::onMouseDown(float x, float y)
{
    (void)x; (void)y;
    if (m_mode == GizmoMode::None) return false;
    return false;
}

bool GizmoService::onMouseMove(float x, float y)
{
    (void)x; (void)y;
    if (!m_dragging) return false;
    return false;
}

void GizmoService::onMouseUp()
{
    if (m_dragging) {
        m_ctx.commands().endMerge();
        m_dragging = false;
    }
}

} // namespace cakery
