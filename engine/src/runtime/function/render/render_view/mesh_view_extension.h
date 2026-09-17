// do@Redlive

#pragma once

#include "dopch.h"

#include "view_extension.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/render_graph/render_graph_resource.h"

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
        Vector4f frame_time_data{0.0f};

        RenderGraphTextureHandle opaque_shadow_map{};
        RenderGraphBufferHandle opaque_instance_buffer{};
        RenderGraphBufferHandle shadow_caster_instance_buffer{};
        RenderGraphTextureHandle transparent_override_color{};

        void reset() override;
        void buildMeshPassPrimitiveIndices();
        [[nodiscard]] const MeshPassRelevance& getMeshPassRelevance(UInt32 primitive_index) const;
        [[nodiscard]] const DynamicArray<UInt32>& getMeshPassPrimitiveIndices(MeshPassType pass_type) const;
    };

} // namespace dodoe
