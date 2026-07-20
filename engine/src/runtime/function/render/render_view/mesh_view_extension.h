// do@Redlive

#pragma once

#include "dopch.h"

#include "view_extension.h"
#include "../mesh_draw/mesh_draw_types.h"
#include "../mesh_draw/mesh_draw_command.h"

namespace dodoe {

    class PrimitiveSceneInfo;
    enum class MeshPassType : UInt8;

    class MeshViewExtension : public IViewExtension {
    public:
        DynamicArray<const PrimitiveSceneInfo*> visible_primitives{};
        DynamicArray<MeshPassRelevance> primitive_mesh_pass_relevance{};
        DynamicArray<UInt32> mesh_pass_primitive_indices[static_cast<Size_t>(MeshPassType::Count)]{};
        DynamicArray<InstanceSceneData> instance_scene_data{};
        DynamicArray<MeshDrawInstance> cached_draw_instances[static_cast<Size_t>(MeshPassType::Count)]{};
        DynamicArray<MeshDrawInstance> dynamic_draw_instances[static_cast<Size_t>(MeshPassType::Count)]{};
        DynamicArray<MeshDrawCommand> frame_commands{};
        DynamicArray<GBufferMeshDrawShaderData> gbuffer_shader_data{};
        DynamicArray<GBufferMeshDrawShaderData> dynamic_shader_data{};
        Matrix4f directional_shadow_view_projection{1.0f};
        Vector4f frame_time_data{0.0f};
        const DynamicArray<MeshDrawCommand>* cached_commands{nullptr};

        void reset() override;
        void buildMeshPassPrimitiveIndices();
        [[nodiscard]] const MeshPassRelevance& getMeshPassRelevance(UInt32 primitive_index) const;
        [[nodiscard]] const DynamicArray<UInt32>& getMeshPassPrimitiveIndices(MeshPassType pass_type) const;
    };

} // dodoe
