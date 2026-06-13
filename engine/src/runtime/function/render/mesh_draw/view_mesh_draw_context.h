// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_draw_command.h"
#include "mesh_pass_type.h"

namespace dodoe {

    class PrimitiveSceneInfo;

    struct InstanceSceneData {
        Matrix4f model{1.0f};
        Vector4f color_tint{1.0f, 1.0f, 1.0f, 1.0f};
        Vector4f params{0.0f};
    };

    struct MeshPassRelevance {
        Bool pass_relevance[static_cast<Size_t>(MeshPassType::Count)]{false};

        void setRelevant(const MeshPassType pass_type, const Bool relevant) {
            pass_relevance[static_cast<Size_t>(pass_type)] = relevant;
        }

        [[nodiscard]] Bool isRelevant(const MeshPassType pass_type) const {
            return pass_relevance[static_cast<Size_t>(pass_type)];
        }
    };

    struct GBufferMeshDrawShaderData {
        Matrix4f view_projection{1.0f};
        Vector4i draw_data{0};
        Vector4f material_data{0.0f, 1.0f, 1.0f, 0.0f};
        Vector4f time_data{0.0f};
    };

    struct ViewMeshDrawContext {
        DynamicArray<const PrimitiveSceneInfo*> visible_primitives{};
        DynamicArray<UInt32> primitive_first_instance_offsets{};
        DynamicArray<InstanceSceneData> instance_scene_data{};
        DynamicArray<MeshPassRelevance> primitive_mesh_pass_relevance{};
        DynamicArray<UInt32> mesh_pass_primitive_indices[static_cast<Size_t>(MeshPassType::Count)]{};
        DynamicArray<MeshDrawCommand> mesh_pass_commands[static_cast<Size_t>(MeshPassType::Count)]{};
        DynamicArray<GBufferMeshDrawShaderData> gbuffer_shader_data{};
        Matrix4f directional_shadow_view_projection{1.0f};
        Vector4f frame_time_data{0.0f};

        void reset() {
            visible_primitives.clear();
            primitive_first_instance_offsets.clear();
            instance_scene_data.clear();
            primitive_mesh_pass_relevance.clear();
            gbuffer_shader_data.clear();
            directional_shadow_view_projection = Matrix4f(1.0f);
            frame_time_data = Vector4f(0.0f);
            for (auto& primitive_indices : mesh_pass_primitive_indices) {
                primitive_indices.clear();
            }
            for (auto& commands : mesh_pass_commands) {
                commands.clear();
            }
        }

        void buildMeshPassPrimitiveIndices() {
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

        [[nodiscard]] const MeshPassRelevance& getMeshPassRelevance(const UInt32 primitive_index) const {
            DO_ASSERT(primitive_index < primitive_mesh_pass_relevance.size(), "ViewMeshDrawContext primitive relevance index out of range");
            return primitive_mesh_pass_relevance[primitive_index];
        }

        [[nodiscard]] const DynamicArray<UInt32>& getMeshPassPrimitiveIndices(const MeshPassType pass_type) const {
            return mesh_pass_primitive_indices[static_cast<Size_t>(pass_type)];
        }

        [[nodiscard]] DynamicArray<MeshDrawCommand>& getMeshPassCommands(const MeshPassType pass_type) {
            return mesh_pass_commands[static_cast<Size_t>(pass_type)];
        }

        [[nodiscard]] const DynamicArray<MeshDrawCommand>& getMeshPassCommands(const MeshPassType pass_type) const {
            return mesh_pass_commands[static_cast<Size_t>(pass_type)];
        }
    };

} // dodoe
