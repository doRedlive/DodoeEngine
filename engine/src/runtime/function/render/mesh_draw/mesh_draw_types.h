// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_pass_type.h"

namespace dodoe {

    struct InstanceSceneData {
        Matrix4f model{1.0f};
        Vector4f color_tint{1.0f, 1.0f, 1.0f, 1.0f};
        Vector4f params{0.0f};
    };

    struct MeshPassRelevance {
        Bool pass_relevance[static_cast<Size_t>(MeshPassType::Count)]{false};

        void setRelevant(MeshPassType pass_type, Bool relevant);
        [[nodiscard]] Bool isRelevant(MeshPassType pass_type) const;
    };

    struct GBufferMeshDrawShaderData {
        Matrix4f view_projection{1.0f};
        Vector4i draw_data{0};
        Vector4f material_data{0.0f, 1.0f, 1.0f, 0.0f};
        Vector4f time_data{0.0f};
    };

} // dodoe
