// do@Redlive
#pragma once
#include "dopch.h"

#include "core/bridge/EditorCommands.h"
#include "core/Signal.h"

namespace cakery {

class EditorSession;

class EditorViewController {
public:
    explicit EditorViewController(EditorSession& session);
    ~EditorViewController();

    bool is2D() const { return m_is2D; }
    GizmoMode gizmoMode() const { return m_gizmoMode; }

    void setCamera2D(bool ortho);
    void setGizmoMode(GizmoMode mode);
    void setGizmoSnap(bool enabled);
    void setGizmoSnapStep(float translate, float rotate, float scale);

    Signal<> cameraChanged;
    Signal<GizmoMode> gizmoChanged;

private:
    void refreshCamera();
    void refreshGizmo();

    EditorSession& m_session;
    bool m_is2D = false;
    GizmoMode m_gizmoMode = GizmoMode::Translate;
    ScopedConnection m_cameraSubscription;
    ScopedConnection m_gizmoSubscription;
};

} // namespace cakery
