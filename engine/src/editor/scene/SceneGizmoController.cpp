// do@Redlive
#include "dopch.h"
#include "SceneGizmoController.h"
#include "SceneOverlayGeometry.h"
#include "SceneSelectionBounds.h"
#include "backend/EditorCamera.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/components/transform_component.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace dodoe;
namespace cakery {
using namespace scene_overlay;
namespace {
Entity FindEntity(const SceneGizmoContext& context, UInt64 uuid) {
    return context.scene && uuid != 0 ? context.scene->tryGetEntityByUUID(dodoe::UUID(uuid)) : Entity{};
}
constexpr Float kHandleLength = 1.0f;
constexpr Float kRingRadius = 0.9f;
constexpr UInt32 kRingSegments = 48;
constexpr Float kGizmoHitThresholdPx = 12.0f;

const Color kAxisRed{0.91f, 0.29f, 0.32f, 1.0f};
const Color kAxisGreen{0.43f, 0.78f, 0.31f, 1.0f};
const Color kAxisBlue{0.31f, 0.58f, 0.96f, 1.0f};
const Vector3f kAxes[3] = {
    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
};
const Color kColors[3] = {kAxisRed, kAxisGreen, kAxisBlue};
const Color kHoverColors[3] = {
    {1.0f, 0.87f, 0.25f, 1.0f},
    {1.0f, 0.87f, 0.25f, 1.0f},
    {1.0f, 0.87f, 0.25f, 1.0f},
};
const Color kSelectionColor{1.0f, 0.82f, 0.25f, 0.95f};

struct EditorGizmoPart {
    DynamicArray<Vector3f> positions;
    DynamicArray<UInt32> indices;
};

enum class GizmoPartId { Arrow, Shaft, Ring, Cube };

EditorGizmoPart MakeTranslationArrow() {
    EditorGizmoPart part;
    constexpr UInt32 segments = 24;
    // A slim shaft and a short solid cone; the tip is exactly one handle unit.
    for (UInt32 i = 0; i < segments; ++i) {
        const float angle = i * 6.2831853f / segments;
        const float x = std::cos(angle), z = std::sin(angle);
        part.positions.push_back({x * 0.009f, 0.08f, z * 0.009f});
        part.positions.push_back({x * 0.009f, 0.79f, z * 0.009f});
        part.positions.push_back({x * 0.052f, 0.79f, z * 0.052f});
    }
    const UInt32 tip = static_cast<UInt32>(part.positions.size());
    part.positions.push_back({0.0f, 1.0f, 0.0f});
    const UInt32 cap = static_cast<UInt32>(part.positions.size());
    part.positions.push_back({0.0f, 0.79f, 0.0f});
    for (UInt32 i = 0; i < segments; ++i) {
        const UInt32 a = i * 3, b = ((i + 1) % segments) * 3;
        for (const UInt32 index : {a, b, a + 1, b, b + 1, a + 1,
                                  a + 2, b + 2, tip, cap, b + 2, a + 2}) {
            part.indices.push_back(index);
        }
    }
    return part;
}

void LoadGizmoPart(EditorGizmoPart& part, const char* file) {
    const auto path = FileSystem::GetEngineResPath() / "models" / "editor" / file;
    std::ifstream stream(path);
    if (!stream.is_open()) {
        DO_ERROR("GizmoPart: failed to open '{}'", path.string());
        return;
    }
    String line;
    String token;
    while (std::getline(stream, line)) {
        if (line.rfind("v ", 0) == 0) {
            std::istringstream iss(line.substr(2).c_str());
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (iss >> x >> y >> z) {
                part.positions.push_back(Vector3f(x, y, z));
            }
        } else if (line.rfind("f ", 0) == 0) {
            std::istringstream iss(line.substr(2).c_str());
            DynamicArray<UInt32> face;
            face.reserve(4);
            while (iss >> token) {
                const Size_t slash = token.find('/');
                const String index_text =
                    (slash == String::npos) ? token : token.substr(0, slash);
                if (index_text.empty()) {
                    continue;
                }
                const int index = std::atoi(index_text.c_str());
                if (index > 0) {
                    face.push_back(static_cast<UInt32>(index - 1));
                }
            }
            for (Size_t k = 1; k + 1 < face.size(); ++k) {
                part.indices.push_back(face[0]);
                part.indices.push_back(face[k]);
                part.indices.push_back(face[k + 1]);
            }
        }
    }
    if (part.positions.empty() || part.indices.empty()) {
        DO_ERROR("GizmoPart: '{}' has no usable geometry", path.string());
    } else {
        DO_INFO("GizmoPart: loaded '{}' verts={} indices={}",
                path.string(), part.positions.size(), part.indices.size());
    }
}

const EditorGizmoPart& GetGizmoPart(GizmoPartId id) {
    static EditorGizmoPart parts[4];
    static Bool loaded[4] = {false, false, false, false};
    const Size_t index = static_cast<Size_t>(id);
    if (!loaded[index]) {
        loaded[index] = true;
        if (id == GizmoPartId::Arrow) {
            parts[index] = MakeTranslationArrow();
            return parts[index];
        }
        const char* file = nullptr;
        switch (id) {
            case GizmoPartId::Arrow: file = "gizmo_arrow.obj"; break;
            case GizmoPartId::Shaft: file = "gizmo_shaft.obj"; break;
            case GizmoPartId::Ring:  file = "gizmo_ring.obj";  break;
            case GizmoPartId::Cube:  file = "gizmo_cube.obj";  break;
        }
        LoadGizmoPart(parts[index], file);
    }
    return parts[index];
}

void AppendGizmoPart(GizmoChannelData& data, const EditorGizmoPart& part,
                     const Vector3f& axis, const Vector3f& origin,
                     float scale, const Color& color) {
    if (part.positions.empty() || part.indices.empty()) {
        return;
    }
    const Vector3f reference =
        (std::abs(axis.z) > 0.9f) ? Vector3f(1.0f, 0.0f, 0.0f) : Vector3f(0.0f, 0.0f, 1.0f);
    const Vector3f basis_u = Math::Normalize(Math::Cross(axis, reference));
    const Vector3f basis_v = Math::Cross(basis_u, axis);

    const UInt32 base_vertex = static_cast<UInt32>(data.vertices.size());
    const UInt32 base_index  = static_cast<UInt32>(data.indices.size());

    for (const Vector3f& p : part.positions) {
        const Vector3f world = origin +
            (basis_u * p.x + axis * p.y + basis_v * p.z) * scale;
        data.vertices.push_back(MakeVertex(world, color));
    }
    for (const UInt32 index : part.indices) {
        data.indices.push_back(index);
    }

    GizmoDrawCommand cmd;
    cmd.vertex_offset = base_vertex;
    cmd.vertex_count  = static_cast<UInt32>(data.vertices.size()) - base_vertex;
    cmd.index_offset  = base_index;
    cmd.index_count   = static_cast<UInt32>(data.indices.size()) - base_index;
    cmd.topology      = GfxPrimitiveType::TriangleList;
    cmd.transform     = Matrix4f(1.0f);
    data.commands.push_back(cmd);
}

void GenerateTranslateGizmo(GizmoChannelData& data, const Vector3f& position,
                            float scale, int hoverAxis) {
    const EditorGizmoPart& arrow = GetGizmoPart(GizmoPartId::Arrow);
    for (Int32 i = 0; i < 3; ++i) {
        const Color& color = (i == hoverAxis) ? kHoverColors[i] : kColors[i];
        AppendGizmoPart(data, arrow, kAxes[i], position, scale, color);
    }
    data.has_data = true;
}

void AddPlaneHandle(GizmoChannelData& data, const Vector3f& position, float scale,
                    int normalAxis, bool hovered, bool planar = false) {
    const Vector3f u = kAxes[(normalAxis + 1) % 3] * scale;
    const Vector3f v = kAxes[(normalAxis + 2) % 3] * scale;
    const Vector3f corners[] = {position + (u + v) * 0.18f,
        position + u * 0.38f + v * 0.18f, position + (u + v) * 0.38f,
        position + u * 0.18f + v * 0.38f};
    Color color = hovered || planar ? kSelectionColor : kColors[normalAxis];
    color.a = hovered ? 0.55f : 0.22f;
    GizmoDrawCommand command{};
    command.vertex_offset = static_cast<UInt32>(data.vertices.size());
    command.vertex_count = 6;
    command.topology = GfxPrimitiveType::TriangleList;
    for (const int index : {0, 1, 2, 0, 2, 3}) data.vertices.push_back(MakeVertex(corners[index], color));
    data.commands.push_back(command);
    color.a = hovered ? 1.0f : 0.7f;
    for (int i = 0; i < 4; ++i) AddLine(data, corners[i], corners[(i + 1) % 4], color);
}

[[nodiscard]] float SnapToStep(float value, float step) {
    if (step <= 1e-6f) {
        return value;
    }
    return std::round(value / step) * step;
}

float PointDistanceSq(float px, float py, const Vector2f& a) {
    const float dx = px - a.x;
    const float dy = py - a.y;
    return dx * dx + dy * dy;
}

float PointSegmentDistanceSq(float px, float py, const Vector2f& a, const Vector2f& b) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float lenSq = abx * abx + aby * aby;
    float t = 0.0f;
    if (lenSq > 1e-8f) {
        t = ((px - a.x) * abx + (py - a.y) * aby) / lenSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    const float cx = a.x + abx * t;
    const float cy = a.y + aby * t;
    const float dx = px - cx;
    const float dy = py - cy;
    return dx * dx + dy * dy;
}

bool RayPlaneIntersect(const Vector3f& origin, const Vector3f& dir,
                       const Vector3f& planePoint, const Vector3f& planeNormal,
                       Vector3f& outPoint) {
    const float denom = Math::Dot(planeNormal, dir);
    if (std::abs(denom) < 1e-6f) {
        return false;
    }
    const float t = Math::Dot(planeNormal, planePoint - origin) / denom;
    if (t < 0.0f) {
        return false;
    }
    outPoint = origin + dir * t;
    return true;
}

void GenerateRotateGizmo(GizmoChannelData& data, const Vector3f& position,
                         float scale, int hoverAxis) {
    const EditorGizmoPart& ring = GetGizmoPart(GizmoPartId::Ring);
    for (Int32 axis = 0; axis < 3; ++axis) {
        const Color& color = (axis == hoverAxis) ? kHoverColors[axis] : kColors[axis];
        AppendGizmoPart(data, ring, kAxes[axis], position, scale, color);
    }
    data.has_data = true;
}

void GenerateScaleGizmo(GizmoChannelData& data, const Vector3f& position,
                        float scale, int hoverAxis) {
    const EditorGizmoPart& shaft = GetGizmoPart(GizmoPartId::Shaft);
    const EditorGizmoPart& cube = GetGizmoPart(GizmoPartId::Cube);
    for (Int32 i = 0; i < 3; ++i) {
        const Color& color = (i == hoverAxis) ? kHoverColors[i] : kColors[i];
        const Vector3f axis = kAxes[i];
        const Vector3f tip = position + axis * (kHandleLength * scale);
        AppendGizmoPart(data, shaft, axis, position, scale, color);
        AppendGizmoPart(data, cube, axis, tip, scale, color);
    }
    data.has_data = true;
}


} // namespace

void SceneGizmoController::draw(const SceneGizmoContext& context, GizmoChannelData& data)
{
    if (!context.camera || !context.scene) return;
    drawSelectionHighlight(context, data);
    Entity entity = FindEntity(context, context.selectedUuid);
    if (!entity || !entity.hasComponent<TransformComponent>() || m_gizmoMode == "none") return;
    const Vector3f position = Vector3f(sceneEntityTransform(entity)[3]);
    m_gizmoScale = std::max(computeGizmoScale(context, position), 1e-3f);
    const int active = dragging() ? m_dragAxis : m_hoverAxis;
    const bool ortho = context.camera->mode() == EditorCamera::Mode::Ortho2D;
    if (ortho) {
        if (m_gizmoMode == "rotate") {
            AppendGizmoPart(data, GetGizmoPart(GizmoPartId::Ring), kAxes[2], position,
                m_gizmoScale, active == 2 ? kSelectionColor : kColors[2]);
        } else {
            for (int axis = 0; axis < 2; ++axis) {
                const Color& color = axis == active ? kSelectionColor : kColors[axis];
                if (m_gizmoMode == "translate") {
                    AppendGizmoPart(data, GetGizmoPart(GizmoPartId::Arrow), kAxes[axis], position, m_gizmoScale, color);
                } else if (m_gizmoMode == "scale") {
                    AppendGizmoPart(data, GetGizmoPart(GizmoPartId::Shaft), kAxes[axis], position, m_gizmoScale, color);
                    AppendGizmoPart(data, GetGizmoPart(GizmoPartId::Cube), kAxes[axis],
                        position + kAxes[axis] * m_gizmoScale, m_gizmoScale, color);
                }
            }
            if (m_gizmoMode == "translate") AddPlaneHandle(data, position, m_gizmoScale, 2, active == 5, true);
        }
    } else if (m_gizmoMode == "translate") {
        GenerateTranslateGizmo(data, position, m_gizmoScale, active);
        for (int axis = 0; axis < 3; ++axis) AddPlaneHandle(data, position, m_gizmoScale, axis, active == axis + 3);
    } else if (m_gizmoMode == "rotate") {
        GenerateRotateGizmo(data, position, m_gizmoScale, active);
    } else if (m_gizmoMode == "scale") {
        GenerateScaleGizmo(data, position, m_gizmoScale, active);
    }
    data.has_data = !data.commands.empty();
}

void SceneGizmoController::drawSelectionHighlight(const SceneGizmoContext& context, GizmoChannelData& data)
{
    const auto drawEntity = [&](UInt64 uuid) {
        Entity entity = FindEntity(context, uuid);
        if (!entity || !entity.hasComponent<TransformComponent>()) return;
        const auto bounds = sceneSelectionBounds(entity);
        if (!bounds.hasGeometry) {
            // An empty GameObject has a pivot, not a fabricated unit-sized box.
            const Vector3f origin(bounds.localToWorld[3]);
            const float size = context.camera->pixelsToWorld(origin, 4.0f);
            AddLine(data, origin - Vector3f(size, 0, 0), origin + Vector3f(size, 0, 0), kSelectionColor);
            AddLine(data, origin - Vector3f(0, size, 0), origin + Vector3f(0, size, 0), kSelectionColor);
            return;
        }
        const auto& lo = bounds.minimum;
        const auto& hi = bounds.maximum;
        Vector3f corners[] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                             {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
        for (auto& corner : corners) corner = Vector3f(bounds.localToWorld * Vector4f(corner, 1.0f));
        for (int i = 0; i < 4; ++i) {
            AddLine(data, corners[i], corners[(i + 1) % 4], kSelectionColor);
            if (!bounds.planar) {
                AddLine(data, corners[i + 4], corners[(i + 1) % 4 + 4], kSelectionColor);
                AddLine(data, corners[i], corners[i + 4], kSelectionColor);
            }
        }
    };
    for (UInt64 uuid : context.selection) if (uuid != context.selectedUuid) drawEntity(uuid);
    drawEntity(context.selectedUuid);
    data.has_data = !data.commands.empty();
}

bool SceneGizmoController::press(const SceneGizmoContext& context, float x, float y)
{
    if (m_gizmoMode == "none") return false;
    const int axis = hitTest(context, x, y);
    if (axis < 0) return false;
    beginDrag(context, axis, x, y);
    return dragging();
}

void SceneGizmoController::hover(const SceneGizmoContext& context, float x, float y)
{
    m_hoverAxis = m_gizmoMode == "none" ? -1 : hitTest(context, x, y);
}

void SceneGizmoController::setSnapSteps(float translate, float rotate, float scale)
{
    m_translateSnap = std::max(0.0f, translate);
    m_rotateSnap = std::max(0.0f, rotate);
    m_scaleSnap = std::max(0.0f, scale);
}

void SceneGizmoController::cancelDrag(const SceneGizmoContext& context)
{
    if (!dragging()) return;
    DynamicArray<TransformUpdate> restore;
    restore.push_back({m_dragPrimary, m_dragStartPosition, m_dragStartRotation, m_dragStartScale});
    restore.insert(restore.end(), m_dragEntities.begin(), m_dragEntities.end());
    for (const auto& original : restore) {
        Entity entity = FindEntity(context, original.uuid);
        if (!entity || !entity.hasComponent<TransformComponent>()) continue;
        auto& t = entity.getComponent<TransformComponent>();
        t.setPosition(original.position);
        t.setRotation(original.rotation);
        t.setScale(original.scale);
    }
    emitTransformChanges(context, restore);
    endDrag(context);
}

float SceneGizmoController::computeGizmoScale(const SceneGizmoContext& context, const Vector3f& position) const
{
    if (!context.camera) {
        return 1.0f;
    }
    return context.camera->pixelsToWorld(position, 90.0f);
}

int SceneGizmoController::hitTest(const SceneGizmoContext& context, float screenX, float screenY)
{
    Entity entity = FindEntity(context, context.selectedUuid);
    if (!entity || !entity.hasComponent<TransformComponent>() || !context.camera) {
        return -1;
    }
    const Vector3f center = Vector3f(sceneEntityTransform(entity)[3]);
    m_gizmoScale = std::max(computeGizmoScale(context, center), 1e-3f);
    const bool ortho = context.camera->mode() == EditorCamera::Mode::Ortho2D;
    const float thresholdSq = kGizmoHitThresholdPx * kGizmoHitThresholdPx;
    const float handleLength = kHandleLength * m_gizmoScale;
    const float ringRadius = kRingRadius * m_gizmoScale;
    int bestAxis = -1;
    float bestDist = thresholdSq;

    if (m_gizmoMode == "rotate") {
        for (Int32 axis = 0; axis < 3; ++axis) {
            if (ortho && axis != 2) continue;
            float minDist = 1e30f;
            for (UInt32 i = 0; i < kRingSegments; ++i) {
                const Float a = static_cast<Float>(i) * 2.0f * 3.14159265f / static_cast<Float>(kRingSegments);
                const Float c = std::cos(a), s = std::sin(a);
                Vector3f point;
                if (axis == 0) {
                    point = center + Vector3f(0.0f, c, s) * ringRadius;
                } else if (axis == 1) {
                    point = center + Vector3f(c, 0.0f, s) * ringRadius;
                } else {
                    point = center + Vector3f(c, s, 0.0f) * ringRadius;
                }
                const Vector2f screenPt = context.camera->projectToScreen(point);
                minDist = std::min(minDist, PointDistanceSq(screenX, screenY, screenPt));
            }
            if (minDist < bestDist) {
                bestDist = minDist;
                bestAxis = axis;
            }
        }
        return bestAxis;
    }

    for (Int32 axis = 0; axis < 3; ++axis) {
        if (ortho && axis == 2) continue;
        const Vector2f start = context.camera->projectToScreen(center + kAxes[axis] * handleLength * 0.08f);
        const Vector2f end = context.camera->projectToScreen(center + kAxes[axis] * handleLength);
        const Vector2f projected = end - start;
        if (Math::Dot(projected, projected) < 16.0f) continue;
        const float dist = PointSegmentDistanceSq(screenX, screenY, start, end);
        if (dist < bestDist) {
            bestDist = dist;
            bestAxis = axis;
        }
    }
    if (m_gizmoMode == "translate" && bestAxis < 0) {
        Vector3f origin, direction;
        context.camera->screenToRay(screenX, screenY, origin, direction);
        for (int axis = 0; axis < 3; ++axis) {
            if (ortho && axis != 2) continue;
            Vector3f point;
            if (!RayPlaneIntersect(origin, direction, center, kAxes[axis], point)) continue;
            const Vector3f local = (point - center) / m_gizmoScale;
            const float u = local[(axis + 1) % 3], v = local[(axis + 2) % 3];
            if (u >= 0.18f && u <= 0.38f && v >= 0.18f && v <= 0.38f) return axis + 3;
        }
    }
    return bestAxis;
}

void SceneGizmoController::beginDrag(const SceneGizmoContext& context, int axis, float screenX, float screenY)
{
    Entity entity = FindEntity(context, context.selectedUuid);
    if (!entity || !entity.hasComponent<TransformComponent>()) {
        return;
    }
    auto& transform = entity.getComponent<TransformComponent>();
    m_dragMode = m_gizmoMode;
    m_dragAxis = axis;
    m_dragStartPosition = transform.getPosition();
    m_dragStartWorldPosition = Vector3f(sceneEntityTransform(entity)[3]);
    m_dragStartRotation = transform.getRotation();
    m_dragStartScale = transform.getScale();
    m_dragEntities.clear();

    m_dragPrimary = context.selectedUuid;
    for (const UInt64 uuid : context.selection) {
        if (uuid == context.selectedUuid) continue;
        Entity other = FindEntity(context, uuid);
        if (!other || !other.hasComponent<TransformComponent>()) continue;
        const auto& t = other.getComponent<TransformComponent>();
        m_dragEntities.push_back({uuid, t.getPosition(), t.getRotation(), t.getScale()});
    }

    if (!context.camera) {
        m_dragAxis = -1;
        m_dragMode.clear();
        m_dragEntities.clear();
        return;
    }

    if (m_dragMode == "translate" || m_dragMode == "scale") {
        Vector3f origin, dir;
        context.camera->screenToRay(screenX, screenY, origin, dir);
        m_dragPlaneNormal = axis >= 3 ? kAxes[axis - 3] : context.camera->forwardDirection();
        if (!RayPlaneIntersect(origin, dir, m_dragStartWorldPosition, m_dragPlaneNormal, m_dragPlanePoint)) {
            m_dragAxis = -1;
            m_dragMode.clear();
            m_dragEntities.clear();
            return;
        }
    }

    if (m_dragMode == "scale") {
        const Vector2f start = context.camera->projectToScreen(m_dragStartWorldPosition);
        const Vector2f end = context.camera->projectToScreen(
            m_dragStartWorldPosition + kAxes[m_dragAxis] * kHandleLength * m_gizmoScale);
        const Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        // A view-facing axis has no screen-space direction in a 2D/editor
        // view. It cannot provide a meaningful drag distance.
        if (axisLengthSq < 1e-6f) {
            m_dragAxis = -1;
            m_dragMode.clear();
            m_dragEntities.clear();
            return;
        }
        const Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        m_dragStartAxisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
    }

    if (m_dragMode == "rotate") {
        const Vector2f centerScreen = context.camera->projectToScreen(m_dragStartWorldPosition);
        m_dragStartAngle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
        m_dragAccumulatedAngle = 0.0f;
    }

    context.publish(BackendEventMessage{"transform_drag_begin", ""});
}

void SceneGizmoController::updateDrag(const SceneGizmoContext& context, float screenX, float screenY)
{
    if (!dragging() || context.selectedUuid != m_dragPrimary) {
        cancelDrag(context);
        return;
    }
    Entity entity = FindEntity(context, context.selectedUuid);
    if (!entity || !entity.hasComponent<TransformComponent>() || !context.camera) {
        return;
    }
    auto& transform = entity.getComponent<TransformComponent>();
    const bool snapping = m_snapEnabled || context.snapModifier;

    if (m_dragMode == "translate") {
        Vector3f origin, dir;
        context.camera->screenToRay(screenX, screenY, origin, dir);
        Vector3f planePoint;
        if (!RayPlaneIntersect(origin, dir, m_dragStartWorldPosition, m_dragPlaneNormal, planePoint)) {
            return;
        }
        Vector3f movement = planePoint - m_dragPlanePoint;
        if (m_dragAxis >= 3) {
            movement[m_dragAxis - 3] = 0.0f;
            if (snapping) {
                for (int i = 0; i < 3; ++i) movement[i] = SnapToStep(movement[i], m_translateSnap);
            }
        } else {
            const float facing = Math::Dot(m_dragPlaneNormal, kAxes[m_dragAxis]);
            float amount = Math::Dot(movement, kAxes[m_dragAxis]) / std::max(1.0f - facing * facing, 0.001f);
            if (snapping) amount = SnapToStep(amount, m_translateSnap);
            movement = kAxes[m_dragAxis] * amount;
        }
        const auto localMovement = [&movement](Entity target) {
            const Matrix4f parent = sceneEntityParentTransform(target);
            if (std::abs(glm::determinant(parent)) < 1e-8f) return Vector3f(0.0f);
            return Vector3f(glm::inverse(parent) * Vector4f(movement, 0.0f));
        };
        const Vector3f newPosition = m_dragStartPosition + localMovement(entity);
        transform.setPosition(newPosition);

        DynamicArray<TransformUpdate> updates;
        updates.push_back({context.selectedUuid, newPosition, transform.getRotation(), transform.getScale()});
        for (const TransformUpdate& start : m_dragEntities) {
            Entity other = FindEntity(context, start.uuid);
            if (!other.valid() || !other.hasComponent<TransformComponent>()) {
                continue;
            }
            const Vector3f otherPosition = start.position + localMovement(other);
            other.getComponent<TransformComponent>().setPosition(otherPosition);
            updates.push_back({start.uuid, otherPosition, start.rotation, start.scale});
        }
        emitTransformChanges(context, updates);
    } else if (m_dragMode == "rotate") {
        const Vector2f centerScreen = context.camera->projectToScreen(m_dragStartWorldPosition);
        const float angle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
        m_dragAccumulatedAngle += std::remainder(angle - m_dragStartAngle, 6.2831853f);
        m_dragStartAngle = angle;
        const float direction = Math::Dot(kAxes[m_dragAxis], context.camera->forwardDirection()) < 0.0f ? -1.0f : 1.0f;
        float deltaDegrees = direction * m_dragAccumulatedAngle * 180.0f / 3.14159265f;
        if (snapping) {
            deltaDegrees = SnapToStep(deltaDegrees, m_rotateSnap);
        }
        Vector3f newRotation = m_dragStartRotation;
        newRotation[m_dragAxis] += deltaDegrees;
        transform.setRotation(newRotation);

        DynamicArray<TransformUpdate> updates;
        updates.push_back({context.selectedUuid, m_dragStartPosition, newRotation, m_dragStartScale});
        for (const TransformUpdate& start : m_dragEntities) {
            Entity other = FindEntity(context, start.uuid);
            if (!other.valid() || !other.hasComponent<TransformComponent>()) {
                continue;
            }
            Vector3f otherRotation = start.rotation;
            otherRotation[m_dragAxis] += deltaDegrees;
            other.getComponent<TransformComponent>().setRotation(otherRotation);
            updates.push_back({start.uuid, start.position, otherRotation, start.scale});
        }
        emitTransformChanges(context, updates);
    } else if (m_dragMode == "scale") {
        const Vector2f start = context.camera->projectToScreen(m_dragStartWorldPosition);
        const Vector2f end = context.camera->projectToScreen(
            m_dragStartWorldPosition + kAxes[m_dragAxis] * kHandleLength * m_gizmoScale);
        const Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        if (axisLengthSq < 1e-6f) {
            return;
        }
        const Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        const float axisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
        const float movement = axisParam - m_dragStartAxisParam;
        const float scaleFactor = std::max(0.001f, 1.0f + movement);
        Vector3f newScale = m_dragStartScale;
        float primaryScale = m_dragStartScale[m_dragAxis] * scaleFactor;
        if (snapping) {
            primaryScale = SnapToStep(primaryScale, m_scaleSnap);
        }
        newScale[m_dragAxis] = std::copysign(std::max(0.01f, std::abs(primaryScale)), m_dragStartScale[m_dragAxis]);
        transform.setScale(newScale);

        DynamicArray<TransformUpdate> updates;
        updates.push_back({context.selectedUuid, m_dragStartPosition, m_dragStartRotation, newScale});
        for (const TransformUpdate& dragStart : m_dragEntities) {
            Entity other = FindEntity(context, dragStart.uuid);
            if (!other.valid() || !other.hasComponent<TransformComponent>()) {
                continue;
            }
            Vector3f otherScale = dragStart.scale;
            float otherAxis = dragStart.scale[m_dragAxis] * scaleFactor;
            if (snapping) {
                otherAxis = SnapToStep(otherAxis, m_scaleSnap);
            }
            otherScale[m_dragAxis] = std::copysign(std::max(0.01f, std::abs(otherAxis)), dragStart.scale[m_dragAxis]);
            other.getComponent<TransformComponent>().setScale(otherScale);
            updates.push_back({dragStart.uuid, dragStart.position, dragStart.rotation, otherScale});
        }
        emitTransformChanges(context, updates);
    }

    if (context.camera) {
        context.camera->updateLastMouse(screenX, screenY);
    }
}

void SceneGizmoController::endDrag(const SceneGizmoContext& context)
{
    if (!dragging()) return;
    resetInteraction();
    context.publish(BackendEventMessage{"transform_drag_end", ""});
}

void SceneGizmoController::resetInteraction()
{
    m_dragEntities.clear();
    m_dragPrimary = 0;
    m_dragAxis = -1;
    m_dragMode.clear();
    m_hoverAxis = -1;
}

void SceneGizmoController::emitTransformChange(const SceneGizmoContext& context, const Vector3f& position,
                                               const Vector3f& rotation,
                                               const Vector3f& scale)
{
    if (!context.editing) {
        return;
    }
    Json payload = {
        {"uuid", context.selectedUuid},
        {"value", {
            {"position", {position.x, position.y, position.z}},
            {"rotation", {rotation.x, rotation.y, rotation.z}},
            {"scale", {scale.x, scale.y, scale.z}},
        }},
    };
    context.publish(BackendEventMessage{"transform_changed", ToString(payload)});
}

void SceneGizmoController::emitTransformChanges(const SceneGizmoContext& context, const DynamicArray<TransformUpdate>& updates)
{
    if (!context.editing) {
        return;
    }
    if (updates.size() == 1 && updates[0].uuid == context.selectedUuid) {
        emitTransformChange(context, updates[0].position, updates[0].rotation, updates[0].scale);
        return;
    }
    Json entities = Json::array();
    for (const TransformUpdate& update : updates) {
        entities.push_back({
            {"uuid", update.uuid},
            {"value", {
                {"position", {update.position.x, update.position.y, update.position.z}},
                {"rotation", {update.rotation.x, update.rotation.y, update.rotation.z}},
                {"scale", {update.scale.x, update.scale.y, update.scale.z}},
            }},
        });
    }
    Json payload = {{"entities", std::move(entities)}};
    context.publish(BackendEventMessage{"transform_changed", ToString(payload)});
}

} // namespace cakery
