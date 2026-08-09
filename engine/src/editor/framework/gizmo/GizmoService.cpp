// do@Redlive

#include "GizmoService.h"
#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"
#include "framework/selection/SelectionManager.h"

#include "runtime/core/channel/gizmo_channel.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/util.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/components/transform_component.h"

namespace cakery {
namespace {

    constexpr Float kHandleLength = 1.0f;
    constexpr Float kArrowHeadLength = 0.15f;
    constexpr Float kArrowHeadRadius = 0.04f;
    constexpr UInt32 kArrowHeadSegments = 4;

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
        const dodoe::Color kRed{1.0f, 0.2f, 0.2f, 1.0f};
        const dodoe::Color kGreen{0.2f, 1.0f, 0.2f, 1.0f};
        const dodoe::Color kBlue{0.2f, 0.4f, 1.0f, 1.0f};

        const dodoe::Matrix4f translation = dodoe::Math::Translate(dodoe::Matrix4f(1.0f), position);

        const dodoe::Vector3f kAxes[3] = {
            {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
        };
        const dodoe::Color kColors[3] = {kRed, kGreen, kBlue};

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

} // anonymous namespace

void GizmoService::update() {
    dodoe::GizmoChannelData& channel_data = dodoe::GetGizmoChannel().get<dodoe::GizmoChannelData>();
    channel_data.clear();

    if (m_mode == GizmoMode::None) return;

    auto& sel = m_ctx.selection();
    if (sel.empty()) return;

    const auto uuid = sel.primary();
    if (!uuid.isValid()) return;

    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    dodoe::Entity entity = scene->tryGetEntityByUUID(uuid);
    if (!entity) return;

    if (!entity.hasComponent<dodoe::TransformComponent>()) return;
    auto& transform = entity.getComponent<dodoe::TransformComponent>();

    const dodoe::Vector3f position = transform.getPosition();

    switch (m_mode) {
    case GizmoMode::Translate:
        GenerateTranslateGizmo(channel_data, position);
        break;
    case GizmoMode::Rotate:
        break;
    case GizmoMode::Scale:
        break;
    default:
        break;
    }
}

bool GizmoService::onMouseDown(float x, float y) {
    (void)x; (void)y;
    if (m_mode == GizmoMode::None) return false;
    return false;
}

bool GizmoService::onMouseMove(float x, float y) {
    (void)x; (void)y;
    if (!m_dragging) return false;
    return false;
}

void GizmoService::onMouseUp() {
    if (m_dragging) {
        m_ctx.commands().endMerge();
        m_dragging = false;
    }
}

} // namespace cakery
