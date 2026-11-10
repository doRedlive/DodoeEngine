// do@Redlive

#include "dopch.h"

#include "RuntimeBackend.h"

#include "EditorCamera.h"
#include "services/UuidResolve.h"
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

const Color kAxisRed{1.0f, 0.2f, 0.2f, 1.0f};
const Color kAxisGreen{0.2f, 1.0f, 0.2f, 1.0f};
const Color kAxisBlue{0.2f, 0.4f, 1.0f, 1.0f};
const Vector3f kAxes[3] = {
    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
};
const Color kColors[3] = {kAxisRed, kAxisGreen, kAxisBlue};
const Color kHoverColors[3] = {
    {1.0f, 0.62f, 0.35f, 1.0f},
    {0.55f, 1.0f, 0.45f, 1.0f},
    {0.45f, 0.72f, 1.0f, 1.0f},
};
const Color kSelectionColor{1.0f, 0.82f, 0.25f, 0.95f};

[[nodiscard]] GizmoVertex MakeVertex(const Vector3f& pos, const Color& color) {
    return {pos.x, pos.y, pos.z, color.r, color.g, color.b, color.a};
}

void AddLine(GizmoChannelData& data, const Vector3f& start, const Vector3f& end,
             const Color& color) {
    const UInt32 base = static_cast<UInt32>(data.vertices.size());
    data.vertices.push_back(MakeVertex(start, color));
    data.vertices.push_back(MakeVertex(end, color));

    GizmoDrawCommand cmd;
    cmd.vertex_offset = base;
    cmd.vertex_count  = 2;
    cmd.index_offset  = 0;
    cmd.index_count   = 0;
    cmd.topology      = GfxPrimitiveType::LineList;
    cmd.transform     = Matrix4f(1.0f);
    data.commands.push_back(cmd);
}

struct EditorGizmoPart {
    DynamicArray<Vector3f> positions;
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
                const int index = std::atoi(index_text);
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

void DrawSelectionBox(GizmoChannelData& data, const Vector3f& center,
                      const Vector3f& halfExtents) {
    const float x0 = center.x - halfExtents.x;
    const float x1 = center.x + halfExtents.x;
    const float y0 = center.y - halfExtents.y;
    const float y1 = center.y + halfExtents.y;
    const float z0 = center.z - halfExtents.z;
    const float z1 = center.z + halfExtents.z;
    const Vector3f corners[8] = {
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

constexpr float k2DHandlePixels = 8.0f;
constexpr float k2DArrowPixels = 56.0f;
constexpr float k2DRotateKnobPixels = 10.0f;

void AddRect2D(GizmoChannelData& data, const Vector2f& min, const Vector2f& max,
               float z, const Color& color) {
    AddLine(data, {min.x, min.y, z}, {max.x, min.y, z}, color);
    AddLine(data, {max.x, min.y, z}, {max.x, max.y, z}, color);
    AddLine(data, {max.x, max.y, z}, {min.x, max.y, z}, color);
    AddLine(data, {min.x, max.y, z}, {min.x, min.y, z}, color);
}

void AddHandle2D(GizmoChannelData& data, const Vector2f& center, float half,
                 float z, const Color& color) {
    AddRect2D(data, {center.x - half, center.y - half}, {center.x + half, center.y + half}, z, color);
}

void AddCircle2D(GizmoChannelData& data, const Vector2f& center, float radius,
                 float z, const Color& color) {
    constexpr int kSegments = 24;
    constexpr float kTwoPi = 6.2831853f;
    Vector2f prev(center.x + radius, center.y);
    for (int i = 1; i <= kSegments; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(kSegments)) * kTwoPi;
        const Vector2f next(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
        AddLine(data, {prev.x, prev.y, z}, {next.x, next.y, z}, color);
        prev = next;
    }
}

void AddArrow2D(GizmoChannelData& data, const Vector2f& origin,
                const Vector2f& dir, float length, float z, const Color& color) {
    const float tipX = origin.x + dir.x * length;
    const float tipY = origin.y + dir.y * length;
    const float head = length * 0.25f;
    const float perpX = -dir.y;
    const float perpY = dir.x;
    AddLine(data, {origin.x, origin.y, z}, {tipX, tipY, z}, color);
    AddLine(data, {tipX, tipY, z}, {tipX - dir.x * head + perpX * head, tipY - dir.y * head + perpY * head, z}, color);
    AddLine(data, {tipX, tipY, z}, {tipX - dir.x * head - perpX * head, tipY - dir.y * head - perpY * head, z}, color);
}

void Generate2DGizmo(GizmoChannelData& data, cakery::EditorCamera& camera,
                     const Vector3f& position, const Vector3f& scale,
                     const String& mode, int hoverAxis) {
    const float z = position.z;
    const float halfW = 0.5f * std::fabs(scale.x);
    const float halfH = 0.5f * std::fabs(scale.y);
    const float x0 = position.x - halfW;
    const float x1 = position.x + halfW;
    const float y0 = position.y - halfH;
    const float y1 = position.y + halfH;

    AddRect2D(data, {x0, y0}, {x1, y1}, z, kSelectionColor);

    const float handle_half = std::max(camera.pixelsToWorld(position, k2DHandlePixels) * 0.5f, 1e-4f);
    const Vector2f handle_points[8] = {
        {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1},
        {(x0 + x1) * 0.5f, y0}, {x1, (y0 + y1) * 0.5f},
        {(x0 + x1) * 0.5f, y1}, {x0, (y0 + y1) * 0.5f},
    };
    for (const auto& p : handle_points) {
        AddHandle2D(data, p, handle_half, z, kSelectionColor);
    }

    if (mode == "rotate") {
        const float knob_radius = std::max(camera.pixelsToWorld(position, k2DRotateKnobPixels), 1e-4f);
        const float stem = handle_half * 4.0f;
        AddLine(data, {position.x, y1, z}, {position.x, y1 + stem, z}, kSelectionColor);
        AddCircle2D(data, {position.x, y1 + stem}, knob_radius, z, kSelectionColor);
    } else if (mode == "translate") {
        const float len = std::max(camera.pixelsToWorld(position, k2DArrowPixels), 1e-4f);
        const Color& color_x = (hoverAxis == 0) ? kHoverColors[0] : kColors[0];
        const Color& color_y = (hoverAxis == 1) ? kHoverColors[1] : kColors[1];
        AddArrow2D(data, {position.x, position.y}, {1.0f, 0.0f}, len, z, color_x);
        AddArrow2D(data, {position.x, position.y}, {0.0f, 1.0f}, len, z, color_y);
    }

    data.has_data = true;
}

} // anonymous namespace

Entity RuntimeBackend::activeTilemapEntity() const
{
    if (!m_tilePaint || !m_tilePaint->hasTarget()) return {};
    World* world = runtimeWorld();
    Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) return {};
    return ResolveEntity(scene, m_tilePaint->activeTilemap());
}

void RuntimeBackend::updateTileOverlay()
{
    if (!m_tilePaint || !m_tilePaint->hasTarget()) return;
    Entity tm = activeTilemapEntity();
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) {
        m_tilePaint->setActiveTilemap(UUID(0));
        m_tilePaint->setActiveLayer(UUID(0));
        m_tilePaintActive = false;
        emitTilemapEditMode(false);
        return;
    }

    const auto& comp = tm.getComponent<TilemapComponent>();
    const float mapW = static_cast<float>(comp.map_width * comp.tile_width);
    const float mapH = static_cast<float>(comp.map_height * comp.tile_height);

    const Color borderColor{1.0f, 1.0f, 1.0f, 0.9f};
    const Color gridColor{1.0f, 1.0f, 1.0f, 0.10f};
    const Color ghostColor{0.25f, 1.0f, 0.35f, 1.0f};
    const Color selectColor{1.0f, 0.82f, 0.3f, 1.0f};

    auto& data = GetGizmoChannel().get<GizmoChannelData>();

    auto addCellRect = [&data, &comp](int cellX, int cellY, int cellsW, int cellsH,
                                      const Color& color) {
        const float x0 = static_cast<float>(cellX * comp.tile_width);
        const float y0 = static_cast<float>(cellY * comp.tile_height);
        const float x1 = x0 + static_cast<float>(cellsW * comp.tile_width);
        const float y1 = y0 + static_cast<float>(cellsH * comp.tile_height);
        AddLine(data, Vector3f(x0, y0, 0.0f), Vector3f(x1, y0, 0.0f), color);
        AddLine(data, Vector3f(x1, y0, 0.0f), Vector3f(x1, y1, 0.0f), color);
        AddLine(data, Vector3f(x1, y1, 0.0f), Vector3f(x0, y1, 0.0f), color);
        AddLine(data, Vector3f(x0, y1, 0.0f), Vector3f(x0, y0, 0.0f), color);
    };

    AddLine(data, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(mapW, 0.0f, 0.0f), borderColor);
    AddLine(data, Vector3f(mapW, 0.0f, 0.0f), Vector3f(mapW, mapH, 0.0f), borderColor);
    AddLine(data, Vector3f(mapW, mapH, 0.0f), Vector3f(0.0f, mapH, 0.0f), borderColor);
    AddLine(data, Vector3f(0.0f, mapH, 0.0f), Vector3f(0.0f, 0.0f, 0.0f), borderColor);

    const int gw = static_cast<int>(comp.map_width);
    const int gh = static_cast<int>(comp.map_height);
    if (gw <= 256 && gh <= 256) {
        for (int x = 1; x < gw; ++x) {
            const float px = static_cast<float>(x * comp.tile_width);
            AddLine(data, Vector3f(px, 0.0f, 0.0f), Vector3f(px, mapH, 0.0f), gridColor);
        }
        for (int y = 1; y < gh; ++y) {
            const float py = static_cast<float>(y * comp.tile_height);
            AddLine(data, Vector3f(0.0f, py, 0.0f), Vector3f(mapW, py, 0.0f), gridColor);
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

void RuntimeBackend::updateGizmo()
{
    DO_PROFILE_SCOPE_CATEGORY("Cakery::updateGizmo", "frame");
    GizmoChannelData& channel_data = GetGizmoChannel().get<GizmoChannelData>();
    channel_data.clear();
    channel_data.grid.ortho2d = m_camera && m_camera->mode() == EditorCamera::Mode::Ortho2D;
    if (channel_data.grid.ortho2d) {
        const float world_per_px = m_camera->pixelsToWorld(Vector3f(0.0f), 1.0f);
        const float target_px = 24.0f;
        float minor = 1000.0f;
        for (const float base : {0.01f, 0.02f, 0.05f, 0.1f, 0.2f, 0.5f,
                                 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f}) {
            minor = base;
            if (base / world_per_px >= target_px) {
                break;
            }
        }
        channel_data.grid.minor_spacing = minor;
        channel_data.grid.major_spacing = minor * 5.0f;
        channel_data.grid.fade_begin = 1.0e8f;
        channel_data.grid.fade_end = 1.0e8f + 1.0f;
    }
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
    Entity entity = scene->tryGetEntityByUUID(UUID(m_selectedUuid));
    if (!entity || !entity.hasComponent<TransformComponent>()) {
        return;
    }
    const TransformComponent& transform = entity.getComponent<TransformComponent>();
    const Vector3f position = transform.getPosition();
    if (m_gizmoMode == "none") {
        drawSelectionHighlight(channel_data);
        return;
    }
    if (m_camera && m_camera->mode() == EditorCamera::Mode::Ortho2D) {
        Generate2DGizmo(channel_data, *m_camera, position, transform.getScale(), m_gizmoMode, m_hoverAxis);
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

float RuntimeBackend::computeGizmoScale(const Vector3f& position) const
{
    if (!m_camera) {
        return 1.0f;
    }
    return m_camera->pixelsToWorld(position, 90.0f);
}

void RuntimeBackend::drawSelectionHighlight(GizmoChannelData& data)
{
    Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<TransformComponent>()) {
        return;
    }
    const TransformComponent& transform = entity.getComponent<TransformComponent>();
    const Vector3f& p = transform.getPosition();
    const Vector3f& s = transform.getScale();
    const Vector3f half{0.5f * std::fabs(s.x), 0.5f * std::fabs(s.y), 0.5f * std::fabs(s.z)};
    DrawSelectionBox(data, p, half);
}

void RuntimeBackend::pickAt(float screenX, float screenY)
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
    Vector3f origin, dir;
    m_camera->screenToRay(screenX, screenY, origin, dir);
    Entity entity = PickingBackend::RaycastNearest(*scene, origin, dir);
    setSelectedUuid(entity.valid() ? static_cast<UInt64>(entity.uuid()) : 0);
}

void RuntimeBackend::requestPick(float screenX, float screenY)
{
    if (!m_camera) {
        return;
    }
    auto& channel = GetPickChannel().get<PickChannelData>();
    const float dpr = m_surface.devicePixelRatio > 0.0f ? m_surface.devicePixelRatio : 1.0f;
    channel.request.x = static_cast<Int32>(std::lround(screenX * dpr));
    channel.request.y = static_cast<Int32>(std::lround(screenY * dpr));
    channel.request.sequence = ++m_pick_sequence;
    DO_INFO("EditorPick: request ({}, {}) seq={}", channel.request.x, channel.request.y, channel.request.sequence);
}

void RuntimeBackend::setSelectedUuid(UInt64 uuid)
{
    if (uuid == 0) {
        if (m_selectedUuid != 0) {
            m_selectedUuid = 0;
            m_hoverAxis = -1;
            m_eventCallback(BackendEventMessage{"selection_changed", String()});
        }
        return;
    }
    m_selectedUuid = uuid;
    m_hoverAxis = -1;
    m_eventCallback(BackendEventMessage{"selection_changed", ToString(uuid)});
}

Entity RuntimeBackend::selectedSceneEntity() const
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
    return scene->tryGetEntityByUUID(UUID(m_selectedUuid));
}

Entity RuntimeBackend::dragEntityByUuid(UInt64 uuid) const
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
    return scene->tryGetEntityByUUID(UUID(uuid));
}

int RuntimeBackend::hitTestGizmo(float screenX, float screenY)
{
    Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<TransformComponent>() || !m_camera) {
        return -1;
    }
    const Vector3f center = entity.getComponent<TransformComponent>().getPosition();
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
                Vector3f point;
                if (axis == 0) {
                    point = center + Vector3f(0.0f, c, s) * ringRadius;
                } else if (axis == 1) {
                    point = center + Vector3f(c, 0.0f, s) * ringRadius;
                } else {
                    point = center + Vector3f(c, s, 0.0f) * ringRadius;
                }
                const Vector2f screenPt = m_camera->projectToScreen(point);
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
        const Vector2f start = m_camera->projectToScreen(center);
        const Vector2f end = m_camera->projectToScreen(center + kAxes[axis] * handleLength);
        const float dist = PointSegmentDistanceSq(screenX, screenY, start, end);
        if (dist < bestDist) {
            bestDist = dist;
            bestAxis = axis;
        }
    }
    return bestAxis;
}

void RuntimeBackend::beginDrag(int axis, float screenX, float screenY)
{
    Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<TransformComponent>()) {
        return;
    }
    auto& transform = entity.getComponent<TransformComponent>();
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
            for (const UInt64 uuid : m_session->selection().selectedAll()) {
                if (uuid == m_selectedUuid) {
                    continue;
                }
                Entity other = scene->tryGetEntityByUUID(UUID(uuid));
                if (!other.valid() || !other.hasComponent<TransformComponent>()) {
                    continue;
                }
                auto& otherTransform = other.getComponent<TransformComponent>();
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
        Vector3f origin, dir;
        m_camera->screenToRay(screenX, screenY, origin, dir);
        if (!RayPlaneIntersect(origin, dir, m_dragStartPosition, m_camera->forwardDirection(), m_dragPlanePoint)) {
            m_dragAxis = -1;
            m_dragMode.clear();
            m_dragEntities.clear();
            return;
        }
    }

    if (m_dragMode == "scale") {
        const Vector2f start = m_camera->projectToScreen(m_dragStartPosition);
        const Vector2f end = m_camera->projectToScreen(
            m_dragStartPosition + kAxes[m_dragAxis] * kHandleLength * m_gizmoScale);
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
        const Vector2f centerScreen = m_camera->projectToScreen(m_dragStartPosition);
        m_dragStartAngle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
    }

    m_eventCallback(BackendEventMessage{"transform_drag_begin", ""});
}

void RuntimeBackend::updateDrag(float screenX, float screenY)
{
    Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<TransformComponent>() || !m_camera) {
        return;
    }
    auto& transform = entity.getComponent<TransformComponent>();
    const bool snapping = m_snapEnabled || m_ctrlHeld;

    if (m_dragMode == "translate") {
        Vector3f origin, dir;
        m_camera->screenToRay(screenX, screenY, origin, dir);
        Vector3f planePoint;
        if (!RayPlaneIntersect(origin, dir, m_dragStartPosition, m_camera->forwardDirection(), planePoint)) {
            return;
        }
        float movement = Math::Dot(planePoint - m_dragPlanePoint, kAxes[m_dragAxis]);
        if (snapping) {
            movement = SnapToStep(movement, m_translateSnap);
        }
        const Vector3f newPosition = m_dragStartPosition + kAxes[m_dragAxis] * movement;
        const Vector3f delta = newPosition - m_dragStartPosition;
        transform.setPosition(newPosition);

        DynamicArray<TransformUpdate> updates;
        updates.push_back({m_selectedUuid, newPosition, transform.getRotation(), transform.getScale()});
        for (const TransformUpdate& start : m_dragEntities) {
            Entity other = dragEntityByUuid(start.uuid);
            if (!other.valid() || !other.hasComponent<TransformComponent>()) {
                continue;
            }
            const Vector3f otherPosition = start.position + delta;
            other.getComponent<TransformComponent>().setPosition(otherPosition);
            updates.push_back({start.uuid, otherPosition, start.rotation, start.scale});
        }
        emitTransformChanges(updates);
    } else if (m_dragMode == "rotate") {
        const Vector2f centerScreen = m_camera->projectToScreen(m_dragStartPosition);
        const float angle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
        float deltaDegrees = (angle - m_dragStartAngle) * 180.0f / 3.14159265f;
        if (snapping) {
            deltaDegrees = SnapToStep(deltaDegrees, m_rotateSnap);
        }
        Vector3f newRotation = m_dragStartRotation;
        newRotation[m_dragAxis] += deltaDegrees;
        transform.setRotation(newRotation);

        DynamicArray<TransformUpdate> updates;
        updates.push_back({m_selectedUuid, m_dragStartPosition, newRotation, m_dragStartScale});
        for (const TransformUpdate& start : m_dragEntities) {
            Entity other = dragEntityByUuid(start.uuid);
            if (!other.valid() || !other.hasComponent<TransformComponent>()) {
                continue;
            }
            Vector3f otherRotation = start.rotation;
            otherRotation[m_dragAxis] += deltaDegrees;
            other.getComponent<TransformComponent>().setRotation(otherRotation);
            updates.push_back({start.uuid, start.position, otherRotation, start.scale});
        }
        emitTransformChanges(updates);
    } else if (m_dragMode == "scale") {
        const Vector2f start = m_camera->projectToScreen(m_dragStartPosition);
        const Vector2f end = m_camera->projectToScreen(
            m_dragStartPosition + kAxes[m_dragAxis] * kHandleLength * m_gizmoScale);
        const Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        if (axisLengthSq < 1e-6f) {
            return;
        }
        const Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        const float axisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
        const float movement = axisParam - m_dragStartAxisParam;
        Vector3f newScale = m_dragStartScale;
        float primaryScale = m_dragStartScale[m_dragAxis] + movement * kHandleLength * m_gizmoScale;
        if (snapping) {
            primaryScale = SnapToStep(primaryScale, m_scaleSnap);
        }
        newScale[m_dragAxis] = std::max(0.01f, primaryScale);
        transform.setScale(newScale);

        DynamicArray<TransformUpdate> updates;
        updates.push_back({m_selectedUuid, m_dragStartPosition, m_dragStartRotation, newScale});
        for (const TransformUpdate& dragStart : m_dragEntities) {
            Entity other = dragEntityByUuid(dragStart.uuid);
            if (!other.valid() || !other.hasComponent<TransformComponent>()) {
                continue;
            }
            Vector3f otherScale = dragStart.scale;
            float otherAxis = dragStart.scale[m_dragAxis] + movement * kHandleLength * m_gizmoScale;
            if (snapping) {
                otherAxis = SnapToStep(otherAxis, m_scaleSnap);
            }
            otherScale[m_dragAxis] = std::max(0.01f, otherAxis);
            other.getComponent<TransformComponent>().setScale(otherScale);
            updates.push_back({dragStart.uuid, dragStart.position, dragStart.rotation, otherScale});
        }
        emitTransformChanges(updates);
    }

    if (m_camera) {
        m_camera->updateLastMouse(screenX, screenY);
    }
}

void RuntimeBackend::endDrag()
{
    m_dragAxis = -1;
    m_dragMode.clear();
    m_hoverAxis = -1;
    m_eventCallback(BackendEventMessage{"transform_drag_end", ""});
}

void RuntimeBackend::emitTransformChange(const Vector3f& position,
                                               const Vector3f& rotation,
                                               const Vector3f& scale)
{
    if (m_playState != "edit") {
        return;
    }
    Json payload = {
        {"uuid", m_selectedUuid},
        {"value", {
            {"position", {position.x, position.y, position.z}},
            {"rotation", {rotation.x, rotation.y, rotation.z}},
            {"scale", {scale.x, scale.y, scale.z}},
        }},
    };
    m_eventCallback(BackendEventMessage{"transform_changed", ToString(payload)});
}

void RuntimeBackend::emitTransformChanges(const DynamicArray<TransformUpdate>& updates)
{
    if (m_playState != "edit") {
        return;
    }
    if (updates.size() == 1) {
        emitTransformChange(updates[0].position, updates[0].rotation, updates[0].scale);
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
    m_eventCallback(BackendEventMessage{"transform_changed", ToString(payload)});
}

} // namespace cakery
