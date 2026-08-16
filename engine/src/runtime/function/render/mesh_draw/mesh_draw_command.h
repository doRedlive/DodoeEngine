// do@Redlive

#pragma once

#include "dopch.h"
#include "mesh_pass_type.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/shader/shader_parameter.h"

namespace dodoe {

    struct MaterialInstance;

    struct MeshDrawCommandCacheKey {
        Size_t batch_hash{0};
        Size_t material_hash{0};
        Size_t pass_hash{0};

        Bool operator==(const MeshDrawCommandCacheKey& other) const {
            return batch_hash == other.batch_hash &&
                   material_hash == other.material_hash &&
                   pass_hash == other.pass_hash;
        }
    };

    class MeshDrawCommand {
        MeshPassType m_pass_type{MeshPassType::GBuffer};
        GfxGraphicsPipelineHandle m_pipeline{};
        StaticArray<GfxBindingSetHandle, ShaderParameterBinder::kShaderParameterSetCount> m_binding_sets{};
        DynamicArray<GfxVertexBufferBinding> m_vertex_bindings{};
        GfxIndexBufferBinding m_index_binding{};
        GfxDrawArguments m_draw_args{};
        MaterialInstance* m_material_instance{nullptr};

    public:
        [[nodiscard]] MeshPassType getPassType() const { return m_pass_type; }
        [[nodiscard]] const GfxGraphicsPipelineHandle& getPipeline() const { return m_pipeline; }
        [[nodiscard]] const StaticArray<GfxBindingSetHandle, ShaderParameterBinder::kShaderParameterSetCount>& getBindingSets() const { return m_binding_sets; }
        [[nodiscard]] const DynamicArray<GfxVertexBufferBinding>& getVertexBindings() const { return m_vertex_bindings; }
        [[nodiscard]] const GfxIndexBufferBinding& getIndexBinding() const { return m_index_binding; }
        [[nodiscard]] const GfxDrawArguments& getDrawArguments() const { return m_draw_args; }
        [[nodiscard]] MaterialInstance* getMaterialInstance() const { return m_material_instance; }

        void setPassType(const MeshPassType pass_type) { m_pass_type = pass_type; }
        void setPipeline(GfxGraphicsPipelineHandle pipeline) { m_pipeline = std::move(pipeline); }
        void setBindingSet(const ShaderParameterSet set, const GfxBindingSetHandle& binding_set) {
            m_binding_sets[static_cast<Size_t>(set)] = binding_set;
        }
        void addVertexBinding(const GfxVertexBufferBinding& binding) { m_vertex_bindings.push_back(binding); }
        void setIndexBinding(const GfxIndexBufferBinding& binding) { m_index_binding = binding; }
        void setDrawArguments(const GfxDrawArguments& args) { m_draw_args = args; }
        void setMaterialInstance(MaterialInstance* mi) { m_material_instance = mi; }
    };

    struct MeshDrawInstance {
        UInt32 cmd_index{0};
        UInt32 shader_data_index{std::numeric_limits<UInt32>::max()};
        UInt64 instance_offset{0};

        [[nodiscard]] Bool hasShaderData() const {
            return shader_data_index != std::numeric_limits<UInt32>::max();
        }
    };

} // dodoe

template <>
struct std::hash<dodoe::MeshDrawCommandCacheKey> {
    dodoe::Size_t operator()(const dodoe::MeshDrawCommandCacheKey& k) const {
        return k.batch_hash ^ (k.material_hash << 1) ^ (k.pass_hash << 3);
    }
};
