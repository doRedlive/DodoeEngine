// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_draw_command.h"
#include "mesh_draw_types.h"

namespace dodoe {

    struct MeshDrawList {
        DynamicArray<MeshDrawInstance> cached_instances;
        DynamicArray<MeshDrawInstance> dynamic_instances;
        DynamicArray<MeshDrawCommand> frame_commands;
        DynamicArray<PrimitiveMeshDrawShaderData> cached_shader_data;
        DynamicArray<PrimitiveMeshDrawShaderData> dynamic_shader_data;
        const DynamicArray<MeshDrawCommand>* cached_commands{nullptr};

        void reset() {
            cached_instances.clear();
            dynamic_instances.clear();
            frame_commands.clear();
            cached_shader_data.clear();
            dynamic_shader_data.clear();
            cached_commands = nullptr;
        }
    };

} // namespace dodoe
