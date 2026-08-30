// do@Redlive

#include "shadow_scene_feature.h"

#include <chrono>

#include "runtime/function/render/render_pipeline/passes/render_shadow_pass.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/mesh_draw/shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"

namespace dodoe {

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

    static GfxFramebufferInfo MakeShadowFramebufferInfo() {
        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info.setDepthFormat(GfxFormat::D32);
        return framebuffer_info;
    }

    static GfxGraphicsPipelineDesc MakeShadowPipelineDesc(const ShaderLibrary& shader_library,
                                                          GfxInputLayoutHandle shadow_input_layout,
                                                          const GfxBindingLayoutHandle& global_binding_layout,
                                                          const GfxBindingLayoutHandle& view_binding_layout) {
        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(shader_library.getShadowVertexShader())
            .setPixelShader(shader_library.getShadowPixelShader())
            .setInputLayout(shadow_input_layout)
            .addBindingLayout(global_binding_layout)
            .addBindingLayout(view_binding_layout)
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

    void ShadowSceneFeature::initialize(SharedRenderService& resources) {
        auto* gfx = resources.getGfxContext();
        auto* deletion_queue = resources.getRenderTargetSystem()
            ? resources.getRenderTargetSystem()->getDeletionQueue()
            : nullptr;
        m_shared_render_service = &resources;

        m_shadow_map = create_scope<RenderTargetHandle>();
        m_shadow_map->initialize(BuildShadowMapDesc(), *gfx, deletion_queue);

        auto* binding_layout_cache = resources.getBindingLayoutCache();
        auto* binding_set_cache = resources.getBindingSetCache();
        DO_ASSERT(binding_layout_cache != nullptr, "ShadowSceneFeature binding layout cache is null");
        DO_ASSERT(binding_set_cache != nullptr, "ShadowSceneFeature binding set cache is null");
        m_shadow_processor = create_scope<ShadowMeshProcessor>(
            *binding_layout_cache, *binding_set_cache);
    }

    void ShadowSceneFeature::onResize(const UInt32 width, const UInt32 height) {
        auto* gfx = m_shared_render_service ? m_shared_render_service->getGfxContext() : nullptr;
        DO_ASSERT(gfx != nullptr, "ShadowSceneFeature onResize requires valid GfxContext");

        if (m_shadow_map) {
            m_shadow_map->resolve(width, height, *gfx, 0);
        }
    }

    void ShadowSceneFeature::shutdown() {
        if (m_shadow_map) {
            m_shadow_map->shutdown();
            m_shadow_map.reset();
        }
        if (m_shadow_processor) {
            m_shadow_processor->reset();
            m_shadow_processor.reset();
        }
    }

    void ShadowSceneFeature::registerGraphImports(RenderGraphImportRegistry& imports,
                                                  const RenderView& view) {
        (void)view;
        if (m_shadow_map) {
            imports.publish<ShadowMapRenderTargetKey>(m_shadow_map.get());
        }
    }

    void ShadowSceneFeature::collectPasses(PassCollector& collector) {
        DO_ASSERT(m_shadow_processor != nullptr, "ShadowSceneFeature shadow processor is null");
        collector.addPass<ShadowPass>(m_shadow_processor.get());
    }

    void ShadowSceneFeature::buildShadowDrawCommands(RenderViewFamily& view_family,
                                                     DrawCommandList& cmd_list) {
        DO_ASSERT(m_shared_render_service != nullptr, "ShadowSceneFeature shared render service is null");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "ShadowSceneFeature shader library is null");
        DO_ASSERT(m_shared_render_service->getPipelineStateCache() != nullptr, "ShadowSceneFeature pipeline cache is null");

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
        const auto shadow_input_layout = input_layout_cache
            ? input_layout_cache->getOrCreate(mesh_vertex_attributes, shader_library.getShadowVertexShader())
            : GfxInputLayoutHandle{};

        auto* pso_cache = m_shared_render_service->getPipelineStateCache();
        DO_ASSERT(pso_cache != nullptr, "ShadowSceneFeature PSO cache is null");

        const auto shadow_fb_info = MakeShadowFramebufferInfo();
        (void)pso_cache->resolveGraphicsPipeline(
            MeshPassType::Shadow,
            MakeShadowPipelineDesc(shader_library, shadow_input_layout,
                m_shadow_processor->getGlobalBindingLayout(),
                m_shadow_processor->getViewBindingLayout()),
            shadow_fb_info,
            cmd_list);

        m_shadow_draw_lists.resize(view_family.getSize());

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

            auto& shadow_list = m_shadow_draw_lists[view_index];
            shadow_list.reset();
            shadow_list.cached_commands = &m_mesh_draw_cache.getCommands();

            m_shadow_processor->buildCachedCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::Shadow)],
                mesh_ext.directional_shadow_view_projection,
                m_mesh_draw_cache,
                shadow_list.cached_instances
            );
            m_shadow_processor->buildDynamicCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::Shadow)],
                mesh_ext.directional_shadow_view_projection,
                shadow_list.frame_commands,
                shadow_list.dynamic_instances
            );
        }

        static auto last_stats_sample = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        if (now - last_stats_sample >= std::chrono::seconds(1)) {
            last_stats_sample = now;
            DO_WARN("MeshDrawCache[SHADOW]: commands={}", m_mesh_draw_cache.size());
        }
    }

} // namespace dodoe
