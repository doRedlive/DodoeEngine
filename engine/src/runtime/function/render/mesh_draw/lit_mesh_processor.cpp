// do@Redlive

#include "lit_mesh_processor.h"

#include "mesh_draw_types.h"
#include "mesh_draw_list.h"
#include "runtime/core/math/math.h"
#include "../render_scene/primitive_render_object.h"
#include "../material/material_system.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;

        GfxGraphicsPipelineDesc MakeLitPipelineDesc(const MeshPassPipelineContext& context,
                                                    const GfxBindingLayoutHandle& global_binding_layout,
                                                    const GfxBindingLayoutHandle& view_binding_layout,
                                                    const GfxBindingLayoutHandle& primitive_binding_layout,
                                                    const GfxBindingLayoutHandle& sampler_binding_layout) {
            auto pipeline_desc = GfxGraphicsPipelineDesc()
                .setVertexShader(context.vertex_shader)
                .setPixelShader(context.pixel_shader)
                .setInputLayout(context.input_layout)
                .addBindingLayout(global_binding_layout)
                .addBindingLayout(view_binding_layout)
                .setPrimType(GfxPrimitiveType::TriangleList);
            if (RenderSettings::IsBindlessActive()) {
                pipeline_desc.addBindingLayout(sampler_binding_layout);
            } else if (context.binding_layout_cache) {
                auto material_layout = context.binding_layout_cache->getOrCreate(
                    GfxBindingLayoutDesc()
                        .setVisibility(GfxShaderType::Pixel)
                        .setRegisterSpaceIsDescriptorSet(true)
                        .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
                        .addItem(GfxBindingLayoutItem::Sampler(1))
                        .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                        .addItem(GfxBindingLayoutItem::Texture_SRV(3)));
                pipeline_desc.addBindingLayout(material_layout);
            }
            if (context.pass_binding_layout) {
                pipeline_desc.addBindingLayout(context.pass_binding_layout);
            }
            pipeline_desc.addBindingLayout(primitive_binding_layout);
            if (RenderSettings::IsBindlessActive() && context.descriptor_table &&
                context.descriptor_table->getDescriptorTable()) {
                pipeline_desc.addBindingLayout(context.descriptor_table->getDescriptorTable()->getLayout());
            }
            return pipeline_desc;
        }
    }

    GfxGraphicsPipelineDesc LitMeshProcessor::buildPipelineDescription(
        const MeshPassPipelineContext& context) const {
        auto pipeline_desc = MakeLitPipelineDesc(context, m_global_binding_layout,
            m_view_binding_layout, m_primitive_binding_layout, m_sampler_binding_layout);
        GfxDepthStencilState depth_stencil_state;
        GfxRenderState render_state;
        if (getMeshPassType() == MeshPassType::Transparent) {
            depth_stencil_state.enableDepthTest().disableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
            GfxBlendState blend_state;
            GfxBlendState::RenderTarget blend_target;
            blend_target.enableBlend()
                .setSrcBlend(GfxBlendFactor::SrcAlpha)
                .setDestBlend(GfxBlendFactor::OneMinusSrcAlpha)
                .setSrcBlendAlpha(GfxBlendFactor::One)
                .setDestBlendAlpha(GfxBlendFactor::OneMinusSrcAlpha);
            blend_state.setRenderTarget(0, blend_target);
            render_state.setDepthStencilState(depth_stencil_state).setBlendState(blend_state);
        } else {
            depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
            render_state.setDepthStencilState(depth_stencil_state);
        }
        pipeline_desc.setRenderState(render_state);
        return pipeline_desc;
    }

    LitMeshProcessor::LitMeshProcessor(const MeshPassType pass_type,
                                       GfxBindingSetHandle descriptor_binding_set,
                                       BindingLayoutCache& binding_layout_cache,
                                       BindingSetCache& binding_set_cache)
        : MeshPassProcessor(pass_type),
          m_descriptor_binding_set(std::move(descriptor_binding_set)) {
        m_sampler = GDrawCommandList.createSampler(GfxSamplerDesc());
        m_global_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Global))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kGlobalBindingConstants)));
        m_view_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kViewBindingConstants)));
        m_primitive_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Primitive))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kPrimitiveBindingConstants)));
        m_sampler_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
                .addItem(GfxBindingLayoutItem::Sampler(shader_bindings::kMaterialBindingSampler)));
        m_global_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(GlobalMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("LitMeshProcessor Global ConstantBuffer"));
        m_view_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("LitMeshProcessor View ConstantBuffer"));
        m_primitive_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(PrimitiveMeshDrawShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("LitMeshProcessor Primitive ConstantBuffer"));
        m_global_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::ConstantBuffer(shader_bindings::kGlobalBindingConstants, m_global_constant_buffer->getRHIHandle())),
            m_global_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_global_binding_layout));
        m_view_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::ConstantBuffer(shader_bindings::kViewBindingConstants, m_view_constant_buffer->getRHIHandle())),
            m_view_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_view_binding_layout));
        m_primitive_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::ConstantBuffer(shader_bindings::kPrimitiveBindingConstants, m_primitive_constant_buffer->getRHIHandle())),
            m_primitive_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_primitive_binding_layout));
        m_sampler_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::Sampler(shader_bindings::kMaterialBindingSampler, m_sampler)),
            m_sampler_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_sampler_binding_layout));
    }

    void LitMeshProcessor::reset() {
        m_global_constant_buffer = nullptr;
        m_view_constant_buffer = nullptr;
        m_primitive_constant_buffer = nullptr;
        m_global_binding_set = nullptr;
        m_view_binding_set = nullptr;
        m_primitive_binding_set = nullptr;
        m_sampler_binding_set = nullptr;
        m_global_binding_layout = nullptr;
        m_view_binding_layout = nullptr;
        m_primitive_binding_layout = nullptr;
        m_sampler_binding_layout = nullptr;
        m_sampler = nullptr;
    }

    Bool LitMeshProcessor::shouldDrawPrimitive(const PrimitiveSceneInfo& primitive) const {
        return primitive.isVisible();
    }

    Bool LitMeshProcessor::setupMeshDrawCommand(
        const MeshBatch& batch,
        const MeshBatchElement&,
        MeshDrawCommand& command,
        PrimitiveMeshDrawShaderData& shader_data) const {
        const auto* material = batch.getMaterialInstance();
        if (!material) {
            return false;
        }
        shader_data.draw_data.x = material->texture_descriptor_indices.empty()
            ? 0 : material->texture_descriptor_indices[0];
        shader_data.draw_data.y = material->texture_descriptor_indices.size() > 1
            ? material->texture_descriptor_indices[1] : -1;
        shader_data.draw_data.z = material->texture_descriptor_indices.size() > 1 ? 1 : 0;
        command.setBindingSet(ShaderParameterSet::Global, m_global_binding_set);
        command.setBindingSet(ShaderParameterSet::View, m_view_binding_set);
        command.setBindingSet(ShaderParameterSet::Primitive, m_primitive_binding_set);
        if (RenderSettings::IsBindlessActive()) {
            command.setBindingSet(ShaderParameterSet::Material, m_sampler_binding_set);
            command.setBindingSet(ShaderParameterSet::Bindless, m_descriptor_binding_set);
        } else {
            const auto& material_binding_set = batch.getMaterialBindingSet();
            if (!material_binding_set) {
                return false;
            }
            command.setBindingSet(ShaderParameterSet::Material, material_binding_set);
        }
        return true;
    }

} // namespace dodoe
