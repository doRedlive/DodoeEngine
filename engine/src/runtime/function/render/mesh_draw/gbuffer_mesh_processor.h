// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_processor_base.h"
#include "../framework/descriptor_table_manager.h"
#include "../framework/texture_manager.h"
#include "../render_scene/primitive_scene_info.h"

namespace dodoe {

    class RenderView;
    class PrimitiveSceneInfo;
    struct InstanceSceneData;
    struct MeshPassRelevance;
    struct MeshDrawCommand;
    struct GBufferMeshDrawShaderData;

    class GBufferMeshProcessor final : public IMeshPassProcessor {
        DescriptorTableManager* m_descriptor_table{nullptr};
        TextureManager* m_texture_manager{nullptr};
        GfxSamplerHandle m_sampler{};
        GfxBindingLayoutHandle m_binding_layout{};
        GfxBindingSetHandle m_binding_set{};
        GfxBufferHandle m_constant_buffer{};

    public:
        GBufferMeshProcessor() = default;
        GBufferMeshProcessor(DescriptorTableManager* descriptor_table, TextureManager* texture_manager)
            : m_descriptor_table(descriptor_table), m_texture_manager(texture_manager) { }

        void initialize(GfxContext& gfx_context, DescriptorTableManager* descriptor_table, TextureManager* texture_manager);
        void reset() override;

        void setDescriptorTable(DescriptorTableManager* descriptor_table) { m_descriptor_table = descriptor_table; }
        void setTextureManager(TextureManager* texture_manager) { m_texture_manager = texture_manager; }
        [[nodiscard]] const GfxBindingLayoutHandle& getBindingLayout() const override { return m_binding_layout; }
        [[nodiscard]] const GfxBindingSetHandle& getBindingSet() const { return m_binding_set; }
        [[nodiscard]] const GfxBufferHandle& getConstantBuffer() const override { return m_constant_buffer; }

        void buildCommands(
            const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
            const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
            const DynamicArray<UInt32>& mesh_pass_primitive_indices,
            const Matrix4f& view_projection,
            DynamicArray<MeshDrawCommand>& out_commands,
            DynamicArray<GBufferMeshDrawShaderData>& out_shader_data) const;
    };

} // dodoe
