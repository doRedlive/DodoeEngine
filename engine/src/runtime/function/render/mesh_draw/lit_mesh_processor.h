// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_processor_base.h"
#include "cached_mesh_draw_command.h"

#include "../render_scene/primitive_scene_info.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class PrimitiveSceneInfo;
    class BindingLayoutCache;
    class BindingSetCache;
    class MaterialSystem;
    struct InstanceSceneData;
    struct MeshPassRelevance;

    class LitMeshProcessor final : public MeshPassProcessor {
        GfxBindingSetHandle m_descriptor_binding_set{};
        MaterialSystem* m_material_system{nullptr};
        GfxSamplerHandle m_sampler{};
        GfxBindingLayoutHandle m_global_binding_layout{};
        GfxBindingLayoutHandle m_view_binding_layout{};
        GfxBindingLayoutHandle m_primitive_binding_layout{};
        GfxBindingLayoutHandle m_sampler_binding_layout{};
        GfxBindingSetHandle m_global_binding_set{};
        GfxBindingSetHandle m_view_binding_set{};
        GfxBindingSetHandle m_primitive_binding_set{};
        GfxBindingSetHandle m_sampler_binding_set{};
        GfxBufferHandle m_global_constant_buffer{};
        GfxBufferHandle m_view_constant_buffer{};
        GfxBufferHandle m_primitive_constant_buffer{};

    public:
        LitMeshProcessor(const MeshPassType pass_type,
                         GfxBindingSetHandle descriptor_binding_set,
                         BindingLayoutCache& binding_layout_cache,
                         BindingSetCache& binding_set_cache,
                         MaterialSystem& material_system);
        void reset() override;
        [[nodiscard]] GfxGraphicsPipelineDesc buildPipelineDescription(
            const MeshPassPipelineContext& context) const override;

        [[nodiscard]] const GfxBindingLayoutHandle& getGlobalBindingLayout() const override { return m_global_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getViewBindingLayout() const override { return m_view_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getPrimitiveBindingLayout() const override { return m_primitive_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getSamplerBindingLayout() const override { return m_sampler_binding_layout; }
        [[nodiscard]] const GfxBindingSetHandle& getGlobalBindingSet() const { return m_global_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getViewBindingSet() const { return m_view_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getPrimitiveBindingSet() const { return m_primitive_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getSamplerBindingSet() const { return m_sampler_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getDescriptorBindingSet() const { return m_descriptor_binding_set; }
        [[nodiscard]] const GfxBufferHandle& getGlobalConstantBuffer() const override { return m_global_constant_buffer; }
        [[nodiscard]] const GfxBufferHandle& getViewConstantBuffer() const override { return m_view_constant_buffer; }
        [[nodiscard]] const GfxBufferHandle& getPrimitiveConstantBuffer() const override { return m_primitive_constant_buffer; }

    protected:
        [[nodiscard]] Bool shouldDrawPrimitive(const PrimitiveSceneInfo& primitive) const override;
        [[nodiscard]] Bool setupMeshDrawCommand(
            const MeshBatch& batch,
            const MeshBatchElement& element,
            MeshDrawCommand& command,
            PrimitiveMeshDrawShaderData& shader_data) const override;
    };

} // namespace dodoe
