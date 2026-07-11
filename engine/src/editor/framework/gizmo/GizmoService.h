// do@Redlive

#pragma once

#include "runtime/core/utils/uuid.h"

namespace cakery {

class EditorContext;

enum class GizmoMode  { None, Translate, Rotate, Scale };
enum class GizmoSpace { World, Local };

class GizmoService {
public:
    explicit GizmoService(EditorContext& ctx) : m_ctx(ctx) {}

    void setMode(GizmoMode m)   { m_mode = m; }
    GizmoMode mode() const      { return m_mode; }
    void setSpace(GizmoSpace s) { m_space = s; }

    void update();

    bool onMouseDown(float x, float y);
    bool onMouseMove(float x, float y);
    void onMouseUp();

    bool isDragging() const { return m_dragging; }

private:
    EditorContext& m_ctx;
    GizmoMode  m_mode  = GizmoMode::Translate;
    GizmoSpace m_space = GizmoSpace::World;
    bool m_dragging = false;
};

} // namespace cakery
