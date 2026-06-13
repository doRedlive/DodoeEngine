// do@Redlive

#pragma once

#include "dopch.h"

#include "render_scene.h"
#include "render_view_family.h"

namespace dodoe::scene_visibility {

    inline Vector4f NormalizePlane(const Vector4f& plane) {
        const Vector3f normal(plane.x, plane.y, plane.z);
        const Float length = Math::Length(normal);
        if (length <= Math::Epsilon<Float>()) {
            return Vector4f(0.0f);
        }
        return Vector4f(normal / length, plane.w / length);
    }

    inline std::array<Vector4f, 6> ExtractFrustumPlanes(const Matrix4f& view_projection) {
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

    inline Bool IntersectsFrustum(const std::array<Vector4f, 6>& planes, const Vector3f& center, const Vector3f& extents) {
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

    inline Bool IsPrimitiveVisible(const PrimitiveSceneInfo& primitive, const std::array<Vector4f, 6>& frustum_planes) {
        if (!primitive.isVisible()) {
            return false;
        }
        const Vector3f local_center = (primitive.getBoundsMin() + primitive.getBoundsMax()) * 0.5f;
        const Vector3f local_extents = (primitive.getBoundsMax() - primitive.getBoundsMin()) * 0.5f;
        const Matrix4f& world_transform = primitive.getWorldTransform();
        const Vector3f world_center = Vector3f(world_transform * Vector4f(local_center, 1.0f));
        const Matrix3f linear = Matrix3f(world_transform);
        const Matrix3f abs_linear(glm::abs(linear[0]), glm::abs(linear[1]), glm::abs(linear[2]));
        const Vector3f world_extents = abs_linear * local_extents;
        return IntersectsFrustum(frustum_planes, world_center, world_extents);
    }

    inline void BuildVisiblePrimitiveSet(const RenderScene& scene, RenderView& view) {
        auto& visible_primitives = view.getMeshDrawContext().visible_primitives;
        visible_primitives.clear();
        visible_primitives.reserve(scene.getPrimitives().size());
        const auto frustum_planes = ExtractFrustumPlanes(view.getViewProjectionMatrix());

        for (const auto& primitive : scene.getPrimitives()) {
            if (IsPrimitiveVisible(primitive, frustum_planes)) {
                visible_primitives.push_back(&primitive);
            }
        }
    }

    inline void BuildVisiblePrimitiveSets(RenderScene const& scene, RenderViewFamily& view_family) {
        for (auto& view : view_family.getViews()) {
            BuildVisiblePrimitiveSet(scene, view);
        }
    }

} // namespace dodoe::scene_visibility
