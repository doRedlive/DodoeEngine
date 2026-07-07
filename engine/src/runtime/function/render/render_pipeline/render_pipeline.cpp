// do@Redlive

#include "render_pipeline.h"

#include "passes/render_pipeline_passes.h"
#include "render_feature/imgui_feature.h"
#include "render_feature/sprite_feature.h"
#include "render_feature/test_feature.h"
#include "render_feature/render_builtin_features.h"
#include "render_pipeline_pass_utils.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/common.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command.h"
#include "runtime/function/render/render_scene/render_object.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/framework/pipeline_state_cache.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {
    namespace {
        MeshPassRelevance BuildPrimitiveMeshPassRelevance(const PrimitiveSceneInfo& primitive) {
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

    }

    RenderPassContext RenderPipeline::buildPassContext(const RenderScene& scene) const {
        RenderPassContext context{};
        context.gfx_context = m_gfx_context;
        context.shared_render_service = m_shared_render_service;
        context.scene = &scene;
        context.local_vertex_factory = m_local_vertex_factory.get();
        for (size_t i = 0; i < static_cast<size_t>(MeshPassType::Count); ++i) {
            context.mesh_processors[i] = m_mesh_processors[i].get();
        }
        return context;
    }

    void RenderPipeline::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
        const auto pipeline_type = RenderSettings::GetRenderingPipelineType();
        for (auto& view : view_family.getViews()) {
            view.resetExtensions();
        }
        if (pipeline_type == RenderingPipelineType::Only2D) {
            view_family.buildVisibleSprites(scene);
        } else {
            view_family.buildVisiblePrimitives(scene);
        }
    }

    void RenderPipeline::setupMeshPassRelevance(RenderView& view) const {
        auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
        mesh_ext.primitive_mesh_pass_relevance.clear();
        mesh_ext.primitive_mesh_pass_relevance.reserve(mesh_ext.visible_primitives.size());

        for (const auto* primitive : mesh_ext.visible_primitives) {
            DO_ASSERT(primitive != nullptr, "RenderPipeline visible primitive is null");
            mesh_ext.primitive_mesh_pass_relevance.push_back(BuildPrimitiveMeshPassRelevance(*primitive));
        }

        mesh_ext.buildMeshPassPrimitiveIndices();
    }

    void RenderPipeline::setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const {
        Vector3f light_direction(0.3f, -0.8f, -0.5f);
        for (const auto& info : scene.getLightSceneInfos()) {
            if (info.getLightType() == LightType::Directional && info.isEnabled()) {
                light_direction = info.getDirectionalLightData().direction;
                break;
            }
        }
        const Matrix4f directional_light_view_projection = rendering_pipeline_utils::BuildDirectionalLightViewProjection(light_direction);

        for (auto& view : view_family.getViews()) {
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
            mesh_ext.frame_time_data = Vector4f(view_family.getTimeSeconds(), view_family.getDeltaSeconds(), 0.0f, 0.0f);
            mesh_ext.primitive_first_instance_offsets.reserve(mesh_ext.visible_primitives.size());
            Size_t total_instance_count = 0;
            for (const auto* primitive : mesh_ext.visible_primitives) {
                total_instance_count += primitive ? primitive->getInstanceCount() : 1;
            }
            mesh_ext.instance_scene_data.reserve(total_instance_count);
            for (const auto* primitive : mesh_ext.visible_primitives) {
                mesh_ext.primitive_first_instance_offsets.push_back(static_cast<UInt32>(mesh_ext.instance_scene_data.size()));
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
            setupMeshPassRelevance(view);
        }
    }

    void RenderPipeline::buildMeshDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list) const {
        DO_ASSERT(m_shared_render_service != nullptr, "RenderPipeline shared render service is null");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "RenderPipeline shader library is null");
        DO_ASSERT(m_shared_render_service->getPipelineStateCache() != nullptr, "RenderPipeline pipeline cache is null");
        DO_ASSERT(m_local_vertex_factory != nullptr, "RenderPipeline local vertex factory is null");
        DO_ASSERT(m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)] != nullptr, "RenderPipeline GBuffer mesh processor is null");
        DO_ASSERT(m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)] != nullptr, "RenderPipeline directional shadow mesh processor is null");

        const auto& shader_library = *m_shared_render_service->getShaderLibrary();
        const auto& local_vertex_factory = *m_local_vertex_factory;
        const auto& gbuffer_mesh_processor = *static_cast<const GBufferMeshProcessor*>(m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)].get());
        const auto& directional_shadow_mesh_processor = *static_cast<const DirectionalShadowMeshProcessor*>(m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)].get());
        auto build_gbuffer_framebuffer_info = []() {
            GfxFramebufferInfo framebuffer_info{};
            framebuffer_info
                .addColorFormat(GfxFormat::RGBA8_UNORM)
                .addColorFormat(GfxFormat::RGBA16_FLOAT)
                .addColorFormat(GfxFormat::RGBA32_FLOAT)
                .addColorFormat(GfxFormat::RGBA8_UNORM)
                .setDepthFormat(GfxFormat::D32);
            return framebuffer_info;
        };
        auto build_shadow_framebuffer_info = []() {
            GfxFramebufferInfo framebuffer_info{};
            framebuffer_info.setDepthFormat(GfxFormat::D32);
            return framebuffer_info;
        };
        auto build_gbuffer_pipeline_desc = [this, &shader_library, &local_vertex_factory, &gbuffer_mesh_processor]() {
            auto pipeline_desc = GfxGraphicsPipelineDesc()
                .setVertexShader(shader_library.getGBufferVertexShader())
                .setPixelShader(shader_library.getGBufferPixelShader())
                .setInputLayout(local_vertex_factory.getGBufferInputLayout())
                .addBindingLayout(gbuffer_mesh_processor.getBindingLayout())
                .setPrimType(GfxPrimitiveType::TriangleList);
            if (m_shared_render_service->getDescriptorTable() && m_shared_render_service->getDescriptorTable()->getDescriptorTable()) {
                pipeline_desc.addBindingLayout(m_shared_render_service->getDescriptorTable()->getDescriptorTable()->getLayout());
            }
            GfxDepthStencilState depth_stencil_state;
            depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
            GfxRenderState render_state;
            render_state.setDepthStencilState(depth_stencil_state);
            pipeline_desc.setRenderState(render_state);
            return pipeline_desc;
        };
        auto build_shadow_pipeline_desc = [&shader_library, &local_vertex_factory, &directional_shadow_mesh_processor]() {
            auto pipeline_desc = GfxGraphicsPipelineDesc()
                .setVertexShader(shader_library.getShadowVertexShader())
                .setPixelShader(shader_library.getShadowPixelShader())
                .setInputLayout(local_vertex_factory.getShadowInputLayout())
                .addBindingLayout(directional_shadow_mesh_processor.getBindingLayout())
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
        };
        auto assign_pipeline = [](DynamicArray<MeshDrawCommand>& commands, const GfxGraphicsPipelineHandle& pipeline) {
            for (auto& command : commands) {
                command.pipeline = pipeline;
            }
        };

        auto* pso_cache = m_shared_render_service->getPipelineStateCache();
        DO_ASSERT(pso_cache != nullptr, "RenderPipeline PSO cache is null");

        const auto gbuffer_fb_info  = build_gbuffer_framebuffer_info();
        const auto shadow_fb_info   = build_shadow_framebuffer_info();

        auto gbuffer_pipeline = pso_cache->resolveGraphicsPipeline(
            MeshPassType::GBuffer,
            build_gbuffer_pipeline_desc(),
            gbuffer_fb_info,
            cmd_list);

        auto shadow_pipeline = pso_cache->resolveGraphicsPipeline(
            MeshPassType::DirectionalShadow,
            build_shadow_pipeline_desc(),
            shadow_fb_info,
            cmd_list);

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
            gbuffer_mesh_processor.buildCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::GBuffer)],
                view.getViewProjectionMatrix(),
                mesh_ext.mesh_pass_commands[static_cast<size_t>(MeshPassType::GBuffer)],
                mesh_ext.gbuffer_shader_data
            );
            assign_pipeline(mesh_ext.mesh_pass_commands[static_cast<size_t>(MeshPassType::GBuffer)], gbuffer_pipeline);
            directional_shadow_mesh_processor.buildCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                mesh_ext.directional_shadow_view_projection,
                mesh_ext.mesh_pass_commands[static_cast<size_t>(MeshPassType::DirectionalShadow)]
            );
            assign_pipeline(mesh_ext.mesh_pass_commands[static_cast<size_t>(MeshPassType::DirectionalShadow)], shadow_pipeline);
        }
    }

    Bool RenderPipeline::initialize(const RenderPipelineCreateInfo& info) {
        const auto pipeline_type = RenderSettings::GetRenderingPipelineType();
        DO_ASSERT(pipeline_type == RenderingPipelineType::Deferred || pipeline_type == RenderingPipelineType::Only2D,
                  "RenderPipeline currently only supports Deferred and Only2D pipeline types");

        Size_t worker_count = info.worker_count;
        if (worker_count == 0) {
            worker_count = std::thread::hardware_concurrency();
        }

        m_thread_pool = create_scope<ThreadPool>(std::max(Size_t{1}, worker_count));
        m_gfx_context = info.gfx_context;
        m_shared_render_service = info.shared_render_service;
        DO_ASSERT(m_gfx_context != nullptr, "RenderPipeline requires valid gfx_context");
        DO_ASSERT(m_shared_render_service != nullptr, "RenderPipeline requires shared render service");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "RenderPipeline requires shader library");
        DO_ASSERT(m_shared_render_service->getDescriptorTable() != nullptr, "RenderPipeline requires descriptor table");
        DO_ASSERT(m_shared_render_service->getTextureManager() != nullptr, "RenderPipeline requires texture manager");
        const auto* shader_library = m_shared_render_service->getShaderLibrary();

        m_local_vertex_factory = create_scope<LocalVertexFactory>();
        if (pipeline_type == RenderingPipelineType::Deferred) {
            m_local_vertex_factory->initialize(
                GDrawCommandList,
                shader_library->getGBufferVertexShader(),
                shader_library->getShadowVertexShader()
            );
        }

        if (pipeline_type == RenderingPipelineType::Deferred) {
            m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)] = create_scope<GBufferMeshProcessor>();
            static_cast<GBufferMeshProcessor*>(m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)].get())->initialize(*m_gfx_context, m_shared_render_service->getDescriptorTable(), m_shared_render_service->getTextureManager());
            m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)] = create_scope<DirectionalShadowMeshProcessor>();
            static_cast<DirectionalShadowMeshProcessor*>(m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)].get())->initialize(*m_gfx_context);

            m_features.push_back(create_scope<BaseSceneFeature>());
            m_features.push_back(create_scope<LightingFeature>());
            m_features.push_back(create_scope<PostProcessFeature>());
        }
        m_features.push_back(create_scope<SpriteFeature>());
        if (pipeline_type == RenderingPipelineType::Only2D) {
            m_features.push_back(create_scope<PostProcess2DFeature>());
        }
        m_features.push_back(create_scope<TestFeature>());
        m_features.push_back(create_scope<ImGuiFeature>());
        m_features.push_back(create_scope<PresentFeature>());
        return true;
    }

    void RenderPipeline::shutdown() {
        m_features.clear();
        for (auto& proc : m_mesh_processors) {
            if (proc) {
                proc->reset();
                proc.reset();
            }
        }
        if (m_local_vertex_factory) {
            m_local_vertex_factory->reset();
            m_local_vertex_factory.reset();
        }
        m_shared_render_service = nullptr;
        m_gfx_context = nullptr;
        m_thread_pool.reset();
    }

    void RenderPipeline::render(RenderViewFamily& view_family, RenderScene& scene, const UInt32 swapchain_image_index, DrawCommandList& out_commands) {
        const auto pipeline_type = RenderSettings::GetRenderingPipelineType();
        switch (pipeline_type) {
        case RenderingPipelineType::Deferred:
            renderDeferred(view_family, scene, swapchain_image_index, out_commands);
            break;
        case RenderingPipelineType::Only2D:
            renderOnly2D(view_family, scene, swapchain_image_index, out_commands);
            break;
        default:
            DO_ERROR("Unsupported pipeline type");
            break;
        }
    }

    void RenderPipeline::renderDeferred(RenderViewFamily& view_family, RenderScene& scene, const UInt32 swapchain_image_index, DrawCommandList& out_commands) {
        initViews(scene, view_family);
        setupMeshPassContexts(scene, view_family);
        buildMeshDrawCommands(view_family, out_commands);
        buildFrameDrawCommandList(view_family, scene, swapchain_image_index, out_commands);
    }

    void RenderPipeline::renderOnly2D(RenderViewFamily& view_family, RenderScene& scene, const UInt32 swapchain_image_index, DrawCommandList& out_commands) {
        initViews(scene, view_family);
        buildFrameDrawCommandList(view_family, scene, swapchain_image_index, out_commands);
    }

    void RenderPipeline::buildFrameDrawCommandList(
        const RenderViewFamily& view_family,
        RenderScene& scene,
        const UInt32 swapchain_image_index,
        DrawCommandList& out_commands) const
    {
        const auto pass_context = buildPassContext(scene);

        DynamicArray<RenderGraphBuilder> graphs;
        graphs.reserve(view_family.getSize());

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            RenderGraphBuilder graph{};
            const auto& view = view_family.getView(view_index);
            const RenderFeatureContext feature_context{
                .view = &view,
                .pass_context = &pass_context
            };
            for (const auto& feature : m_features) {
                feature->registerPass(graph, feature_context);
            }
            graph.compile();
            graphs.push_back(std::move(graph));
        }

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            executeFrameGraph(graphs[view_index], view_family, scene,
                            view_family.getView(view_index), view_index, swapchain_image_index,
                            out_commands);
        }
    }

    void RenderPipeline::executeFrameGraph(
        RenderGraphBuilder& graph,
        const RenderViewFamily& view_family,
        RenderScene& scene,
        const RenderView& view,
        const Size_t view_index,
        const UInt32 swapchain_image_index,
        DrawCommandList& out_commands) const
    {
        RenderGraphExecuteContext context{};
        context.view_family = &view_family;
        context.scene = &scene;
        context.view = &view;
        context.view_index = view_index;
        context.gfx_context = m_gfx_context;
        context.swapchain_image_index = swapchain_image_index;
        graph.execute(*m_thread_pool, context, out_commands);
    }

} // dodoe
