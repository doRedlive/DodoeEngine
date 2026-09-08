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

    ShadowFrameData ShadowSystem::buildFrameData(const RenderView& view,
                                                 const RenderScene& scene) {
        ShadowFrameData result{};
        const auto* mesh_ext = view.getExtension<MeshViewExtension>();

        for (const auto& light : scene.getLightSceneInfos()) {
            if (light.getLightType() == LightType::Directional && light.isEnabled()) {
                result.has_shadow = true;
                result.light_direction = Math::Normalize(light.getDirectionalLightData().direction);
                break;
            }
        }

        if (!mesh_ext || mesh_ext->visible_primitives.empty()) {
            result.light_view_projection =
                rendering_pipeline_utils::BuildDirectionalLightViewProjection(result.light_direction);
            return result;
        }

        Vector3f bounds_min(0.0f);
        Vector3f bounds_max(0.0f);
        Bool bounds_valid = false;
        for (const auto* primitive : mesh_ext->visible_primitives) {
            if (!primitive || !primitive->isVisible()) {
                continue;
            }
            if (!bounds_valid) {
                bounds_min = primitive->getBoundsMin();
                bounds_max = primitive->getBoundsMax();
                bounds_valid = true;
                continue;
            }
            bounds_min = Math::Min(bounds_min, primitive->getBoundsMin());
            bounds_max = Math::Max(bounds_max, primitive->getBoundsMax());
        }

        if (!bounds_valid) {
            result.light_view_projection =
                rendering_pipeline_utils::BuildDirectionalLightViewProjection(result.light_direction);
            return result;
        }

        result.bounds_center = (bounds_min + bounds_max) * 0.5f;
        result.bounds_extent = Math::Length(
            Math::Max(bounds_max - result.bounds_center, Vector3f(0.0f)));
        result.light_view_projection =
            rendering_pipeline_utils::BuildDirectionalLightViewProjection(
                result.light_direction, result.bounds_center, result.bounds_extent * 1.2f);
        return result;
    }

    void ShadowSystem::setupView(const RenderScene& scene,
                                 RenderViewFamily& view_family) {
        for (auto& view : view_family.getViews()) {
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
            const auto data = buildFrameData(view, scene);
            mesh_ext.directional_shadow_view_projection = data.light_view_projection;
            mesh_ext.directional_shadow_params = data.shadow_params;
        }
    }

} // namespace dodoe
