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
        DynamicArray<UInt32> primitive_first_instance_offsets{};
        DynamicArray<InstanceSceneData> instance_scene_data{};
        DynamicArray<MeshPassRelevance> primitive_mesh_pass_relevance{};
        DynamicArray<UInt32> mesh_pass_primitive_indices[static_cast<Size_t>(8)]{};
        DynamicArray<MeshDrawCommand> mesh_pass_commands[static_cast<Size_t>(8)]{};
        DynamicArray<GBufferMeshDrawShaderData> gbuffer_shader_data{};
        Matrix4f directional_shadow_view_projection{1.0f};
        Vector4f frame_time_data{0.0f};

        void reset() override;
        void buildMeshPassPrimitiveIndices();
        [[nodiscard]] const MeshPassRelevance& getMeshPassRelevance(UInt32 primitive_index) const;
        [[nodiscard]] const DynamicArray<UInt32>& getMeshPassPrimitiveIndices(MeshPassType pass_type) const;
        [[nodiscard]] DynamicArray<MeshDrawCommand>& getMeshPassCommands(MeshPassType pass_type);
        [[nodiscard]] const DynamicArray<MeshDrawCommand>& getMeshPassCommands(MeshPassType pass_type) const;
    };

} // dodoe
