// do@Redlive

#pragma once

#include "dopch.h"

#include "view_extension.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/render_pipeline/shadow/shadow_system.h"

namespace dodoe {

    class PrimitiveSceneInfo;
    enum class MeshPassType : UInt8;

    class MeshViewExtension : public IViewExtension {
    public:
        DynamicArray<const PrimitiveSceneInfo*> visible_primitives{};
        DynamicArray<MeshPassRelevance> primitive_mesh_pass_relevance{};
        DynamicArray<UInt32> mesh_pass_primitive_indices[static_cast<Size_t>(MeshPassType::Count)]{};
        DynamicArray<UInt32> primitive_instance_offsets{};
        DynamicArray<InstanceSceneData> instance_scene_data{};
        Bool directional_shadow_active{false};
        StaticArray<Matrix4f, kShadowCascadeCount> directional_shadow_view_projections{
            Matrix4f(1.0f), Matrix4f(1.0f), Matrix4f(1.0f), Matrix4f(1.0f)};
        Vector4f directional_shadow_split_depths{0.0f};
        Vector4f directional_shadow_params{0.005f, 0.2f, 0.0f, 2.0f};
        Vector4f frame_time_data{0.0f};

        DynamicArray<const PrimitiveSceneInfo*> shadow_casters{};
        DynamicArray<MeshPassRelevance> shadow_caster_pass_relevance{};
        DynamicArray<UInt32> shadow_caster_primitive_indices{};
        DynamicArray<UInt32> shadow_caster_instance_offsets{};
        DynamicArray<InstanceSceneData> shadow_caster_instance_data{};
        DynamicArray<UInt8> shadow_caster_cascade_masks{};

        void reset() override;
        void buildMeshPassPrimitiveIndices();
        [[nodiscard]] const MeshPassRelevance& getMeshPassRelevance(UInt32 primitive_index) const;
        [[nodiscard]] const DynamicArray<UInt32>& getMeshPassPrimitiveIndices(MeshPassType pass_type) const;
    };

} // namespace dodoe
