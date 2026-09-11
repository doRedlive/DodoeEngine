// do@Redlive

#include "shadow_system.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/render_scene/primitive_scene_info.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_view/shadow_view_extension.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/render_view_family.h"

namespace dodoe {

    namespace {
        constexpr Float kMaxShadowDistance = 5000.0f;
        constexpr Float kCasterLateralMargin = 25.0f;
        constexpr Float kMinOrthoNear = 0.05f;
        constexpr Float kMinOrthoFarGap = 0.1f;

        void GetWorldBounds(const PrimitiveSceneInfo& primitive,
                                 Vector3f& world_center, Vector3f& world_extents) {
            const Vector3f local_center = (primitive.getBoundsMin() + primitive.getBoundsMax()) * 0.5f;
            const Vector3f local_extents = (primitive.getBoundsMax() - primitive.getBoundsMin()) * 0.5f;
            const Matrix4f& world_transform = primitive.getWorldTransform();
            world_center = Vector3f(world_transform * Vector4f(local_center, 1.0f));
            const Matrix3f linear = Matrix3f(world_transform);
            const Matrix3f abs_linear(Math::Abs(linear[0]), Math::Abs(linear[1]), Math::Abs(linear[2]));
            world_extents = abs_linear * local_extents;
        }

        void ExtractCameraNearFar(const Matrix4f& projection, Float& near_plane, Float& far_plane) {
            near_plane = 0.1f;
            far_plane = 1000.0f;
            const Float a = projection[2][2];
            const Float b = projection[3][2];
            if (Math::Abs(projection[3][3]) > 1e-5f && Math::Abs(a) > 1e-8f) {
                const Float extracted_near = (b + 1.0f) / a;
                const Float extracted_far = (b - 1.0f) / a;
                if (extracted_near > 0.0f && extracted_far > extracted_near) {
                    near_plane = extracted_near;
                    far_plane = extracted_far;
                }
                return;
            }
            if (Math::Abs(b) > 1e-8f && Math::Abs(a - 1.0f) > 1e-8f && Math::Abs(a + 1.0f) > 1e-8f) {
                const Float extracted_near = b / (a - 1.0f);
                const Float extracted_far = b / (a + 1.0f);
                if (extracted_near > 0.0f && extracted_far > extracted_near) {
                    near_plane = extracted_near;
                    far_plane = extracted_far;
                }
            }
        }

        void BuildCascadeSplitDepths(Float range_near, Float range_far, Vector4f& split_depths) {
            split_depths = Vector4f(range_far);
            if (!(range_far > range_near)) {
                return;
            }
            const Float ratio = Math::Pow(
                range_far / range_near, 1.0f / static_cast<Float>(kShadowCascadeCount));
            Float split = range_near;
            for (UInt32 cascade = 0; cascade + 1 < kShadowCascadeCount; ++cascade) {
                split *= ratio;
                split_depths[static_cast<Size_t>(cascade)] = split;
            }
        }

        Matrix4f BuildLightView(const Vector3f& light_direction) {
            const Vector3f dir = Math::Normalize(light_direction);
            Vector3f up(0.0f, 1.0f, 0.0f);
            if (Math::Abs(Math::Dot(up, dir)) > 0.99f) {
                up = Vector3f(0.0f, 0.0f, 1.0f);
            }
            return Math::LookAt(-dir, Vector3f(0.0f), up);
        }

        Float QuantizeTexelSize(const Float span, const UInt32 cascade_resolution) {
            if (!(span > 0.0f) || cascade_resolution == 0) {
                return 0.0f;
            }
            const Float texel = span / static_cast<Float>(cascade_resolution);
            const Float exponent = std::floor(std::log2(texel));
            return std::exp2(exponent);
        }

        Matrix4f BuildCascadeViewProjection(const Matrix4f& light_view,
                                            const StaticArray<Vector3f, 8>& slice_corners_world,
                                            UInt32 cascade_resolution,
                                            Bool has_casters,
                                            const Vector2f& caster_xy_min,
                                            const Vector2f& caster_xy_max,
                                            Float caster_z_min,
                                            Float caster_z_max) {
            StaticArray<Vector3f, 8> corners{};
            for (Size_t i = 0; i < 8; ++i) {
                corners[i] = Vector3f(
                    light_view * Vector4f(slice_corners_world[i], 1.0f));
            }
            Vector3f bounds_min = corners[0];
            Vector3f bounds_max = corners[0];
            for (Size_t i = 1; i < 8; ++i) {
                bounds_min = Math::Min(bounds_min, corners[i]);
                bounds_max = Math::Max(bounds_max, corners[i]);
            }

            Vector2f xy_min(bounds_min.x, bounds_min.y);
            Vector2f xy_max(bounds_max.x, bounds_max.y);
            if (has_casters) {
                const Vector2f clipped_min = Math::Max(xy_min, caster_xy_min);
                const Vector2f clipped_max = Math::Min(xy_max, caster_xy_max);
                if (clipped_min.x < clipped_max.x && clipped_min.y < clipped_max.y) {
                    xy_min = clipped_min;
                    xy_max = clipped_max;
                }
            }
            const Float span_x = Math::Max(xy_max.x - xy_min.x, 0.0f);
            const Float span_y = Math::Max(xy_max.y - xy_min.y, 0.0f);
            const Float margin_x = Math::Clamp(span_x * 0.25f, 1.0f, kCasterLateralMargin);
            const Float margin_y = Math::Clamp(span_y * 0.25f, 1.0f, kCasterLateralMargin);
            xy_min.x -= margin_x;
            xy_max.x += margin_x;
            xy_min.y -= margin_y;
            xy_max.y += margin_y;

            const Float raw_width = Math::Max(xy_max.x - xy_min.x, 0.0f);
            const Float raw_height = Math::Max(xy_max.y - xy_min.y, 0.0f);
            const Float texel_x = QuantizeTexelSize(raw_width, cascade_resolution);
            const Float texel_y = QuantizeTexelSize(raw_height, cascade_resolution);
            if (texel_x > 0.0f) {
                xy_min.x = std::floor(xy_min.x / texel_x) * texel_x;
                xy_max.x = xy_min.x + std::ceil(raw_width / texel_x) * texel_x;
            }
            if (texel_y > 0.0f) {
                xy_min.y = std::floor(xy_min.y / texel_y) * texel_y;
                xy_max.y = xy_min.y + std::ceil(raw_height / texel_y) * texel_y;
            }

            bounds_min.z -= kShadowCasterExtrusion;
            bounds_max.z += kShadowCasterExtrusion;
            if (has_casters) {
                bounds_min.z = Math::Min(bounds_min.z, caster_z_min - kShadowCasterExtrusion);
                bounds_max.z = Math::Max(bounds_max.z, caster_z_max + kShadowCasterExtrusion);
            }

            const Float z_span = Math::Max(bounds_max.z - bounds_min.z, kMinOrthoFarGap);
            const Float ortho_near = kMinOrthoNear;
            const Float ortho_far = ortho_near + z_span + kMinOrthoFarGap;
            const Matrix4f view_shift = Math::Translate(
                Matrix4f(1.0f), Vector3f(0.0f, 0.0f, -ortho_near - bounds_max.z));
            return Math::OrthoRH_ZO(xy_min.x, xy_max.x, xy_min.y, xy_max.y,
                                    ortho_near, ortho_far) * view_shift * light_view;
        }

        void CollectShadowCasters(const RenderView& view,
                                  const RenderScene& scene,
                                  const StaticArray<StaticArray<Vector4f, 6>, kShadowCascadeCount>& cascade_planes,
                                  ShadowViewData& result) {
            for (const auto& primitive : scene.getPrimitiveSceneInfos()) {
                if (!primitive.isVisible()) {
                    continue;
                }
                if (!primitive.castsShadow()) {
                    continue;
                }
                if (primitive.isEditorOnly() && !view.hasViewFlag(RenderView::kShowEditorPrimitives)) {
                    continue;
                }
                Vector3f world_center(0.0f);
                Vector3f world_extents(0.0f);
                GetWorldBounds(primitive, world_center, world_extents);
                UInt8 cascade_mask = 0;
                for (UInt32 cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
                    if (rendering_pipeline_utils::IntersectsAABBFrustum(
                            cascade_planes[cascade], world_center, world_extents)) {
                        cascade_mask |= static_cast<UInt8>(1u << cascade);
                    }
                }
                if (cascade_mask == 0) {
                    continue;
                }
                result.shadow_caster_instance_offsets.push_back(
                    static_cast<UInt32>(result.shadow_caster_instance_data.size()));
                for (const auto& inst_data : primitive.getInstanceSceneData()) {
                    result.shadow_caster_instance_data.push_back(inst_data);
                }
                MeshPassRelevance relevance{};
                relevance.setRelevant(MeshPassType::Shadow,
                                      primitive.hasRelevantBatch(MeshPassType::Shadow));
                result.shadow_caster_pass_relevance.push_back(relevance);
                result.shadow_caster_cascade_masks.push_back(cascade_mask);
                result.shadow_caster_primitive_indices.push_back(
                    static_cast<UInt32>(result.shadow_casters.size()));
                result.shadow_casters.push_back(&primitive);
            }
        }

    } // namespace

    ShadowViewData ShadowSystem::BuildFrameData(const RenderView& view,
                                                const RenderScene& scene) {
        ShadowViewData result{};

        for (const auto& light : scene.getLightSceneInfos()) {
            if (light.getLightType() == LightType::Directional && light.isEnabled() && light.castsShadow()) {
                result.has_shadow = true;
                result.light_direction = Math::Normalize(light.getDirectionalLightData().direction);
                break;
            }
        }

        const Float cascade_resolution = static_cast<Float>(kShadowAtlasSize) / 2.0f;
        Float near_plane = 0.1f;
        Float far_plane = 1000.0f;
        ExtractCameraNearFar(view.getProjectionMatrix(), near_plane, far_plane);

        const Matrix4f light_view = BuildLightView(result.light_direction);
        const Vector3f camera_position = rendering_pipeline_utils::ExtractCameraPosition(view);
        const Vector3f camera_forward = rendering_pipeline_utils::ExtractCameraDirection(view);
        Matrix4f camera_view_projection = view.getUnjitteredViewProjection();
        const Matrix4f inverse_view_projection = Math::Inverse(camera_view_projection);

        const Vector3f ndc_far_corners[4] = {
            Vector3f(-1.0f, -1.0f, 1.0f),
            Vector3f(1.0f, -1.0f, 1.0f),
            Vector3f(-1.0f, 1.0f, 1.0f),
            Vector3f(1.0f, 1.0f, 1.0f)};
        Vector3f ray_directions[4]{};
        Vector3f corner_positions[4]{};
        for (Size_t i = 0; i < 4; ++i) {
            const Vector4f corner = inverse_view_projection * Vector4f(ndc_far_corners[i], 1.0f);
            corner_positions[i] = Vector3f(corner) / corner.w;
            ray_directions[i] = Math::Normalize(corner_positions[i] - camera_position);
        }

        Bool has_casters = false;
        Float caster_z_min = 0.0f;
        Float caster_z_max = 0.0f;
        Float caster_view_min = 0.0f;
        Float caster_view_max = 0.0f;
        Vector2f caster_xy_min(0.0f);
        Vector2f caster_xy_max(0.0f);
        {
            const Vector3f light_row0(light_view[0].x, light_view[1].x, light_view[2].x);
            const Vector3f light_row1(light_view[0].y, light_view[1].y, light_view[2].y);
            const Float light_x_offset = light_view[3].x;
            const Float light_y_offset = light_view[3].y;
            const Vector3f light_row = Math::Abs(result.light_direction);
            const Vector3f forward_row = Math::Abs(camera_forward);
            for (const auto& primitive : scene.getPrimitiveSceneInfos()) {
                if (!primitive.isVisible() || !primitive.castsShadow()) {
                    continue;
                }
                Vector3f world_center(0.0f);
                Vector3f world_extents(0.0f);
                GetWorldBounds(primitive, world_center, world_extents);
                const Float z_center = -Math::Dot(result.light_direction, world_center) - 1.0f;
                const Float z_half = Math::Dot(light_row, world_extents);
                const Float z_lo = z_center - z_half;
                const Float z_hi = z_center + z_half;
                const Float view_center = Math::Dot(world_center - camera_position, camera_forward);
                const Float view_half = Math::Dot(forward_row, world_extents);
                const Float view_lo = view_center - view_half;
                const Float view_hi = view_center + view_half;
                const Float x_center = Math::Dot(light_row0, world_center) + light_x_offset;
                const Float x_half = Math::Dot(Math::Abs(light_row0), world_extents);
                const Float y_center = Math::Dot(light_row1, world_center) + light_y_offset;
                const Float y_half = Math::Dot(Math::Abs(light_row1), world_extents);
                if (!has_casters) {
                    has_casters = true;
                    caster_z_min = z_lo;
                    caster_z_max = z_hi;
                    caster_view_min = view_lo;
                    caster_view_max = view_hi;
                    caster_xy_min = Vector2f(x_center - x_half, y_center - y_half);
                    caster_xy_max = Vector2f(x_center + x_half, y_center + y_half);
                } else {
                    caster_z_min = Math::Min(caster_z_min, z_lo);
                    caster_z_max = Math::Max(caster_z_max, z_hi);
                    caster_view_min = Math::Min(caster_view_min, view_lo);
                    caster_view_max = Math::Max(caster_view_max, view_hi);
                    caster_xy_min = Math::Min(caster_xy_min, Vector2f(x_center - x_half, y_center - y_half));
                    caster_xy_max = Math::Max(caster_xy_max, Vector2f(x_center + x_half, y_center + y_half));
                }
            }
        }

        Float range_near = near_plane;
        Float range_far = Math::Min(far_plane, kMaxShadowDistance);
        if (has_casters) {
            const Float pad = 0.1f * (caster_view_max - caster_view_min);
            range_near = Math::Max(range_near, caster_view_min - pad);
            range_far = Math::Clamp(caster_view_max + pad,
                                    range_near + kMinOrthoFarGap, range_far);
        }
        BuildCascadeSplitDepths(range_near, range_far, result.cascade_split_depths);

        StaticArray<Vector3f, 8> slice_corners{};
        for (UInt32 cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
            const Float split_near = cascade == 0
                ? range_near
                : result.cascade_split_depths[static_cast<Size_t>(cascade - 1)];
            const Float split_far = Math::Max(
                result.cascade_split_depths[static_cast<Size_t>(cascade)],
                split_near + kMinOrthoFarGap);
            for (Size_t i = 0; i < 4; ++i) {
                const Float ray_cosine = Math::Max(
                    Math::Dot(ray_directions[i], camera_forward), 1e-4f);
                slice_corners[i] = camera_position +
                    ray_directions[i] * (split_near / ray_cosine);
                slice_corners[i + 4] = camera_position +
                    ray_directions[i] * (split_far / ray_cosine);
            }
            result.cascade_view_projections[cascade] = BuildCascadeViewProjection(
                light_view, slice_corners,
                static_cast<UInt32>(cascade_resolution),
                has_casters, caster_xy_min, caster_xy_max, caster_z_min, caster_z_max);
        }

        StaticArray<StaticArray<Vector4f, 6>, kShadowCascadeCount> cascade_planes{};
        for (UInt32 cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
            const auto receiver_planes = rendering_pipeline_utils::ExtractViewFrustumPlanesZO(
                result.cascade_view_projections[cascade]);
            cascade_planes[cascade] = rendering_pipeline_utils::ExpandFrustumDepthPlanes(
                receiver_planes, kShadowCasterExtrusion);
        }
        CollectShadowCasters(view, scene, cascade_planes, result);
        return result;
    }

    void ShadowSystem::SetupView(const RenderScene& scene,
                                 RenderViewFamily& view_family) {
        for (auto& view : view_family.getViews()) {
            auto& shadow_ext = view.getOrCreateExtension<ShadowViewExtension>();
            auto data = BuildFrameData(view, scene);
            shadow_ext.getData() = std::move(data);
        }
    }

} // namespace dodoe
