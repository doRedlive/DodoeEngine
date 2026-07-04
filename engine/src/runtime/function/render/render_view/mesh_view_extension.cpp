// do@Redlive

#include "mesh_view_extension.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"

namespace dodoe {

    void MeshViewExtension::reset() {
        visible_primitives.clear();
        primitive_first_instance_offsets.clear();
        instance_scene_data.clear();
        primitive_mesh_pass_relevance.clear();

        for (auto& indices : mesh_pass_primitive_indices) {
            indices.clear();
        }
        for (auto& commands : mesh_pass_commands) {
            commands.clear();
        }

        gbuffer_shader_data.clear();
        directional_shadow_view_projection = Matrix4f(1.0f);
        frame_time_data = Vector4f(0.0f);
    }

    void MeshViewExtension::buildMeshPassPrimitiveIndices() {
        for (auto& primitive_indices : mesh_pass_primitive_indices) {
            primitive_indices.clear();
        }

        for (Size_t primitive_index = 0; primitive_index < primitive_mesh_pass_relevance.size(); primitive_index++) {
            const auto& relevance = primitive_mesh_pass_relevance[primitive_index];
            for (UInt32 pass_index = 0; pass_index < static_cast<UInt32>(MeshPassType::Count); pass_index++) {
                const auto pass_type = static_cast<MeshPassType>(pass_index);
                if (relevance.isRelevant(pass_type)) {
                    mesh_pass_primitive_indices[pass_index].push_back(static_cast<UInt32>(primitive_index));
                }
            }
        }
    }

    const MeshPassRelevance& MeshViewExtension::getMeshPassRelevance(const UInt32 primitive_index) const {
        DO_ASSERT(primitive_index < primitive_mesh_pass_relevance.size(), "MeshViewExtension primitive relevance index out of range");
        return primitive_mesh_pass_relevance[primitive_index];
    }

    const DynamicArray<UInt32>& MeshViewExtension::getMeshPassPrimitiveIndices(const MeshPassType pass_type) const {
        return mesh_pass_primitive_indices[static_cast<Size_t>(pass_type)];
    }

    DynamicArray<MeshDrawCommand>& MeshViewExtension::getMeshPassCommands(const MeshPassType pass_type) {
        return mesh_pass_commands[static_cast<Size_t>(pass_type)];
    }

    const DynamicArray<MeshDrawCommand>& MeshViewExtension::getMeshPassCommands(const MeshPassType pass_type) const {
        return mesh_pass_commands[static_cast<Size_t>(pass_type)];
    }

} // dodoe
