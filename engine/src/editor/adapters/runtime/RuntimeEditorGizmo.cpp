// do@Redlive

#include "RuntimeEditorBackend.h"

#include "EditorCamera.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/EditorSession.h"

#include "runtime/core/channel/gizmo_channel.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/instrumentor.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/service/editor/picking_backend.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/world.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace dodoe;

namespace cakery {

namespace {

constexpr Float kHandleLength = 1.0f;
constexpr Float kRingRadius = 0.9f;
constexpr UInt32 kRingSegments = 48;
constexpr Float kGizmoHitThresholdPx = 12.0f;

const dodoe::Color kAxisRed{1.0f, 0.2f, 0.2f, 1.0f};
const dodoe::Color kAxisGreen{0.2f, 1.0f, 0.2f, 1.0f};
const dodoe::Color kAxisBlue{0.2f, 0.4f, 1.0f, 1.0f};
const dodoe::Vector3f kAxes[3] = {
    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
};
const dodoe::Color kColors[3] = {kAxisRed, kAxisGreen, kAxisBlue};
const dodoe::Color kHoverColors[3] = {
    {1.0f, 0.62f, 0.35f, 1.0f},
    {0.55f, 1.0f, 0.45f, 1.0f},
    {0.45f, 0.72f, 1.0f, 1.0f},
};
const dodoe::Color kSelectionColor{1.0f, 0.82f, 0.25f, 0.95f};

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

struct EditorGizmoPart {
    DynamicArray<dodoe::Vector3f> positions;
    DynamicArray<UInt32> indices;
};

enum class GizmoPartId { Arrow, Shaft, Ring, Cube };

void LoadGizmoPart(EditorGizmoPart& part, const char* file) {
    const auto path = FileSystem::GetEngineResPath() / "models" / "editor" / file;
    std::ifstream stream(path);
    if (!stream.is_open()) {
        DO_ERROR("GizmoPart: failed to open '{}'", path.string());
        return;
    }
    std::string line;
    std::string token;
    while (std::getline(stream, line)) {
        if (line.rfind("v ", 0) == 0) {
            std::istringstream iss(line.substr(2));
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (iss >> x >> y >> z) {
                part.positions.push_back(dodoe::Vector3f(x, y, z));
            }
        } else if (line.rfind("f ", 0) == 0) {
            std::istringstream iss(line.substr(2));
            DynamicArray<UInt32> face;
            face.reserve(4);
            while (iss >> token) {
                const Size_t slash = token.find('/');
                const std::string index_text =
                    (slash == std::string::npos) ? token : token.substr(0, slash);
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

void AppendGizmoPart(dodoe::GizmoChannelData& data, const EditorGizmoPart& part,
                     const dodoe::Vector3f& axis, const dodoe::Vector3f& origin,
                     float scale, const dodoe::Color& color) {
    if (part.positions.empty() || part.indices.empty()) {
        return;
    }
    const dodoe::Vector3f reference =
        (std::abs(axis.z) > 0.9f) ? dodoe::Vector3f(1.0f, 0.0f, 0.0f) : dodoe::Vector3f(0.0f, 0.0f, 1.0f);
    const dodoe::Vector3f basis_u = dodoe::Math::Normalize(dodoe::Math::Cross(axis, reference));
    const dodoe::Vector3f basis_v = dodoe::Math::Cross(basis_u, axis);

    const UInt32 base_vertex = static_cast<UInt32>(data.vertices.size());
    const UInt32 base_index  = static_cast<UInt32>(data.indices.size());

    for (const dodoe::Vector3f& p : part.positions) {
        const dodoe::Vector3f world = origin +
            (basis_u * p.x + axis * p.y + basis_v * p.z) * scale;
        data.vertices.push_back(MakeVertex(world, color));
    }
    for (const UInt32 index : part.indices) {
        data.indices.push_back(index);
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

void GenerateTranslateGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position,
                            float scale, int hoverAxis) {
    const EditorGizmoPart& arrow = GetGizmoPart(GizmoPartId::Arrow);
    for (Int32 i = 0; i < 3; ++i) {
        const dodoe::Color& color = (i == hoverAxis) ? kHoverColors[i] : kColors[i];
        AppendGizmoPart(data, arrow, kAxes[i], position, scale, color);
    }
    data.has_data = true;
}

[[nodiscard]] float SnapToStep(float value, float step) {
    if (step <= 1e-6f) {
        return value;
    }
    return std::round(value / step) * step;
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

void GenerateRotateGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position,
                         float scale, int hoverAxis) {
    const EditorGizmoPart& ring = GetGizmoPart(GizmoPartId::Ring);
    for (Int32 axis = 0; axis < 3; ++axis) {
        const dodoe::Color& color = (axis == hoverAxis) ? kHoverColors[axis] : kColors[axis];
        AppendGizmoPart(data, ring, kAxes[axis], position, scale, color);
    }
    data.has_data = true;
}

void GenerateScaleGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position,
                        float scale, int hoverAxis) {
    const EditorGizmoPart& shaft = GetGizmoPart(GizmoPartId::Shaft);
    const EditorGizmoPart& cube = GetGizmoPart(GizmoPartId::Cube);
    for (Int32 i = 0; i < 3; ++i) {
        const dodoe::Color& color = (i == hoverAxis) ? kHoverColors[i] : kColors[i];
        const dodoe::Vector3f axis = kAxes[i];
        const dodoe::Vector3f tip = position + axis * (kHandleLength * scale);
        AppendGizmoPart(data, shaft, axis, position, scale, color);
        AppendGizmoPart(data, cube, axis, tip, scale, color);
    }
    data.has_data = true;
}

void DrawSelectionBox(dodoe::GizmoChannelData& data, const dodoe::Vector3f& center,
                      const dodoe::Vector3f& halfExtents) {
    const float x0 = center.x - halfExtents.x;
    const float x1 = center.x + halfExtents.x;
    const float y0 = center.y - halfExtents.y;
    const float y1 = center.y + halfExtents.y;
    const float z0 = center.z - halfExtents.z;
    const float z1 = center.z + halfExtents.z;
    const dodoe::Vector3f corners[8] = {
        {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
    };
    const UInt32 edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
    for (const auto& edge : edges) {
        AddLine(data, corners[edge[0]], corners[edge[1]], kSelectionColor);
    }
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
    if (!m_tilePaint || !m_tilePaint->hasTarget()) return;
    dodoe::Entity tm = activeTilemapEntity();
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) {
        m_tilePaint->setActiveTilemap(dodoe::UUID(0));
        m_tilePaint->setActiveLayer(dodoe::UUID(0));
        m_tilePaintActive = false;
        emitTilemapEditMode(false);
        return;
    }

    const auto& comp = tm.getComponent<TilemapComponent>();
    const float mapW = static_cast<float>(comp.map_width * comp.tile_width);
    const float mapH = static_cast<float>(comp.map_height * comp.tile_height);

    const dodoe::Color borderColor{1.0f, 1.0f, 1.0f, 0.9f};
    const dodoe::Color gridColor{1.0f, 1.0f, 1.0f, 0.10f};
    const dodoe::Color ghostColor{0.25f, 1.0f, 0.35f, 1.0f};
    const dodoe::Color selectColor{1.0f, 0.82f, 0.3f, 1.0f};

    auto& data = GetGizmoChannel().get<dodoe::GizmoChannelData>();

    auto addCellRect = [&data, &comp](int cellX, int cellY, int cellsW, int cellsH,
                                      const dodoe::Color& color) {
        const float x0 = static_cast<float>(cellX * comp.tile_width);
        const float y0 = static_cast<float>(cellY * comp.tile_height);
        const float x1 = x0 + static_cast<float>(cellsW * comp.tile_width);
        const float y1 = y0 + static_cast<float>(cellsH * comp.tile_height);
        AddLine(data, dodoe::Vector3f(x0, y0, 0.0f), dodoe::Vector3f(x1, y0, 0.0f), color);
        AddLine(data, dodoe::Vector3f(x1, y0, 0.0f), dodoe::Vector3f(x1, y1, 0.0f), color);
        AddLine(data, dodoe::Vector3f(x1, y1, 0.0f), dodoe::Vector3f(x0, y1, 0.0f), color);
        AddLine(data, dodoe::Vector3f(x0, y1, 0.0f), dodoe::Vector3f(x0, y0, 0.0f), color);
    };

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

    const TileTool tool = m_tilePaint->tool();
    if (tool == TileTool::Select) {
        if (m_tilePaint->isMoving() && m_tilePaint->hasAnchor()) {
            addCellRect(m_tilePaint->selectionX(), m_tilePaint->selectionY(),
                        m_tilePaint->selectionW(), m_tilePaint->selectionH(), gridColor);
            addCellRect(m_tilePaint->selectionX() + m_tilePaint->moveDeltaX(),
                        m_tilePaint->selectionY() + m_tilePaint->moveDeltaY(),
                        m_tilePaint->selectionW(), m_tilePaint->selectionH(), selectColor);
        } else if (m_tilePaint->isSelecting() && m_tilePaint->hasAnchor()) {
            const int x0 = std::min(m_tilePaint->anchorX(), m_tilePaint->lastX());
            const int y0 = std::min(m_tilePaint->anchorY(), m_tilePaint->lastY());
            const int w = std::abs(m_tilePaint->lastX() - m_tilePaint->anchorX()) + 1;
            const int h = std::abs(m_tilePaint->lastY() - m_tilePaint->anchorY()) + 1;
            addCellRect(x0, y0, w, h, selectColor);
        } else if (m_tilePaint->hasSelection()) {
            addCellRect(m_tilePaint->selectionX(), m_tilePaint->selectionY(),
                        m_tilePaint->selectionW(), m_tilePaint->selectionH(), selectColor);
        }
        data.has_data = true;
        return;
    }

    const TileBrush& brush = m_tilePaint->brush();
    if (m_tilePaint->hasAnchor() && (tool == TileTool::Line || tool == TileTool::Rect)) {
        const int ax = m_tilePaint->anchorX();
        const int ay = m_tilePaint->anchorY();
        const int lx = m_tilePaint->lastX();
        const int ly = m_tilePaint->lastY();
        if (tool == TileTool::Rect) {
            const int x0 = std::min(ax, lx);
            const int y0 = std::min(ay, ly);
            const int w = std::abs(lx - ax) + brush.w;
            const int h = std::abs(ly - ay) + brush.h;
            addCellRect(x0, y0, w, h, ghostColor);
        } else {
            int x = ax;
            int y = ay;
            const int dx = lx > ax ? lx - ax : ax - lx;
            const int dy = ly > ay ? ly - ay : ay - ly;
            const int sx = ax < lx ? 1 : -1;
            const int sy = ay < ly ? 1 : -1;
            int err = dx - dy;
            for (;;) {
                addCellRect(x, y, brush.w, brush.h, ghostColor);
                if (x == lx && y == ly) break;
                const int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x += sx; }
                if (e2 < dx)  { err += dx; y += sy; }
            }
        }
    } else if (m_tilePaint->hasHover()) {
        addCellRect(m_tilePaint->hoverX(), m_tilePaint->hoverY(), brush.w, brush.h, ghostColor);
    }

    data.has_data = true;
}

void RuntimeEditorBackend::updateGizmo()
{
    DO_PROFILE_SCOPE_CATEGORY("Cakery::updateGizmo", "frame");
    dodoe::GizmoChannelData& channel_data = dodoe::GetGizmoChannel().get<dodoe::GizmoChannelData>();
    channel_data.clear();
    channel_data.grid.ortho2d = m_camera && m_camera->mode() == EditorCamera::Mode::Ortho2D;
    const bool tilePainting = m_tilePaint && m_tilePaint->hasTarget() && activeTilemapEntity().valid();
    updateTileOverlay();
    if (tilePainting || m_selectedUuid == 0) {
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
    const dodoe::TransformComponent& transform = entity.getComponent<dodoe::TransformComponent>();
    const dodoe::Vector3f position = transform.getPosition();
    if (m_gizmoMode == "none") {
        drawSelectionHighlight(channel_data);
        return;
    }
    m_gizmoScale = std::max(computeGizmoScale(position), 1e-3f);
    if (m_gizmoMode == "translate") {
        GenerateTranslateGizmo(channel_data, position, m_gizmoScale, m_hoverAxis);
    } else if (m_gizmoMode == "rotate") {
        GenerateRotateGizmo(channel_data, position, m_gizmoScale, m_hoverAxis);
    } else if (m_gizmoMode == "scale") {
        GenerateScaleGizmo(channel_data, position, m_gizmoScale, m_hoverAxis);
    }
}

float RuntimeEditorBackend::computeGizmoScale(const dodoe::Vector3f& position) const
{
    if (!m_camera) {
        return 1.0f;
    }
    return m_camera->pixelsToWorld(position, 90.0f);
}

void RuntimeEditorBackend::drawSelectionHighlight(dodoe::GizmoChannelData& data)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>()) {
        return;
    }
    const dodoe::TransformComponent& transform = entity.getComponent<dodoe::TransformComponent>();
    const dodoe::Vector3f& p = transform.getPosition();
    const dodoe::Vector3f& s = transform.getScale();
    const dodoe::Vector3f half{0.5f * std::fabs(s.x), 0.5f * std::fabs(s.y), 0.5f * std::fabs(s.z)};
    DrawSelectionBox(data, p, half);
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
    setSelectedUuid(entity.valid() ? static_cast<std::uint64_t>(entity.uuid()) : 0);
}

void RuntimeEditorBackend::requestPick(float screenX, float screenY)
{
    if (!m_camera) {
        return;
    }
    auto& channel = dodoe::GetPickChannel().get<dodoe::PickChannelData>();
    const float dpr = m_surface.devicePixelRatio > 0.0f ? m_surface.devicePixelRatio : 1.0f;
    channel.request.x = static_cast<std::int32_t>(std::lround(screenX * dpr));
    channel.request.y = static_cast<std::int32_t>(std::lround(screenY * dpr));
    channel.request.sequence = ++m_pick_sequence;
    DO_INFO("EditorPick: request ({}, {}) seq={}", channel.request.x, channel.request.y, channel.request.sequence);
}

void RuntimeEditorBackend::setSelectedUuid(std::uint64_t uuid)
{
    if (uuid == 0) {
        if (m_selectedUuid != 0) {
            m_selectedUuid = 0;
            m_hoverAxis = -1;
            m_eventCallback(BackendEventMessage{"selection_changed", std::string()});
        }
        return;
    }
    m_selectedUuid = uuid;
    m_hoverAxis = -1;
    m_eventCallback(BackendEventMessage{"selection_changed", std::to_string(uuid)});
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

dodoe::Entity RuntimeEditorBackend::dragEntityByUuid(std::uint64_t uuid) const
{
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return {};
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        return {};
    }
    return scene->tryGetEntityByUUID(dodoe::UUID(uuid));
}

int RuntimeEditorBackend::hitTestGizmo(float screenX, float screenY)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>() || !m_camera) {
        return -1;
    }
    const dodoe::Vector3f center = entity.getComponent<dodoe::TransformComponent>().getPosition();
    const float thresholdSq = kGizmoHitThresholdPx * kGizmoHitThresholdPx;
    const float handleLength = kHandleLength * m_gizmoScale;
    const float ringRadius = kRingRadius * m_gizmoScale;
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
                    point = center + dodoe::Vector3f(0.0f, c, s) * ringRadius;
                } else if (axis == 1) {
                    point = center + dodoe::Vector3f(c, 0.0f, s) * ringRadius;
                } else {
                    point = center + dodoe::Vector3f(c, s, 0.0f) * ringRadius;
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
        const dodoe::Vector2f end = m_camera->projectToScreen(center + kAxes[axis] * handleLength);
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
    m_dragEntities.clear();

    if (m_session) {
        World* world = m_app ? m_app->context().getWorld() : nullptr;
        Scene* scene = world ? world->getActiveScene() : nullptr;
        if (scene) {
            for (const std::uint64_t uuid : m_session->selection().selectedAll()) {
                if (uuid == m_selectedUuid) {
                    continue;
                }
                dodoe::Entity other = scene->tryGetEntityByUUID(dodoe::UUID(uuid));
                if (!other.valid() || !other.hasComponent<dodoe::TransformComponent>()) {
                    continue;
                }
                auto& otherTransform = other.getComponent<dodoe::TransformComponent>();
                m_dragEntities.push_back({uuid, otherTransform.getPosition(),
                                          otherTransform.getRotation(), otherTransform.getScale()});
            }
        }
    }

    if (!m_camera) {
        m_dragAxis = -1;
        m_dragMode.clear();
        m_dragEntities.clear();
        return;
    }

    if (m_dragMode == "translate" || m_dragMode == "scale") {
        dodoe::Vector3f origin, dir;
        m_camera->screenToRay(screenX, screenY, origin, dir);
        if (!RayPlaneIntersect(origin, dir, m_dragStartPosition, m_camera->forwardDirection(), m_dragPlanePoint)) {
            m_dragAxis = -1;
            m_dragMode.clear();
            m_dragEntities.clear();
            return;
        }
    }

    if (m_dragMode == "scale") {
        const dodoe::Vector2f start = m_camera->projectToScreen(m_dragStartPosition);
        const dodoe::Vector2f end = m_camera->projectToScreen(
            m_dragStartPosition + kAxes[m_dragAxis] * kHandleLength * m_gizmoScale);
        const dodoe::Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        // A view-facing axis has no screen-space direction in a 2D/editor
        // view. It cannot provide a meaningful drag distance.
        if (axisLengthSq < 1e-6f) {
            m_dragAxis = -1;
            m_dragMode.clear();
            m_dragEntities.clear();
            return;
        }
        const dodoe::Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        m_dragStartAxisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
    }

    if (m_dragMode == "rotate") {
        const dodoe::Vector2f centerScreen = m_camera->projectToScreen(m_dragStartPosition);
        m_dragStartAngle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
    }

    m_eventCallback(BackendEventMessage{"transform_drag_begin", ""});
}

void RuntimeEditorBackend::updateDrag(float screenX, float screenY)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>() || !m_camera) {
        return;
    }
    auto& transform = entity.getComponent<dodoe::TransformComponent>();
    const bool snapping = m_snapEnabled || m_ctrlHeld;

    if (m_dragMode == "translate") {
        dodoe::Vector3f origin, dir;
        m_camera->screenToRay(screenX, screenY, origin, dir);
        dodoe::Vector3f planePoint;
        if (!RayPlaneIntersect(origin, dir, m_dragStartPosition, m_camera->forwardDirection(), planePoint)) {
            return;
        }
        float movement = dodoe::Math::Dot(planePoint - m_dragPlanePoint, kAxes[m_dragAxis]);
        if (snapping) {
            movement = SnapToStep(movement, m_translateSnap);
        }
        const dodoe::Vector3f newPosition = m_dragStartPosition + kAxes[m_dragAxis] * movement;
        const dodoe::Vector3f delta = newPosition - m_dragStartPosition;
        transform.setPosition(newPosition);

        std::vector<TransformUpdate> updates;
        updates.push_back({m_selectedUuid, newPosition, transform.getRotation(), transform.getScale()});
        for (const TransformUpdate& start : m_dragEntities) {
            dodoe::Entity other = dragEntityByUuid(start.uuid);
            if (!other.valid() || !other.hasComponent<dodoe::TransformComponent>()) {
                continue;
            }
            const dodoe::Vector3f otherPosition = start.position + delta;
            other.getComponent<dodoe::TransformComponent>().setPosition(otherPosition);
            updates.push_back({start.uuid, otherPosition, start.rotation, start.scale});
        }
        emitTransformChanges(updates);
    } else if (m_dragMode == "rotate") {
        const dodoe::Vector2f centerScreen = m_camera->projectToScreen(m_dragStartPosition);
        const float angle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
        float deltaDegrees = (angle - m_dragStartAngle) * 180.0f / 3.14159265f;
        if (snapping) {
            deltaDegrees = SnapToStep(deltaDegrees, m_rotateSnap);
        }
        dodoe::Vector3f newRotation = m_dragStartRotation;
        newRotation[m_dragAxis] += deltaDegrees;
        transform.setRotation(newRotation);

        std::vector<TransformUpdate> updates;
        updates.push_back({m_selectedUuid, m_dragStartPosition, newRotation, m_dragStartScale});
        for (const TransformUpdate& start : m_dragEntities) {
            dodoe::Entity other = dragEntityByUuid(start.uuid);
            if (!other.valid() || !other.hasComponent<dodoe::TransformComponent>()) {
                continue;
            }
            dodoe::Vector3f otherRotation = start.rotation;
            otherRotation[m_dragAxis] += deltaDegrees;
            other.getComponent<dodoe::TransformComponent>().setRotation(otherRotation);
            updates.push_back({start.uuid, start.position, otherRotation, start.scale});
        }
        emitTransformChanges(updates);
    } else if (m_dragMode == "scale") {
        const dodoe::Vector2f start = m_camera->projectToScreen(m_dragStartPosition);
        const dodoe::Vector2f end = m_camera->projectToScreen(
            m_dragStartPosition + kAxes[m_dragAxis] * kHandleLength * m_gizmoScale);
        const dodoe::Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        if (axisLengthSq < 1e-6f) {
            return;
        }
        const dodoe::Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        const float axisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
        const float movement = axisParam - m_dragStartAxisParam;
        dodoe::Vector3f newScale = m_dragStartScale;
        float primaryScale = m_dragStartScale[m_dragAxis] + movement * kHandleLength * m_gizmoScale;
        if (snapping) {
            primaryScale = SnapToStep(primaryScale, m_scaleSnap);
        }
        newScale[m_dragAxis] = std::max(0.01f, primaryScale);
        transform.setScale(newScale);

        std::vector<TransformUpdate> updates;
        updates.push_back({m_selectedUuid, m_dragStartPosition, m_dragStartRotation, newScale});
        for (const TransformUpdate& dragStart : m_dragEntities) {
            dodoe::Entity other = dragEntityByUuid(dragStart.uuid);
            if (!other.valid() || !other.hasComponent<dodoe::TransformComponent>()) {
                continue;
            }
            dodoe::Vector3f otherScale = dragStart.scale;
            float otherAxis = dragStart.scale[m_dragAxis] + movement * kHandleLength * m_gizmoScale;
            if (snapping) {
                otherAxis = SnapToStep(otherAxis, m_scaleSnap);
            }
            otherScale[m_dragAxis] = std::max(0.01f, otherAxis);
            other.getComponent<dodoe::TransformComponent>().setScale(otherScale);
            updates.push_back({dragStart.uuid, dragStart.position, dragStart.rotation, otherScale});
        }
        emitTransformChanges(updates);
    }

    if (m_camera) {
        m_camera->updateLastMouse(screenX, screenY);
    }
}

void RuntimeEditorBackend::endDrag()
{
    m_dragAxis = -1;
    m_dragMode.clear();
    m_hoverAxis = -1;
    m_eventCallback(BackendEventMessage{"transform_drag_end", ""});
}

void RuntimeEditorBackend::emitTransformChange(const dodoe::Vector3f& position,
                                               const dodoe::Vector3f& rotation,
                                               const dodoe::Vector3f& scale)
{
    if (m_playState != "edit") {
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

void RuntimeEditorBackend::emitTransformChanges(const std::vector<TransformUpdate>& updates)
{
    if (m_playState != "edit") {
        return;
    }
    if (updates.size() == 1) {
        emitTransformChange(updates[0].position, updates[0].rotation, updates[0].scale);
        return;
    }
    nlohmann::json entities = nlohmann::json::array();
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
    nlohmann::json payload = {{"entities", std::move(entities)}};
    m_eventCallback(BackendEventMessage{"transform_changed", payload.dump()});
}

} // namespace cakery
