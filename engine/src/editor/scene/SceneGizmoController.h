// do@Redlive
#pragma once
#include "dopch.h"

#include "core/bridge/EditorTypes.h"
#include "runtime/core/channel/gizmo_channel.h"
#include "runtime/core/math/math.h"

#include <functional>

namespace dodoe { class Scene; }

namespace cakery {

class EditorCamera;

// A borrowed view of the current editor scene, valid for one call. The gizmo
// controller has no dependency on RuntimeBackend or the runtime application.
struct SceneGizmoContext {
    dodoe::Scene* scene;
    EditorCamera* camera;
    UInt64 selectedUuid;
    const DynamicArray<UInt64>& selection;
    bool editing;
    bool snapModifier;
    const std::function<void(const BackendEventMessage&)>& publish;
};

class SceneGizmoController final {
public:
    void draw(const SceneGizmoContext& context, dodoe::GizmoChannelData& data);
    bool press(const SceneGizmoContext& context, float x, float y);
    void hover(const SceneGizmoContext& context, float x, float y);
    void updateDrag(const SceneGizmoContext& context, float x, float y);
    void endDrag(const SceneGizmoContext& context);
    void cancelDrag(const SceneGizmoContext& context);
    void resetInteraction();
    bool dragging() const { return m_dragAxis >= 0; }
    void resetHover() { m_hoverAxis = -1; }
    void setMode(const String& mode) { m_gizmoMode = mode; resetHover(); }
    const String& mode() const { return m_gizmoMode; }
    void setSnapEnabled(bool enabled) { m_snapEnabled = enabled; }
    void setSnapSteps(float translate, float rotate, float scale);

private:
    float computeGizmoScale(const SceneGizmoContext& context, const dodoe::Vector3f& position) const;
    void drawSelectionHighlight(const SceneGizmoContext& context, dodoe::GizmoChannelData& data);
    int hitTest(const SceneGizmoContext& context, float x, float y);
    void beginDrag(const SceneGizmoContext& context, int axis, float x, float y);
    struct TransformUpdate {
        UInt64 uuid = 0;
        dodoe::Vector3f position;
        dodoe::Vector3f rotation;
        dodoe::Vector3f scale;
    };
    void emitTransformChange(const SceneGizmoContext& context, const dodoe::Vector3f& position,
                             const dodoe::Vector3f& rotation, const dodoe::Vector3f& scale);
    void emitTransformChanges(const SceneGizmoContext& context, const DynamicArray<TransformUpdate>& updates);

    String m_gizmoMode = "translate";
    float m_gizmoScale = 1.0f;
    int m_hoverAxis = -1;
    bool m_snapEnabled = false;
    float m_translateSnap = 0.25f;
    float m_rotateSnap = 15.0f;
    float m_scaleSnap = 0.1f;
    int m_dragAxis = -1;
    String m_dragMode;
    UInt64 m_dragPrimary = 0;
    DynamicArray<TransformUpdate> m_dragEntities;
    dodoe::Vector3f m_dragStartPosition{0.0f};
    dodoe::Vector3f m_dragStartWorldPosition{0.0f};
    dodoe::Vector3f m_dragStartRotation{0.0f};
    dodoe::Vector3f m_dragStartScale{1.0f};
    dodoe::Vector3f m_dragPlanePoint{0.0f};
    dodoe::Vector3f m_dragPlaneNormal{0.0f, 0.0f, 1.0f};
    float m_dragStartAngle = 0.0f;
    float m_dragAccumulatedAngle = 0.0f;
    float m_dragStartAxisParam = 0.0f;
};

} // namespace cakery
