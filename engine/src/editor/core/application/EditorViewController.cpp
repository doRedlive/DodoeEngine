// do@Redlive

#include "dopch.h"

#include "EditorViewController.h"

#include "core/EditorSession.h"

namespace cakery {

EditorViewController::EditorViewController(EditorSession& session)
    : m_session(session)
{
    m_cameraSubscription = ScopedConnection(
        m_session.cameraModeChanged,
        m_session.cameraModeChanged.connect([this](const String&) { refreshCamera(); }));
    m_gizmoSubscription = ScopedConnection(
        m_session.gizmoModeChanged,
        m_session.gizmoModeChanged.connect([this](const String&) { refreshGizmo(); }));
    refreshCamera();
    refreshGizmo();
}

EditorViewController::~EditorViewController() = default;

void EditorViewController::setCamera2D(bool ortho)
{
    m_session.execute(CameraModeCommand{ortho});
}

void EditorViewController::setGizmoMode(GizmoMode mode)
{
    m_session.execute(GizmoModeCommand{mode});
}

void EditorViewController::setGizmoSnap(bool enabled)
{
    m_session.execute(GizmoSnapCommand{enabled});
}

void EditorViewController::setGizmoSnapStep(float translate, float rotate, float scale)
{
    m_session.execute(GizmoSnapStepCommand{translate, rotate, scale});
}

void EditorViewController::refreshCamera()
{
    m_is2D = m_session.cameraMode() == "2d";
    cameraChanged.fire();
}

void EditorViewController::refreshGizmo()
{
    m_gizmoMode = gizmoModeFromName(m_session.gizmoMode());
    gizmoChanged.fire(m_gizmoMode);
}

} // namespace cakery
