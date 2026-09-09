// do@Redlive

#include "shadow_system.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/render_scene/primitive_scene_info.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/render_view_family.h"

namespace dodoe {

    namespace {
        constexpr Float kCascadeSplitLambda = 0.75f;
        constexpr Float kMaxShadowDistance = 200.0f;
        constexpr Float kCascadeFitMargin = 0.05f;
        constexpr Float kMinOrthoNear = 0.05f;
        constexpr Float kMinOrthoFarGap = 0.1f;

        struct SceneBounds {
            Vector3f bounds_min{0.0f};
            Vector3f bounds_max{0.0f};
            Bool valid{false};

            void expand(const Vector3f& world_center, const Vector3f& world_extents) {
                const Vector3f lo = world_center - world_extents;
                const Vector3f hi = world_center + world_extents;
                if (!valid) {
                    bounds_min = lo;
                    bounds_max = hi;
                    valid = true;
                    return;
                }
                bounds_min = Math::Min(bounds_min, lo);
                bounds_max = Math::Max(bounds_max, hi);
            }
        };

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
            if (Math::Abs(b) > 1e-8f && Math::Abs(a - 1.0f) > 1e-8f && Math::Abs(a + 1.0f) > 1e-8f) {
                const Float extracted_near = b / (a - 1.0f);
                const Float extracted_far = b / (a + 1.0f);
                if (extracted_near > 0.0f && extracted_far > extracted_near) {
                    near_plane = extracted_near;
                    far_plane = extracted_far;
                }
            }
        }

        void BuildCascadeSplitDepths(Float near_plane, Float far_plane, Vector4f& split_depths) {
            const Float max_distance = Math::Min(far_plane, kMaxShadowDistance);
            if (!(max_distance > near_plane)) {
                split_depths = Vector4f(max_distance);
                return;
            }
            for (UInt32 cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
                const Float fraction =
                    static_cast<Float>(cascade + 1) / static_cast<Float>(kShadowCascadeCount);
                const Float log_split = near_plane * Math::Pow(max_distance / near_plane, fraction);
                const Float uniform_split = near_plane + (max_distance - near_plane) * fraction;
                split_depths[static_cast<Size_t>(cascade)] =
                    Math::Mix(uniform_split, log_split, kCascadeSplitLambda);
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

        void CollectSceneBounds(const RenderScene& scene, const Matrix4f& light_view,
                                const Vector3f& frustum_center,
                                Vector3f& scene_min, Vector3f& scene_max, Bool& scene_valid) {
            SceneBounds world_bounds{};
            for (const auto& primitive : scene.getPrimitiveSceneInfos()) {
                if (!primitive.isVisible()) {
                    continue;
                }
                Vector3f world_center(0.0f);
                Vector3f world_extents(0.0f);
                GetWorldBounds(primitive, world_center, world_extents);
                world_bounds.expand(world_center, world_extents);
            }
            scene_valid = false;
            if (!world_bounds.valid) {
                return;
            }
            const Vector3f offsets[8] = {
                Vector3f(world_bounds.bounds_min.x, world_bounds.bounds_min.y, world_bounds.bounds_min.z),
                Vector3f(world_bounds.bounds_max.x, world_bounds.bounds_min.y, world_bounds.bounds_min.z),
                Vector3f(world_bounds.bounds_min.x, world_bounds.bounds_max.y, world_bounds.bounds_min.z),
                Vector3f(world_bounds.bounds_max.x, world_bounds.bounds_max.y, world_bounds.bounds_min.z),
                Vector3f(world_bounds.bounds_min.x, world_bounds.bounds_min.y, world_bounds.bounds_max.z),
                Vector3f(world_bounds.bounds_max.x, world_bounds.bounds_min.y, world_bounds.bounds_max.z),
                Vector3f(world_bounds.bounds_min.x, world_bounds.bounds_max.y, world_bounds.bounds_max.z),
                Vector3f(world_bounds.bounds_max.x, world_bounds.bounds_max.y, world_bounds.bounds_max.z)};
            scene_min = Vector3f(light_view * Vector4f(offsets[0] - frustum_center, 1.0f));
            scene_max = scene_min;
            for (Size_t i = 1; i < 8; ++i) {
                const Vector3f corner = Vector3f(light_view * Vector4f(offsets[i] - frustum_center, 1.0f));
                scene_min = Math::Min(scene_min, corner);
                scene_max = Math::Max(scene_max, corner);
            }
            scene_valid = true;
        }

        Matrix4f BuildCascadeViewProjection(const Matrix4f& light_view,
                                            const StaticArray<Vector3f, 8>& slice_corners_world,
                                            const Vector3f& frustum_center,
                                            const Vector3f& scene_min, const Vector3f& scene_max,
                                            Bool scene_valid,
                                            UInt32 cascade_resolution) {
            StaticArray<Vector3f, 8> corners{};
            for (Size_t i = 0; i < 8; ++i) {
                corners[i] = Vector3f(
                    light_view * Vector4f(slice_corners_world[i] - frustum_center, 1.0f));
            }
            Vector3f bounds_min = corners[0];
            Vector3f bounds_max = corners[0];
            for (Size_t i = 1; i < 8; ++i) {
                bounds_min = Math::Min(bounds_min, corners[i]);
                bounds_max = Math::Max(bounds_max, corners[i]);
            }
            if (scene_valid) {
                const Vector3f xy_min = Vector3f(
                    Math::Max(bounds_min.x, scene_min.x), Math::Max(bounds_min.y, scene_min.y), bounds_min.z);
                const Vector3f xy_max = Vector3f(
                    Math::Min(bounds_max.x, scene_max.x), Math::Min(bounds_max.y, scene_max.y), bounds_max.z);
                if (xy_max.x > xy_min.x && xy_max.y > xy_min.y) {
                    bounds_min = xy_min;
                    bounds_max = xy_max;
                }
                bounds_min.z = Math::Min(bounds_min.z, scene_min.z);
                bounds_max.z = Math::Max(bounds_max.z, scene_max.z);
            }
            bounds_min -= Vector3f(kCascadeFitMargin);
            bounds_max += Vector3f(kCascadeFitMargin);

            const Float texel_x = (bounds_max.x - bounds_min.x) / static_cast<Float>(cascade_resolution);
            const Float texel_y = (bounds_max.y - bounds_min.y) / static_cast<Float>(cascade_resolution);
            if (texel_x > 0.0f) {
                bounds_min.x = std::floor(bounds_min.x / texel_x) * texel_x;
                bounds_max.x = std::ceil(bounds_max.x / texel_x) * texel_x;
            }
            if (texel_y > 0.0f) {
                bounds_min.y = std::floor(bounds_min.y / texel_y) * texel_y;
                bounds_max.y = std::ceil(bounds_max.y / texel_y) * texel_y;
            }

            const Float z_span = Math::Max(bounds_max.z - bounds_min.z, kMinOrthoFarGap);
            const Float ortho_near = kMinOrthoNear;
            const Float ortho_far = ortho_near + z_span + kMinOrthoFarGap;
            const Matrix4f view_shift = Math::Translate(
                Matrix4f(1.0f), Vector3f(0.0f, 0.0f, -ortho_near - bounds_max.z));
            return Math::OrthoRH_ZO(bounds_min.x, bounds_max.x, bounds_min.y, bounds_max.y,
                                    ortho_near, ortho_far) * view_shift * light_view *
                Math::Translate(Matrix4f(1.0f), -frustum_center);
        }

        void CollectShadowCasters(const RenderView& view,
                                  const RenderScene& scene,
                                  const StaticArray<StaticArray<Vector4f, 6>, kShadowCascadeCount>& cascade_planes,
                                  ShadowFrameData& result) {
            UInt32 stats_total = 0;
            UInt32 stats_visible = 0;
            UInt32 stats_castable = 0;
            UInt32 stats_masked = 0;
            static UInt32 detail_log_counter = 0;
            const bool log_detail = (detail_log_counter++ % 120u) == 0u;
            for (const auto& primitive : scene.getPrimitiveSceneInfos()) {
                ++stats_total;
                if (!primitive.isVisible()) {
                    continue;
                }
                ++stats_visible;
                if (!primitive.castsShadow()) {
                    continue;
                }
                ++stats_castable;
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
                if (log_detail) {
                    DO_WARN("ShadowSystem: prim c=({:.1f},{:.1f},{:.1f}) e=({:.1f},{:.1f},{:.1f}) mask={:x}",
                        world_center.x, world_center.y, world_center.z,
                        world_extents.x, world_extents.y, world_extents.z,
                        static_cast<UInt32>(cascade_mask));
                }
                if (cascade_mask == 0) {
                    continue;
                }
                ++stats_masked;
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
            static UInt32 stats_log_counter = 0;
            if ((stats_log_counter++ % 120u) == 0u) {
                DO_WARN("ShadowSystem: prims={} visible={} castable={} in_cascade={} instances={}",
                    stats_total, stats_visible, stats_castable, stats_masked,
                    result.shadow_caster_instance_data.size());
            }
        }

    } // namespace

    ShadowFrameData ShadowSystem::buildFrameData(const RenderView& view,
                                                 const RenderScene& scene) {
        ShadowFrameData result{};

        for (const auto& light : scene.getLightSceneInfos()) {
            if (light.getLightType() == LightType::Directional && light.isEnabled()) {
                result.has_shadow = true;
                result.light_direction = Math::Normalize(light.getDirectionalLightData().direction);
                break;
            }
        }

        const Float cascade_resolution = static_cast<Float>(kShadowAtlasSize) / 2.0f;
        Float near_plane = 0.1f;
        Float far_plane = 1000.0f;
        ExtractCameraNearFar(view.getProjectionMatrix(), near_plane, far_plane);
        BuildCascadeSplitDepths(near_plane, far_plane, result.cascade_split_depths);

        const Matrix4f light_view = BuildLightView(result.light_direction);
        const Vector3f camera_position = rendering_pipeline_utils::ExtractCameraPosition(view);
        const Vector3f camera_forward = rendering_pipeline_utils::ExtractCameraDirection(view);
        const Matrix4f inverse_view_projection = Math::Inverse(view.getViewProjectionMatrix());

        const Vector3f ndc_far_corners[4] = {
            Vector3f(-1.0f, -1.0f, 1.0f),
            Vector3f(1.0f, -1.0f, 1.0f),
            Vector3f(-1.0f, 1.0f, 1.0f),
            Vector3f(1.0f, 1.0f, 1.0f)};
        Vector3f ray_directions[4]{};
        for (Size_t i = 0; i < 4; ++i) {
            const Vector4f corner = inverse_view_projection * Vector4f(ndc_far_corners[i], 1.0f);
            ray_directions[i] = Math::Normalize(Vector3f(corner) / corner.w - camera_position);
        }

        StaticArray<Vector3f, 8> slice_corners{};
        for (UInt32 cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
            const Float split_near = cascade == 0
                ? near_plane
                : result.cascade_split_depths[static_cast<Size_t>(cascade - 1)];
            const Float split_far = Math::Max(
                result.cascade_split_depths[static_cast<Size_t>(cascade)],
                split_near + kMinOrthoFarGap);
            for (Size_t i = 0; i < 4; ++i) {
                slice_corners[i] = camera_position + ray_directions[i] * split_near;
                slice_corners[i + 4] = camera_position + ray_directions[i] * split_far;
            }
            Vector3f frustum_center(0.0f);
            for (const auto& corner : slice_corners) {
                frustum_center += corner;
            }
            frustum_center *= (1.0f / static_cast<Float>(slice_corners.size()));

            Vector3f scene_min(0.0f);
            Vector3f scene_max(0.0f);
            Bool scene_valid = false;
            CollectSceneBounds(scene, light_view, frustum_center, scene_min, scene_max, scene_valid);

            result.cascade_view_projections[cascade] = BuildCascadeViewProjection(
                light_view, slice_corners, frustum_center, scene_min, scene_max, scene_valid,
                static_cast<UInt32>(cascade_resolution));
        }

        StaticArray<StaticArray<Vector4f, 6>, kShadowCascadeCount> cascade_planes{};
        for (UInt32 cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
            cascade_planes[cascade] = rendering_pipeline_utils::ExtractViewFrustumPlanes(
                result.cascade_view_projections[cascade]);
        }
        CollectShadowCasters(view, scene, cascade_planes, result);

        static UInt32 diag_log_counter = 0;
        if ((diag_log_counter++ % 120u) == 0u) {
            const Vector4f& splits = result.cascade_split_depths;
            DO_WARN("ShadowSystem: splits=({:.2f},{:.2f},{:.2f},{:.2f}) dir=({:.2f},{:.2f},{:.2f})",
                splits.x, splits.y, splits.z, splits.w,
                result.light_direction.x, result.light_direction.y, result.light_direction.z);
            DO_WARN("ShadowSystem: cam=({:.2f},{:.2f},{:.2f})",
                camera_position.x, camera_position.y, camera_position.z);
            for (UInt32 cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
                const auto& vp = result.cascade_view_projections[cascade];
                const Float width = 2.0f / Math::Abs(vp[0][0]);
                const Float height = 2.0f / Math::Abs(vp[1][1]);
                const Float z_near = -vp[3][2] / vp[2][2];
                const Float z_far = (1.0f - vp[3][2]) / vp[2][2];
                DO_WARN("ShadowSystem: cascade{} {:.1f}x{:.1f} z=[{:.2f},{:.2f}] t=({:.1f},{:.1f},{:.1f})",
                    cascade, width, height, z_near, z_far,
                    vp[3][0], vp[3][1], vp[3][2]);
            }
        }
        return result;
    }

    void ShadowSystem::setupView(const RenderScene& scene,
                                 RenderViewFamily& view_family) {
        for (auto& view : view_family.getViews()) {
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
            auto data = buildFrameData(view, scene);
            mesh_ext.directional_shadow_active = data.has_shadow;
            mesh_ext.directional_shadow_view_projections = data.cascade_view_projections;
            mesh_ext.directional_shadow_split_depths = data.cascade_split_depths;
            mesh_ext.directional_shadow_params = data.shadow_params;
            mesh_ext.shadow_casters = std::move(data.shadow_casters);
            mesh_ext.shadow_caster_pass_relevance = std::move(data.shadow_caster_pass_relevance);
            mesh_ext.shadow_caster_primitive_indices = std::move(data.shadow_caster_primitive_indices);
            mesh_ext.shadow_caster_instance_offsets = std::move(data.shadow_caster_instance_offsets);
            mesh_ext.shadow_caster_instance_data = std::move(data.shadow_caster_instance_data);
            mesh_ext.shadow_caster_cascade_masks = std::move(data.shadow_caster_cascade_masks);
        }
    }

} // namespace dodoe
