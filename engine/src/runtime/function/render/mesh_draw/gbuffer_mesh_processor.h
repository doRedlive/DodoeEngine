// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_processor_base.h"
#include "cached_mesh_draw_command.h"

#include "../render_scene/primitive_scene_info.h"
#include "../render_service/shared_render_service.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class PrimitiveSceneInfo;
    class BindingLayoutCache;
    class BindingSetCache;
    struct InstanceSceneData;
    struct MeshPassRelevance;

    class GBufferMeshProcessor final : public IMeshPassProcessor {
        GfxBindingSetHandle m_descriptor_binding_set{};
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
        SharedRenderService* m_shared_render_service{nullptr};
        GfxInputLayoutHandle m_gbuffer_input_layout{};
        GfxFramebufferInfo m_gbuffer_framebuffer_info{};

        [[nodiscard]] GfxGraphicsPipelineHandle resolvePipelineFor(
            const MaterialInstance* material_instance,
            DrawCommandList& cmd_list) const;

    public:
        explicit GBufferMeshProcessor(SharedRenderService* shared_render_service);
        void reset() override;

        void updateFrameData(GfxInputLayoutHandle gbuffer_input_layout,
                             GfxFramebufferInfo gbuffer_framebuffer_info);

        [[nodiscard]] const GfxBindingLayoutHandle& getGlobalBindingLayout() const { return m_global_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getViewBindingLayout() const { return m_view_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getPrimitiveBindingLayout() const { return m_primitive_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getSamplerBindingLayout() const { return m_sampler_binding_layout; }
        [[nodiscard]] const GfxBindingSetHandle& getGlobalBindingSet() const { return m_global_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getViewBindingSet() const { return m_view_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getPrimitiveBindingSet() const { return m_primitive_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getSamplerBindingSet() const { return m_sampler_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getDescriptorBindingSet() const { return m_descriptor_binding_set; }
        [[nodiscard]] const GfxBufferHandle& getGlobalConstantBuffer() const { return m_global_constant_buffer; }
        [[nodiscard]] const GfxBufferHandle& getViewConstantBuffer() const { return m_view_constant_buffer; }
        [[nodiscard]] const GfxBufferHandle& getPrimitiveConstantBuffer() const { return m_primitive_constant_buffer; }

        void buildCachedCommands(
            const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
            const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
            const DynamicArray<UInt32>& mesh_pass_primitive_indices,
            const Matrix4f& view_projection,
            MeshDrawCommandCache& cache,
            DynamicArray<MeshDrawInstance>& out_instances,
            DynamicArray<PrimitiveMeshDrawShaderData>& out_shader_data,
            DrawCommandList& cmd_list) const;

        void buildDynamicCommands(
            const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
            const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
            const DynamicArray<UInt32>& mesh_pass_primitive_indices,
            const Matrix4f& view_projection,
            DynamicArray<MeshDrawCommand>& frame_commands,
            DynamicArray<MeshDrawInstance>& out_instances,
            DynamicArray<PrimitiveMeshDrawShaderData>& out_shader_data,
            DrawCommandList& cmd_list) const;
    };

} // namespace dodoe
