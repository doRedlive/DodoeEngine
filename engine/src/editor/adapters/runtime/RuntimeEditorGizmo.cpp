// do@Redlive

#include "RuntimeEditorBackend.h"

#include "EditorCamera.h"
#include "adapters/runtime/services/UuidResolve.h"

#include "runtime/core/channel/gizmo_channel.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/instrumentor.h"
#include "runtime/service/editor/picking_backend.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/world.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace dodoe;

namespace cakery {

namespace {

constexpr Float kHandleLength = 1.0f;
constexpr Float kArrowHeadLength = 0.15f;
constexpr Float kArrowHeadRadius = 0.04f;
constexpr UInt32 kArrowHeadSegments = 4;
constexpr Float kRingRadius = 0.9f;
constexpr UInt32 kRingSegments = 48;
constexpr Float kCubeHalfSize = 0.06f;
constexpr Float kGizmoHitThresholdPx = 12.0f;

const dodoe::Color kAxisRed{1.0f, 0.2f, 0.2f, 1.0f};
const dodoe::Color kAxisGreen{0.2f, 1.0f, 0.2f, 1.0f};
const dodoe::Color kAxisBlue{0.2f, 0.4f, 1.0f, 1.0f};
const dodoe::Vector3f kAxes[3] = {
    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
};
const dodoe::Color kColors[3] = {kAxisRed, kAxisGreen, kAxisBlue};

[[nodiscard]] dodoe::GizmoVertex MakeVertex(const dodoe::Vector3f& pos, const dodoe::Color& color) {
    return {pos.x, pos.y, pos.z, color.r, color.g, color.b, color.a};
}

void AddLine(dodoe::GizmoChannelData& data, const dodoe::Vector3f& start, const dodoe::Vector3f& end,
             const dodoe::Color& color) {
    const UInt32 base = static_cast<UInt32>(data.vertices.size());
    data.vertices.push_back(MakeVertex(start, color));
    data.vertices.push_back(MakeVertex(end, color));

    dodoe::GizmoDrawCommand cmd;
    cmd.vertex_offset = base;
    cmd.vertex_count  = 2;
    cmd.index_offset  = 0;
    cmd.index_count   = 0;
    cmd.topology      = dodoe::GfxPrimitiveType::LineList;
    cmd.transform     = dodoe::Matrix4f(1.0f);
    data.commands.push_back(cmd);
}

void AddArrowHead(dodoe::GizmoChannelData& data, const dodoe::Vector3f& tip, const dodoe::Vector3f& axis,
                  const dodoe::Color& color) {
    const UInt32 base_vertex = static_cast<UInt32>(data.vertices.size());
    const UInt32 base_index  = static_cast<UInt32>(data.indices.size());

    dodoe::Vector3f perp1, perp2;
    if (std::abs(axis.x) < 0.9f) {
        perp1 = dodoe::Math::Normalize(dodoe::Math::Cross(axis, dodoe::Vector3f(1.0f, 0.0f, 0.0f)));
    } else {
        perp1 = dodoe::Math::Normalize(dodoe::Math::Cross(axis, dodoe::Vector3f(0.0f, 1.0f, 0.0f)));
    }
    perp2 = dodoe::Math::Normalize(dodoe::Math::Cross(axis, perp1));

    const dodoe::Vector3f base_center = tip - axis * kArrowHeadLength;

    data.vertices.push_back(MakeVertex(tip, color));
    for (UInt32 i = 0; i < kArrowHeadSegments; ++i) {
        const Float angle = static_cast<Float>(i) * 2.0f * 3.14159265f / static_cast<Float>(kArrowHeadSegments);
        const dodoe::Vector3f offset = (perp1 * std::cos(angle) + perp2 * std::sin(angle)) * kArrowHeadRadius;
        data.vertices.push_back(MakeVertex(base_center + offset, color));
    }
    data.vertices.push_back(MakeVertex(base_center, color));

    const UInt32 tip_idx   = base_vertex;
    const UInt32 base_mid  = base_vertex + 1 + kArrowHeadSegments;

    for (UInt32 i = 0; i < kArrowHeadSegments; ++i) {
        const UInt32 curr = base_vertex + 1 + i;
        const UInt32 next = base_vertex + 1 + (i + 1) % kArrowHeadSegments;
        data.indices.push_back(tip_idx);
        data.indices.push_back(next);
        data.indices.push_back(curr);
        data.indices.push_back(base_mid);
        data.indices.push_back(curr);
        data.indices.push_back(next);
    }

    dodoe::GizmoDrawCommand cmd;
    cmd.vertex_offset = base_vertex;
    cmd.vertex_count  = static_cast<UInt32>(data.vertices.size()) - base_vertex;
    cmd.index_offset  = base_index;
    cmd.index_count   = static_cast<UInt32>(data.indices.size()) - base_index;
    cmd.topology      = dodoe::GfxPrimitiveType::TriangleList;
    cmd.transform     = dodoe::Matrix4f(1.0f);
    data.commands.push_back(cmd);
}

void AddArrow(dodoe::GizmoChannelData& data, const dodoe::Vector3f& origin, const dodoe::Vector3f& axis,
              const dodoe::Color& color) {
    const dodoe::Vector3f tip = origin + axis * kHandleLength;
    const dodoe::Vector3f shaft_end = tip - axis * kArrowHeadLength;
    AddLine(data, origin, shaft_end, color);
    AddArrowHead(data, tip, axis, color);
}

void GenerateTranslateGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position) {
    const dodoe::Matrix4f translation = dodoe::Math::Translate(dodoe::Matrix4f(1.0f), position);

    for (Int32 i = 0; i < 3; ++i) {
        dodoe::GizmoChannelData axis_data;
        AddArrow(axis_data, dodoe::Vector3f(0.0f), kAxes[i], kColors[i]);

        const UInt32 vertex_base = static_cast<UInt32>(data.vertices.size());
        const UInt32 index_base  = static_cast<UInt32>(data.indices.size());

        for (auto& v : axis_data.vertices) {
            dodoe::Vector4f world_pos = translation * dodoe::Vector4f(v.px, v.py, v.pz, 1.0f);
            v.px = world_pos.x; v.py = world_pos.y; v.pz = world_pos.z;
        }

        for (auto& cmd : axis_data.commands) {
            cmd.vertex_offset += vertex_base;
            cmd.index_offset  += index_base;
        }

        data.vertices.insert(data.vertices.end(), axis_data.vertices.begin(), axis_data.vertices.end());
        data.indices.insert(data.indices.end(), axis_data.indices.begin(), axis_data.indices.end());
        data.commands.insert(data.commands.end(), axis_data.commands.begin(), axis_data.commands.end());
    }

    data.has_data = true;
}

float PointDistanceSq(float px, float py, const dodoe::Vector2f& a) {
    const float dx = px - a.x;
    const float dy = py - a.y;
    return dx * dx + dy * dy;
}

float PointSegmentDistanceSq(float px, float py, const dodoe::Vector2f& a, const dodoe::Vector2f& b) {
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

bool RayPlaneIntersect(const dodoe::Vector3f& origin, const dodoe::Vector3f& dir,
                       const dodoe::Vector3f& planePoint, const dodoe::Vector3f& planeNormal,
                       dodoe::Vector3f& outPoint) {
    const float denom = dodoe::Math::Dot(planeNormal, dir);
    if (std::abs(denom) < 1e-6f) {
        return false;
    }
    const float t = dodoe::Math::Dot(planeNormal, planePoint - origin) / denom;
    if (t < 0.0f) {
        return false;
    }
    outPoint = origin + dir * t;
    return true;
}

void AddCube(dodoe::GizmoChannelData& data, const dodoe::Vector3f& center, float halfSize,
             const dodoe::Color& color) {
    const UInt32 base_vertex = static_cast<UInt32>(data.vertices.size());
    const UInt32 base_index = static_cast<UInt32>(data.indices.size());

    const float h = halfSize;
    const dodoe::Vector3f corners[8] = {
        center + dodoe::Vector3f(-h, -h, -h),
        center + dodoe::Vector3f( h, -h, -h),
        center + dodoe::Vector3f( h,  h, -h),
        center + dodoe::Vector3f(-h,  h, -h),
        center + dodoe::Vector3f(-h, -h,  h),
        center + dodoe::Vector3f( h, -h,  h),
        center + dodoe::Vector3f( h,  h,  h),
        center + dodoe::Vector3f(-h,  h,  h),
    };
    const UInt32 faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0},
    };

    UInt32 face_base = base_vertex;
    for (const auto& face : faces) {
        for (UInt32 i = 0; i < 4; ++i) {
            data.vertices.push_back(MakeVertex(corners[face[i]], color));
        }
        data.indices.push_back(face_base + 0);
        data.indices.push_back(face_base + 1);
        data.indices.push_back(face_base + 2);
        data.indices.push_back(face_base + 0);
        data.indices.push_back(face_base + 2);
        data.indices.push_back(face_base + 3);
        face_base += 4;
    }

    dodoe::GizmoDrawCommand cmd;
    cmd.vertex_offset = base_vertex;
    cmd.vertex_count  = static_cast<UInt32>(data.vertices.size()) - base_vertex;
    cmd.index_offset  = base_index;
    cmd.index_count   = static_cast<UInt32>(data.indices.size()) - base_index;
    cmd.topology      = dodoe::GfxPrimitiveType::TriangleList;
    cmd.transform     = dodoe::Matrix4f(1.0f);
    data.commands.push_back(cmd);
}

void GenerateRotateGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position) {
    for (Int32 axis = 0; axis < 3; ++axis) {
        for (UInt32 i = 0; i < kRingSegments; ++i) {
            const Float a0 = static_cast<Float>(i) * 2.0f * 3.14159265f / static_cast<Float>(kRingSegments);
            const Float a1 = static_cast<Float>(i + 1) * 2.0f * 3.14159265f / static_cast<Float>(kRingSegments);
            const Float c0 = std::cos(a0), s0 = std::sin(a0);
            const Float c1 = std::cos(a1), s1 = std::sin(a1);
            dodoe::Vector3f p0, p1;
            if (axis == 0) {
                p0 = position + dodoe::Vector3f(0.0f, c0, s0) * kRingRadius;
                p1 = position + dodoe::Vector3f(0.0f, c1, s1) * kRingRadius;
            } else if (axis == 1) {
                p0 = position + dodoe::Vector3f(c0, 0.0f, s0) * kRingRadius;
                p1 = position + dodoe::Vector3f(c1, 0.0f, s1) * kRingRadius;
            } else {
                p0 = position + dodoe::Vector3f(c0, s0, 0.0f) * kRingRadius;
                p1 = position + dodoe::Vector3f(c1, s1, 0.0f) * kRingRadius;
            }
            AddLine(data, p0, p1, kColors[axis]);
        }
    }
    data.has_data = true;
}

void GenerateScaleGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position) {
    for (Int32 i = 0; i < 3; ++i) {
        const dodoe::Vector3f tip = position + kAxes[i] * kHandleLength;
        AddLine(data, position, tip, kColors[i]);
        AddCube(data, tip, kCubeHalfSize, kColors[i]);
    }
    data.has_data = true;
}

} // anonymous namespace

dodoe::Entity RuntimeEditorBackend::activeTilemapEntity() const
{
    if (!m_tilePaint || !m_tilePaint->hasTarget()) return {};
    dodoe::World* world = runtimeWorld();
    dodoe::Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) return {};
    return ResolveEntity(scene, m_tilePaint->activeTilemap());
}

void RuntimeEditorBackend::updateTileOverlay()
{
    if (!m_tilePaint || !m_tilePaint->hasTarget() || m_tilePaint->tool() == TileTool::Select) return;
    dodoe::Entity tm = activeTilemapEntity();
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) return;

    const auto& comp = tm.getComponent<TilemapComponent>();
    const float mapW = static_cast<float>(comp.map_width * comp.tile_width);
    const float mapH = static_cast<float>(comp.map_height * comp.tile_height);

    const dodoe::Color borderColor{1.0f, 1.0f, 1.0f, 0.9f};
    const dodoe::Color gridColor{1.0f, 1.0f, 1.0f, 0.10f};
    const dodoe::Color ghostColor{0.25f, 1.0f, 0.35f, 1.0f};

    auto& data = GetGizmoChannel().get<dodoe::GizmoChannelData>();

    AddLine(data, dodoe::Vector3f(0.0f, 0.0f, 0.0f), dodoe::Vector3f(mapW, 0.0f, 0.0f), borderColor);
    AddLine(data, dodoe::Vector3f(mapW, 0.0f, 0.0f), dodoe::Vector3f(mapW, mapH, 0.0f), borderColor);
    AddLine(data, dodoe::Vector3f(mapW, mapH, 0.0f), dodoe::Vector3f(0.0f, mapH, 0.0f), borderColor);
    AddLine(data, dodoe::Vector3f(0.0f, mapH, 0.0f), dodoe::Vector3f(0.0f, 0.0f, 0.0f), borderColor);

    const int gw = static_cast<int>(comp.map_width);
    const int gh = static_cast<int>(comp.map_height);
    if (gw <= 256 && gh <= 256) {
        for (int x = 1; x < gw; ++x) {
            const float px = static_cast<float>(x * comp.tile_width);
            AddLine(data, dodoe::Vector3f(px, 0.0f, 0.0f), dodoe::Vector3f(px, mapH, 0.0f), gridColor);
        }
        for (int y = 1; y < gh; ++y) {
            const float py = static_cast<float>(y * comp.tile_height);
            AddLine(data, dodoe::Vector3f(0.0f, py, 0.0f), dodoe::Vector3f(mapW, py, 0.0f), gridColor);
        }
    }

    if (m_tilePaint->hasHover()) {
        const TileBrush& brush = m_tilePaint->brush();
        const float x0 = static_cast<float>(m_tilePaint->hoverX() * comp.tile_width);
        const float y0 = static_cast<float>(m_tilePaint->hoverY() * comp.tile_height);
        const float x1 = x0 + static_cast<float>(brush.w * comp.tile_width);
        const float y1 = y0 + static_cast<float>(brush.h * comp.tile_height);
        AddLine(data, dodoe::Vector3f(x0, y0, 0.0f), dodoe::Vector3f(x1, y0, 0.0f), ghostColor);
        AddLine(data, dodoe::Vector3f(x1, y0, 0.0f), dodoe::Vector3f(x1, y1, 0.0f), ghostColor);
        AddLine(data, dodoe::Vector3f(x1, y1, 0.0f), dodoe::Vector3f(x0, y1, 0.0f), ghostColor);
        AddLine(data, dodoe::Vector3f(x0, y1, 0.0f), dodoe::Vector3f(x0, y0, 0.0f), ghostColor);
    }

    data.has_data = true;
}

void RuntimeEditorBackend::updateGizmo()
{
    DO_PROFILE_SCOPE_CATEGORY("Cakery::updateGizmo", "frame");
    dodoe::GizmoChannelData& channel_data = dodoe::GetGizmoChannel().get<dodoe::GizmoChannelData>();
    channel_data.clear();
    const bool tilePainting = m_tilePaint && m_tilePaint->hasTarget() &&
                              m_tilePaint->tool() != TileTool::Select;
    updateTileOverlay();
    if (tilePainting || m_gizmoMode == "none" || m_selectedUuid == 0) {
        return;
    }
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return;
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        return;
    }
    dodoe::Entity entity = scene->tryGetEntityByUUID(dodoe::UUID(m_selectedUuid));
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>()) {
        return;
    }
    const dodoe::Vector3f position = entity.getComponent<dodoe::TransformComponent>().getPosition();
    if (m_gizmoMode == "translate") {
        GenerateTranslateGizmo(channel_data, position);
    } else if (m_gizmoMode == "rotate") {
        GenerateRotateGizmo(channel_data, position);
    } else if (m_gizmoMode == "scale") {
        GenerateScaleGizmo(channel_data, position);
    }
}

void RuntimeEditorBackend::pickAt(float screenX, float screenY)
{
    if (!m_camera) {
        return;
    }
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return;
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        return;
    }
    dodoe::Vector3f origin, dir;
    m_camera->screenToRay(screenX, screenY, origin, dir);
    dodoe::Entity entity = dodoe::PickingBackend::RaycastNearest(*scene, origin, dir);
    if (!entity.valid()) {
        // A miss in the current lightweight picker is not proof that the user
        // intended to clear the selection. Keep the Inspector stable until a
        // real entity hit is reported.
        return;
    }
    m_selectedUuid = static_cast<std::uint64_t>(entity.uuid());
    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"selection_changed", std::to_string(m_selectedUuid)});
    }
}

dodoe::Entity RuntimeEditorBackend::selectedSceneEntity() const
{
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return {};
    }
    Scene* scene = world->getActiveScene();
    if (!scene || m_selectedUuid == 0) {
        return {};
    }
    return scene->tryGetEntityByUUID(dodoe::UUID(m_selectedUuid));
}

int RuntimeEditorBackend::hitTestGizmo(float screenX, float screenY)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>() || !m_camera) {
        return -1;
    }
    const dodoe::Vector3f center = entity.getComponent<dodoe::TransformComponent>().getPosition();
    const float thresholdSq = kGizmoHitThresholdPx * kGizmoHitThresholdPx;
    int bestAxis = -1;
    float bestDist = thresholdSq;

    if (m_gizmoMode == "rotate") {
        for (Int32 axis = 0; axis < 3; ++axis) {
            float minDist = 1e30f;
            for (UInt32 i = 0; i < kRingSegments; ++i) {
                const Float a = static_cast<Float>(i) * 2.0f * 3.14159265f / static_cast<Float>(kRingSegments);
                const Float c = std::cos(a), s = std::sin(a);
                dodoe::Vector3f point;
                if (axis == 0) {
                    point = center + dodoe::Vector3f(0.0f, c, s) * kRingRadius;
                } else if (axis == 1) {
                    point = center + dodoe::Vector3f(c, 0.0f, s) * kRingRadius;
                } else {
                    point = center + dodoe::Vector3f(c, s, 0.0f) * kRingRadius;
                }
                const dodoe::Vector2f screenPt = m_camera->projectToScreen(point);
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
        const dodoe::Vector2f start = m_camera->projectToScreen(center);
        const dodoe::Vector2f end = m_camera->projectToScreen(center + kAxes[axis] * kHandleLength);
        const float dist = PointSegmentDistanceSq(screenX, screenY, start, end);
        if (dist < bestDist) {
            bestDist = dist;
            bestAxis = axis;
        }
    }
    return bestAxis;
}

void RuntimeEditorBackend::beginDrag(int axis, float screenX, float screenY)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>()) {
        return;
    }
    auto& transform = entity.getComponent<dodoe::TransformComponent>();
    m_dragMode = m_gizmoMode;
    m_dragAxis = axis;
    m_dragStartPosition = transform.getPosition();
    m_dragStartRotation = transform.getRotation();
    m_dragStartScale = transform.getScale();

    if (!m_camera) {
        m_dragAxis = -1;
        m_dragMode.clear();
        return;
    }

    if (m_dragMode == "translate" || m_dragMode == "scale") {
        dodoe::Vector3f origin, dir;
        m_camera->screenToRay(screenX, screenY, origin, dir);
        if (!RayPlaneIntersect(origin, dir, m_dragStartPosition, m_camera->forwardDirection(), m_dragPlanePoint)) {
            m_dragAxis = -1;
            m_dragMode.clear();
            return;
        }
    }

    if (m_dragMode == "scale") {
        const dodoe::Vector2f start = m_camera->projectToScreen(m_dragStartPosition);
        const dodoe::Vector2f end = m_camera->projectToScreen(
            m_dragStartPosition + kAxes[m_dragAxis] * kHandleLength);
        const dodoe::Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        // A view-facing axis has no screen-space direction in a 2D/editor
        // view. It cannot provide a meaningful drag distance.
        if (axisLengthSq < 1e-6f) {
            m_dragAxis = -1;
            m_dragMode.clear();
            return;
        }
        const dodoe::Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        m_dragStartAxisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
    }

    if (m_dragMode == "rotate") {
        const dodoe::Vector2f centerScreen = m_camera->projectToScreen(m_dragStartPosition);
        m_dragStartAngle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
    }

    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"transform_drag_begin", ""});
    }
}

void RuntimeEditorBackend::updateDrag(float screenX, float screenY)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>() || !m_camera) {
        return;
    }
    auto& transform = entity.getComponent<dodoe::TransformComponent>();

    if (m_dragMode == "translate") {
        dodoe::Vector3f origin, dir;
        m_camera->screenToRay(screenX, screenY, origin, dir);
        dodoe::Vector3f planePoint;
        if (!RayPlaneIntersect(origin, dir, m_dragStartPosition, m_camera->forwardDirection(), planePoint)) {
            return;
        }
        const float movement = dodoe::Math::Dot(planePoint - m_dragPlanePoint, kAxes[m_dragAxis]);
        const dodoe::Vector3f newPosition = m_dragStartPosition + kAxes[m_dragAxis] * movement;
        transform.setPosition(newPosition);
        emitTransformChange(newPosition, transform.getRotation(), transform.getScale());
    } else if (m_dragMode == "rotate") {
        const dodoe::Vector2f centerScreen = m_camera->projectToScreen(m_dragStartPosition);
        const float angle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
        const float deltaDegrees = (angle - m_dragStartAngle) * 180.0f / 3.14159265f;
        dodoe::Vector3f newRotation = m_dragStartRotation;
        newRotation[m_dragAxis] += deltaDegrees;
        transform.setRotation(newRotation);
        emitTransformChange(transform.getPosition(), newRotation, transform.getScale());
    } else if (m_dragMode == "scale") {
        const dodoe::Vector2f start = m_camera->projectToScreen(m_dragStartPosition);
        const dodoe::Vector2f end = m_camera->projectToScreen(
            m_dragStartPosition + kAxes[m_dragAxis] * kHandleLength);
        const dodoe::Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        if (axisLengthSq < 1e-6f) {
            return;
        }
        const dodoe::Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        const float axisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
        const float movement = axisParam - m_dragStartAxisParam;
        dodoe::Vector3f newScale = m_dragStartScale;
        newScale[m_dragAxis] = std::max(0.01f,
            m_dragStartScale[m_dragAxis] + movement * kHandleLength);
        transform.setScale(newScale);
        emitTransformChange(transform.getPosition(), transform.getRotation(), newScale);
    }

    if (m_camera) {
        m_camera->updateLastMouse(screenX, screenY);
    }
}

void RuntimeEditorBackend::endDrag()
{
    m_dragAxis = -1;
    m_dragMode.clear();
    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"transform_drag_end", ""});
    }
}

void RuntimeEditorBackend::emitTransformChange(const dodoe::Vector3f& position,
                                               const dodoe::Vector3f& rotation,
                                               const dodoe::Vector3f& scale)
{
    if (!m_eventCallback || m_playState != "edit") {
        return;
    }
    nlohmann::json payload = {
        {"uuid", m_selectedUuid},
        {"value", {
            {"position", {position.x, position.y, position.z}},
            {"rotation", {rotation.x, rotation.y, rotation.z}},
            {"scale", {scale.x, scale.y, scale.z}},
        }},
    };
    m_eventCallback(BackendEventMessage{"transform_changed", payload.dump()});
}

} // namespace cakery
