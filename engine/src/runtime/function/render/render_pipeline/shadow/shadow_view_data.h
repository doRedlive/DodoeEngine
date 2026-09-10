// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"

namespace dodoe {

    class PrimitiveSceneInfo;

    inline constexpr UInt32 kShadowCascadeCount = 4;
    inline constexpr UInt32 kShadowAtlasSize = 2048;
    inline constexpr Float kShadowCasterExtrusion = 25.0f;

    struct ShadowViewData {
        Bool has_shadow{false};
        Vector3f light_direction{0.3f, -0.8f, -0.5f};
        StaticArray<Matrix4f, kShadowCascadeCount> cascade_view_projections{
            Matrix4f(1.0f), Matrix4f(1.0f), Matrix4f(1.0f), Matrix4f(1.0f)};
        Vector4f cascade_split_depths{0.0f};
        Vector4f shadow_params{0.0025f, 0.2f, 0.0f, 2.0f};

        DynamicArray<const PrimitiveSceneInfo*> shadow_casters{};
        DynamicArray<MeshPassRelevance> shadow_caster_pass_relevance{};
        DynamicArray<UInt32> shadow_caster_primitive_indices{};
        DynamicArray<UInt32> shadow_caster_instance_offsets{};
        DynamicArray<InstanceSceneData> shadow_caster_instance_data{};
        DynamicArray<UInt8> shadow_caster_cascade_masks{};
    };

} // namespace dodoe
