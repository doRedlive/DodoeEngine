// do@Redlive

#include "shadow_mesh_processor.h"

#include "../render_scene/primitive_render_object.h"
#include "runtime/function/graphics/gfx_context.h"
#include "mesh_draw_list.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/shader/shader_parameter.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;
    }

    GfxGraphicsPipelineDesc ShadowMeshProcessor::buildPipelineDescription(
        const MeshPassPipelineContext& context) const {
        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(context.vertex_shader)
            .setPixelShader(context.pixel_shader)
            .setInputLayout(context.input_layout)
            .addBindingLayout(m_global_binding_layout)
            .addBindingLayout(m_view_binding_layout)
            .setPrimType(GfxPrimitiveType::TriangleList);
        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullBack().setDepthBiasClamp(0.0f).setDepthBias(6).setSlopeScaleDepthBias(1.5f);
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state).setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);
        return pipeline_desc;
    }

    ShadowMeshProcessor::ShadowMeshProcessor(BindingLayoutCache& binding_layout_cache,
                                             BindingSetCache& binding_set_cache)
        : MeshPassProcessor(MeshPassType::Shadow) {
        m_global_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Global))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kGlobalBindingConstants))
        );
        m_view_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kViewBindingConstants))
        );
        m_global_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(GlobalMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("ShadowMeshProcessor Global ConstantBuffer"));
        m_view_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("ShadowMeshProcessor View ConstantBuffer"));
        m_global_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc().addItem(GfxBindingSetItem::ConstantBuffer(shader_bindings::kGlobalBindingConstants, m_global_constant_buffer->getRHIHandle())),
            m_global_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_global_binding_layout)
        );
        m_view_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc().addItem(GfxBindingSetItem::ConstantBuffer(shader_bindings::kViewBindingConstants, m_view_constant_buffer->getRHIHandle())),
            m_view_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_view_binding_layout)
        );
    }

    void ShadowMeshProcessor::reset() {
        m_global_constant_buffer = nullptr;
        m_view_constant_buffer = nullptr;
        m_global_binding_set = nullptr;
        m_view_binding_set = nullptr;
        m_global_binding_layout = nullptr;
        m_view_binding_layout = nullptr;
    }

    Bool ShadowMeshProcessor::shouldDrawPrimitive(const PrimitiveSceneInfo& primitive) const {
        return primitive.isVisible() && primitive.castsShadow();
    }

    Bool ShadowMeshProcessor::setupMeshDrawCommand(
        const MeshBatch&, const MeshBatchElement&, MeshDrawCommand& command,
        PrimitiveMeshDrawShaderData&) const {
        command.setBindingSet(ShaderParameterSet::Global, m_global_binding_set);
        command.setBindingSet(ShaderParameterSet::View, m_view_binding_set);
        return true;
    }


} // namespace dodoe
