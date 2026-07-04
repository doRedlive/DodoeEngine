// do@Redlive

#include "render_view.h"
#include "render_view_family.h"
#include "mesh_view_extension.h"
#include "sprite_view_extension.h"

#include "../render_scene/render_scene.h"
#include "runtime/core/math/math.h"

namespace dodoe {
    namespace {
        Vector4f NormalizePlane(const Vector4f& plane) {
            const Vector3f normal(plane.x, plane.y, plane.z);
            const Float length = Math::Length(normal);
            if (length <= Math::Epsilon<Float>()) {
                return Vector4f(0.0f);
            }
            return Vector4f(normal / length, plane.w / length);
        }

        std::array<Vector4f, 6> ExtractFrustumPlanes(const Matrix4f& view_projection) {
            const Vector4f row0(view_projection[0][0], view_projection[1][0], view_projection[2][0], view_projection[3][0]);
            const Vector4f row1(view_projection[0][1], view_projection[1][1], view_projection[2][1], view_projection[3][1]);
            const Vector4f row2(view_projection[0][2], view_projection[1][2], view_projection[2][2], view_projection[3][2]);
            const Vector4f row3(view_projection[0][3], view_projection[1][3], view_projection[2][3], view_projection[3][3]);
            return std::array<Vector4f, 6>{
                NormalizePlane(row3 + row0),
                NormalizePlane(row3 - row0),
                NormalizePlane(row3 + row1),
                NormalizePlane(row3 - row1),
                NormalizePlane(row3 + row2),
                NormalizePlane(row3 - row2)
            };
        }

        Bool IntersectsFrustum(const std::array<Vector4f, 6>& planes, const Vector3f& center, const Vector3f& extents) {
            for (const auto& plane : planes) {
                const Vector3f normal(plane.x, plane.y, plane.z);
                const Float projected_radius =
                    std::abs(normal.x) * extents.x +
                    std::abs(normal.y) * extents.y +
                    std::abs(normal.z) * extents.z;
                const Float signed_distance = Math::Dot(normal, center) + plane.w;
                if (signed_distance + projected_radius < 0.0f) {
                    return false;
                }
            }
            return true;
        }

        Bool IsPrimitiveVisible(const PrimitiveSceneInfo& primitive, const std::array<Vector4f, 6>& frustum_planes) {
            if (!primitive.isVisible()) {
                return false;
            }
            const Vector3f local_center = (primitive.getBoundsMin() + primitive.getBoundsMax()) * 0.5f;
            const Vector3f local_extents = (primitive.getBoundsMax() - primitive.getBoundsMin()) * 0.5f;
            const Matrix4f& world_transform = primitive.getWorldTransform();
            const Vector3f world_center = Vector3f(world_transform * Vector4f(local_center, 1.0f));
            const Matrix3f linear = Matrix3f(world_transform);
            const Matrix3f abs_linear(Math::Abs(linear[0]), Math::Abs(linear[1]), Math::Abs(linear[2]));
            const Vector3f world_extents = abs_linear * local_extents;
            return IntersectsFrustum(frustum_planes, world_center, world_extents);
        }
    }

    void RenderView::setMatrices(const Matrix4f& view_matrix, const Matrix4f& projection_matrix) {
        m_view_matrix = view_matrix;
        m_projection_matrix = projection_matrix;
        m_view_projection_matrix = projection_matrix * view_matrix;
    }

    void RenderView::buildFromViewInfo(const ViewInfo& info) {
        m_view_matrix = info.view;
        m_projection_matrix = info.projection;
        m_view_projection_matrix = info.view_projection;
        m_viewport_rect = info.viewport_rect;
    }

    void RenderView::buildVisiblePrimitives(const RenderScene& scene) {
        auto& mesh_ext = getOrCreateExtension<MeshViewExtension>();
        mesh_ext.visible_primitives.clear();
        mesh_ext.visible_primitives.reserve(scene.getPrimitiveSceneInfos().size());
        const auto frustum_planes = ExtractFrustumPlanes(m_view_projection_matrix);

        DO_DEBUG("RenderView: Total primitives in scene: {}", scene.getPrimitiveSceneInfos().size());

        for (const auto& primitive : scene.getPrimitiveSceneInfos()) {
            if (IsPrimitiveVisible(primitive, frustum_planes)) {
                mesh_ext.visible_primitives.push_back(&primitive);
            }
        }

        DO_DEBUG("RenderView: Visible primitives after culling: {}", mesh_ext.visible_primitives.size());
    }

    void RenderView::buildVisibleSprites(const RenderScene& scene) {
        auto& sprite_ext = getOrCreateExtension<SpriteViewExtension>();
        sprite_ext.visible_sprites.clear();

        const auto& sprite_infos = scene.getSpriteSceneInfos();
        if (sprite_infos.empty()) {
            return;
        }

        sprite_ext.visible_sprites.reserve(sprite_infos.size());
        const auto frustum_planes = ExtractFrustumPlanes(m_view_projection_matrix);

        for (const auto& sprite_info : sprite_infos) {
            if (!sprite_info.isVisible()) {
                continue;
            }

            const Vector2f position = sprite_info.getPosition();
            const Vector2f scale = sprite_info.getScale();
            const Vector3f world_center(position.x, position.y, 0.0f);
            const Vector3f world_extents(scale.x * 0.5f, scale.y * 0.5f, 0.01f);

            if (IntersectsFrustum(frustum_planes, world_center, world_extents)) {
                sprite_ext.visible_sprites.push_back(&sprite_info);
            }
        }
    }

} // dodoe
