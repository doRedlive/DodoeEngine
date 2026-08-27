#include "transparent_scene_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_opaque_pass.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/mesh_draw/lit_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    static GfxFramebufferInfo MakeTransparentFramebufferInfo() {
        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info
            .addColorFormat(GfxFormat::RGBA16_FLOAT)
            .setDepthFormat(GfxFormat::D32);
        return framebuffer_info;
    }

    static GfxGraphicsPipelineDesc MakeTransparentPipelineDesc(const ShaderLibrary& shader_library,
                                                               GfxInputLayoutHandle mesh_input_layout,
                                                               const GfxBindingLayoutHandle& global_binding_layout,
                                                               const GfxBindingLayoutHandle& view_binding_layout,
                                                               const GfxBindingLayoutHandle& primitive_binding_layout,
                                                               const GfxBindingLayoutHandle& sampler_binding_layout,
                                                               const GfxBindingLayoutHandle& pass_binding_layout,
                                                               DescriptorTableManager* descriptor_table,
                                                               BindingLayoutCache* binding_layout_cache) {
        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(shader_library.getGBufferVertexShader())
            .setPixelShader(shader_library.getOpaquePixelShader())
            .setInputLayout(mesh_input_layout)
            .addBindingLayout(global_binding_layout)
            .addBindingLayout(view_binding_layout)
            .setPrimType(GfxPrimitiveType::TriangleList);
        if (RenderSettings::IsBindlessActive()) {
            pipeline_desc.addBindingLayout(sampler_binding_layout);
        } else if (binding_layout_cache) {
            auto material_layout = binding_layout_cache->getOrCreate(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::Pixel)
                    .setRegisterSpaceIsDescriptorSet(true)
                    .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
                    .addItem(GfxBindingLayoutItem::Sampler(1))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(3)));
            pipeline_desc.addBindingLayout(material_layout);
        }
        if (pass_binding_layout) {
            pipeline_desc.addBindingLayout(pass_binding_layout);
        }
        pipeline_desc.addBindingLayout(primitive_binding_layout);
        if (RenderSettings::IsBindlessActive() && descriptor_table && descriptor_table->getDescriptorTable()) {
            pipeline_desc.addBindingLayout(descriptor_table->getDescriptorTable()->getLayout());
        }
        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().disableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxBlendState blend_state;
        GfxBlendState::RenderTarget blend_target;
        blend_target.enableBlend()
            .setSrcBlend(GfxBlendFactor::SrcAlpha)
            .setDestBlend(GfxBlendFactor::OneMinusSrcAlpha)
            .setSrcBlendAlpha(GfxBlendFactor::One)
            .setDestBlendAlpha(GfxBlendFactor::OneMinusSrcAlpha);
        blend_state.setRenderTarget(0, blend_target);
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state).setBlendState(blend_state);
        pipeline_desc.setRenderState(render_state);
        return pipeline_desc;
    }

    void TransparentSceneFeature::initialize(SharedRenderService& resources) {
        m_shared_render_service = &resources;

        GfxBindingSetHandle descriptor_binding_set{};
        auto* descriptor_table = resources.getDescriptorTable();
        if (descriptor_table && descriptor_table->getDescriptorTable()) {
            descriptor_binding_set = create_ref<GfxBindingSet>(
                cutie::BindingSetHandle(descriptor_table->getDescriptorTable()));
        }
        auto* binding_layout_cache = resources.getBindingLayoutCache();
        auto* binding_set_cache = resources.getBindingSetCache();
        DO_ASSERT(binding_layout_cache != nullptr, "TransparentSceneFeature binding layout cache is null");
        DO_ASSERT(binding_set_cache != nullptr, "TransparentSceneFeature binding set cache is null");
        m_lit_processor = create_scope<LitMeshProcessor>(
            MeshPassType::Transparent, descriptor_binding_set, *binding_layout_cache, *binding_set_cache);
    }

    void TransparentSceneFeature::shutdown() {
        if (m_lit_processor) {
            m_lit_processor->reset();
            m_lit_processor.reset();
        }
    }

    void TransparentSceneFeature::registerGraphImports(RenderGraphImportRegistry& imports,
                                                       const RenderView& view) {
        (void)imports;
        (void)view;
    }

    void TransparentSceneFeature::collectPasses(PassCollector& collector) {
        DO_ASSERT(m_lit_processor != nullptr, "TransparentSceneFeature lit processor is null");
        collector.addPass<TransparentPass>(m_lit_processor.get());
    }

    void TransparentSceneFeature::buildTransparentDrawCommands(RenderViewFamily& view_family,
                                                               DrawCommandList& cmd_list) {
        DO_ASSERT(m_shared_render_service != nullptr, "TransparentSceneFeature shared render service is null");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "TransparentSceneFeature shader library is null");
        DO_ASSERT(m_shared_render_service->getPipelineStateCache() != nullptr, "TransparentSceneFeature pipeline cache is null");

        const auto& shader_library = *m_shared_render_service->getShaderLibrary();

        constexpr Size_t kVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        constexpr Size_t kInstanceStride = sizeof(InstanceSceneData);
        const DynamicArray<GfxVertexAttributeDesc> mesh_vertex_attributes = {
            GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("a_Normal").setFormat(GfxFormat::RGBA8_SNORM).setOffset(sizeof(Vector3f)).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(Vector3f) + sizeof(UInt32)).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("TEXCOORD3").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(0).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD4").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD5").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 2).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD6").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 3).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("a_InstanceColorTint").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("a_InstanceParams").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) + sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
        };

        auto* input_layout_cache = m_shared_render_service->getInputLayoutCache();
        const auto mesh_input_layout = input_layout_cache
            ? input_layout_cache->getOrCreate(mesh_vertex_attributes, shader_library.getGBufferVertexShader())
            : GfxInputLayoutHandle{};

        auto* pso_cache = m_shared_render_service->getPipelineStateCache();
        DO_ASSERT(pso_cache != nullptr, "TransparentSceneFeature PSO cache is null");

        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        auto* descriptor_table = m_shared_render_service->getDescriptorTable();

        const auto pass_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                .addItem(GfxBindingLayoutItem::Sampler(9)));

        (void)pso_cache->resolveGraphicsPipeline(
            MeshPassType::Transparent,
            MakeTransparentPipelineDesc(shader_library, mesh_input_layout,
                m_lit_processor->getGlobalBindingLayout(),
                m_lit_processor->getViewBindingLayout(),
                m_lit_processor->getPrimitiveBindingLayout(),
                m_lit_processor->getSamplerBindingLayout(),
                pass_binding_layout,
                descriptor_table,
                binding_layout_cache),
            MakeTransparentFramebufferInfo(),
            cmd_list);

        m_transparent_draw_lists.resize(view_family.getSize());

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

            auto& transparent_list = m_transparent_draw_lists[view_index];
            transparent_list.reset();
            transparent_list.cached_commands = &m_mesh_draw_cache.getCommands();

            m_lit_processor->buildCachedCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::Transparent)],
                view.getViewProjectionMatrix(),
                m_mesh_draw_cache,
                transparent_list.cached_instances,
                transparent_list.cached_shader_data
            );
            m_lit_processor->buildDynamicCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::Transparent)],
                view.getViewProjectionMatrix(),
                transparent_list.frame_commands,
                transparent_list.dynamic_instances,
                transparent_list.dynamic_shader_data
            );
        }
    }

} // namespace dodoe
