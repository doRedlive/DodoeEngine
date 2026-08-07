// do@Redlive

#include "base_scene_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_base_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_skybox_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/material/material_system.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    static RenderTargetDesc BuildGBufferDesc() {
        RenderTargetDesc desc{};
        desc.name = "GBuffer";
        desc.scale_policy = RenderTargetScalePolicy::Relative;
        desc.scale_x = 1.0f;
        desc.scale_y = 1.0f;

        desc.color_attachments.push_back({
            GfxFormat::RGBA8_UNORM, "GBufferAlbedo", GfxColor(0.08f, 0.09f, 0.11f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA16_FLOAT, "GBufferNormal", GfxColor(0.0f, 0.0f, 0.0f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA32_FLOAT, "GBufferPosition", GfxColor(0.0f, 0.0f, 0.0f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA8_UNORM, "GBufferMaterial", GfxColor(0.0f, 1.0f, 1.0f, 1.0f)
        });

        desc.has_depth = true;
        desc.depth_format = GfxFormat::D32;
        desc.depth_debug_name = "GBufferDepth";
        desc.clear_depth = 1.0f;

        return desc;
    }

    static RenderTargetDesc BuildShadowMapDesc() {
        RenderTargetDesc desc{};
        desc.name = "ShadowMap";
        desc.scale_policy = RenderTargetScalePolicy::Relative;
        desc.scale_x = 1.0f;
        desc.scale_y = 1.0f;

        desc.has_depth = true;
        desc.depth_format = GfxFormat::D32;
        desc.depth_debug_name = "ShadowMapDepth";
        desc.clear_depth = 1.0f;

        return desc;
    }

    static GfxFramebufferInfo MakeGBufferFramebufferInfo() {
        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info
            .addColorFormat(GfxFormat::RGBA8_UNORM)
            .addColorFormat(GfxFormat::RGBA16_FLOAT)
            .addColorFormat(GfxFormat::RGBA32_FLOAT)
            .addColorFormat(GfxFormat::RGBA8_UNORM)
            .setDepthFormat(GfxFormat::D32);
        return framebuffer_info;
    }

    static GfxFramebufferInfo MakeShadowFramebufferInfo() {
        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info.setDepthFormat(GfxFormat::D32);
        return framebuffer_info;
    }

    static GfxGraphicsPipelineDesc MakeGBufferPipelineDesc(const ShaderLibrary& shader_library,
                                                            GfxInputLayoutHandle gbuffer_input_layout,
                                                            const GfxBindingLayoutHandle& binding_layout,
                                                            const GfxBindingLayoutHandle& sampler_binding_layout,
                                                            DescriptorTableManager* descriptor_table,
                                                            BindingLayoutCache* binding_layout_cache) {
        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(shader_library.getGBufferVertexShader())
            .setPixelShader(shader_library.getGBufferPixelShader())
            .setInputLayout(gbuffer_input_layout)
            .addBindingLayout(binding_layout)
            .addBindingLayout(sampler_binding_layout)
            .setPrimType(GfxPrimitiveType::TriangleList);
        if (RenderSettings::IsBindlessActive()) {
            if (descriptor_table && descriptor_table->getDescriptorTable()) {
                pipeline_desc.addBindingLayout(descriptor_table->getDescriptorTable()->getLayout());
            }
        } else if (binding_layout_cache) {
            auto texture_layout = binding_layout_cache->getOrCreate(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::Pixel)
                    .setRegisterSpaceIsDescriptorSet(true)
                    .setRegisterSpace(2)
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(1)));
            pipeline_desc.addBindingLayout(texture_layout);
        }
        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state);
        pipeline_desc.setRenderState(render_state);
        return pipeline_desc;
    }

    static GfxGraphicsPipelineDesc MakeShadowPipelineDesc(const ShaderLibrary& shader_library,
                                                           GfxInputLayoutHandle shadow_input_layout,
                                                           const GfxBindingLayoutHandle& binding_layout) {
        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(shader_library.getShadowVertexShader())
            .setPixelShader(shader_library.getShadowPixelShader())
            .setInputLayout(shadow_input_layout)
            .addBindingLayout(binding_layout)
            .setPrimType(GfxPrimitiveType::TriangleList);
        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullBack().setDepthBiasClamp(0.0f).setDepthBias(6).setSlopeScaleDepthBias(1.5f);
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state);
        render_state.setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);
        return pipeline_desc;
    }

    static MeshPassRelevance BuildPrimitiveMeshPassRelevance(const PrimitiveSceneInfo& primitive) {
        MeshPassRelevance relevance{};
        if (!primitive.isVisible()) {
            return relevance;
        }

        for (UInt32 pass_index = 0; pass_index < static_cast<UInt32>(MeshPassType::Count); pass_index++) {
            const auto pass_type = static_cast<MeshPassType>(pass_index);
            relevance.setRelevant(pass_type, primitive.hasRelevantBatch(pass_type));
        }
        return relevance;
    }

    void BaseSceneFeature::initialize(SharedRenderService& resources) {
        auto* gfx = resources.getGfxContext();
        auto* deletion_queue = resources.getRenderTargetSystem()
            ? resources.getRenderTargetSystem()->getDeletionQueue()
            : nullptr;
        m_shared_render_service = &resources;

        m_gbuffer = create_scope<RenderTargetHandle>();
        m_gbuffer->initialize(BuildGBufferDesc(), *gfx, deletion_queue);

        m_shadow_map = create_scope<RenderTargetHandle>();
        m_shadow_map->initialize(BuildShadowMapDesc(), *gfx, deletion_queue);

        GfxBindingSetHandle descriptor_binding_set{};
        auto* descriptor_table = resources.getDescriptorTable();
        if (descriptor_table && descriptor_table->getDescriptorTable()) {
            descriptor_binding_set = create_ref<GfxBindingSet>(
                cutie::BindingSetHandle(descriptor_table->getDescriptorTable()));
        }
        auto* binding_layout_cache = resources.getBindingLayoutCache();
        auto* binding_set_cache = resources.getBindingSetCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaseSceneFeature binding layout cache is null");
        DO_ASSERT(binding_set_cache != nullptr, "BaseSceneFeature binding set cache is null");
        m_gbuffer_processor = create_scope<GBufferMeshProcessor>(
            descriptor_binding_set, *binding_layout_cache, *binding_set_cache);
	    m_shadow_processor = create_scope<DirectionalShadowMeshProcessor>(
            *binding_layout_cache, *binding_set_cache);

        m_skybox_cb = create_ref<GfxBuffer>(
            GfxBufferDesc()
                .setByteSize(sizeof(Matrix4f))
                .setIsConstantBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                .setDebugName("SkyboxCB"));
        m_skybox_cb->initializeRHI(gfx->getDevice());
    }

    void BaseSceneFeature::onResize(const UInt32 width, const UInt32 height) {
        auto* gfx = m_shared_render_service ? m_shared_render_service->getGfxContext() : nullptr;
        DO_ASSERT(gfx != nullptr, "BaseSceneFeature onResize requires valid GfxContext");

        if (m_gbuffer) {
            m_gbuffer->resolve(width, height, *gfx, 0);
        }
        if (m_shadow_map) {
            m_shadow_map->resolve(width, height, *gfx, 0);
        }
    }

    void BaseSceneFeature::shutdown() {
        if (m_gbuffer) {
            m_gbuffer->shutdown();
            m_gbuffer.reset();
        }
        if (m_shadow_map) {
            m_shadow_map->shutdown();
            m_shadow_map.reset();
        }
        if (m_gbuffer_processor) {
            m_gbuffer_processor->reset();
            m_gbuffer_processor.reset();
        }
        if (m_shadow_processor) {
            m_shadow_processor->reset();
            m_shadow_processor.reset();
        }
        m_skybox_cb.reset();
    }

    void BaseSceneFeature::registerGraphImports(RenderGraphImportRegistry& imports,
                                                const RenderView& view) {
        if (m_gbuffer) {
            imports.publish<GBufferRenderTargetKey>(m_gbuffer.get());
        }
        if (m_shadow_map) {
            imports.publish<ShadowMapRenderTargetKey>(m_shadow_map.get());
        }
        if (m_skybox_cb) {
            imports.publish<SkyboxConstantBufferKey>(m_skybox_cb);
        }
    }

    void BaseSceneFeature::collectPasses(PassCollector& collector) {
	    DO_ASSERT(m_gbuffer_processor != nullptr, "BaseSceneFeature GBuffer processor is null");
	    DO_ASSERT(m_shadow_processor != nullptr, "BaseSceneFeature shadow processor is null");
        collector.addPass<GBufferPass>(m_gbuffer_processor.get());
        collector.addPass<DirectionalShadowPass>(m_shadow_processor.get());
        collector.addPass<SkyboxPass>();
    }

    void BaseSceneFeature::setupMeshPassContexts(const RenderScene& scene,
                                                  RenderViewFamily& view_family) const {
        Vector3f light_direction(0.3f, -0.8f, -0.5f);
        for (const auto& info : scene.getLightSceneInfos()) {
            if (info.getLightType() == LightType::Directional && info.isEnabled()) {
                light_direction = info.getDirectionalLightData().direction;
                break;
            }
        }
        const Matrix4f directional_light_view_projection =
            rendering_pipeline_utils::BuildDirectionalLightViewProjection(light_direction);

        for (auto& view : view_family.getViews()) {
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
            mesh_ext.frame_time_data = Vector4f(view_family.getTimeSeconds(),
                                                 view_family.getDeltaSeconds(), 0.0f, 0.0f);
            Size_t total_instance_count = 0;
            for (const auto* primitive : mesh_ext.visible_primitives) {
                total_instance_count += primitive ? primitive->getInstanceCount() : 1;
            }
            mesh_ext.instance_scene_data.reserve(total_instance_count);
            for (const auto* primitive : mesh_ext.visible_primitives) {
                if (primitive) {
                    for (const auto& inst_data : primitive->getInstanceSceneData()) {
                        mesh_ext.instance_scene_data.push_back(inst_data);
                    }
                } else {
                    InstanceSceneData inst_scene_data{};
                    mesh_ext.instance_scene_data.push_back(inst_scene_data);
                }
            }
            mesh_ext.directional_shadow_view_projection = directional_light_view_projection;

            auto& ext = view.getOrCreateExtension<MeshViewExtension>();
            ext.primitive_mesh_pass_relevance.clear();
            ext.primitive_mesh_pass_relevance.reserve(mesh_ext.visible_primitives.size());
            for (const auto* primitive : ext.visible_primitives) {
                DO_ASSERT(primitive != nullptr, "BaseSceneFeature visible primitive is null");
                ext.primitive_mesh_pass_relevance.push_back(BuildPrimitiveMeshPassRelevance(*primitive));
            }
            ext.buildMeshPassPrimitiveIndices();
        }
    }

    void BaseSceneFeature::buildMeshDrawCommands(RenderViewFamily& view_family,
                                                   DrawCommandList& cmd_list) {
        DO_ASSERT(m_shared_render_service != nullptr, "BaseSceneFeature shared render service is null");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "BaseSceneFeature shader library is null");
        DO_ASSERT(m_shared_render_service->getPipelineStateCache() != nullptr, "BaseSceneFeature pipeline cache is null");

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
        const auto gbuffer_input_layout = input_layout_cache
            ? input_layout_cache->getOrCreate(mesh_vertex_attributes, shader_library.getGBufferVertexShader())
            : GfxInputLayoutHandle{};
        const auto shadow_input_layout = input_layout_cache
            ? input_layout_cache->getOrCreate(mesh_vertex_attributes, shader_library.getShadowVertexShader())
            : GfxInputLayoutHandle{};

        auto* pso_cache = m_shared_render_service->getPipelineStateCache();
        DO_ASSERT(pso_cache != nullptr, "BaseSceneFeature PSO cache is null");

        const auto gbuffer_fb_info = MakeGBufferFramebufferInfo();
        const auto shadow_fb_info  = MakeShadowFramebufferInfo();

        pso_cache->resolveGraphicsPipeline(
            MeshPassType::GBuffer,
            MakeGBufferPipelineDesc(shader_library, gbuffer_input_layout,
                m_gbuffer_processor->getBindingLayout(),
                m_gbuffer_processor->getSamplerBindingLayout(),
                m_shared_render_service->getDescriptorTable(),
                m_shared_render_service->getBindingLayoutCache()),
            gbuffer_fb_info,
            cmd_list);

        pso_cache->resolveGraphicsPipeline(
            MeshPassType::DirectionalShadow,
            MakeShadowPipelineDesc(shader_library, shadow_input_layout,
                m_shadow_processor->getBindingLayout()),
            shadow_fb_info,
            cmd_list);

        m_gbuffer_draw_lists.resize(view_family.getSize());
        m_shadow_draw_lists.resize(view_family.getSize());

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

            auto& gbuffer_list = m_gbuffer_draw_lists[view_index];
            gbuffer_list.reset();
            gbuffer_list.cached_commands = &m_mesh_draw_cache.getCommands();

            auto& shadow_list = m_shadow_draw_lists[view_index];
            shadow_list.reset();
            shadow_list.cached_commands = &m_mesh_draw_cache.getCommands();

            m_gbuffer_processor->buildCachedCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::GBuffer)],
                view.getViewProjectionMatrix(),
                m_mesh_draw_cache,
                gbuffer_list.cached_instances,
                gbuffer_list.cached_shader_data
            );
            m_gbuffer_processor->buildDynamicCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::GBuffer)],
                view.getViewProjectionMatrix(),
                gbuffer_list.frame_commands,
                gbuffer_list.dynamic_instances,
                gbuffer_list.dynamic_shader_data
            );
            m_shadow_processor->buildCachedCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                mesh_ext.directional_shadow_view_projection,
                m_mesh_draw_cache,
                shadow_list.cached_instances
            );
            m_shadow_processor->buildDynamicCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                mesh_ext.directional_shadow_view_projection,
                shadow_list.frame_commands,
                shadow_list.dynamic_instances
            );
        }
    }

} // dodoe
