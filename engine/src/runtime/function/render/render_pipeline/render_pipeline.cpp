// do@Redlive

#include "render_pipeline.h"

#include "passes/render_pipeline_passes.h"
#include "render_feature/imgui_feature.h"
#include "render_feature/sprite_feature.h"
#include "render_feature/render_builtin_features.h"
#include "render_pipeline_pass_utils.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/common.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/render/mesh_draw/view_mesh_draw_context.h"
#include "runtime/function/render/render_scene/render_object.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/render_scene/light_scene_info.h"

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
        context.deferred_light_constant_buffer = m_deferred_light_constant_buffer;
        for (size_t i = 0; i < static_cast<size_t>(MeshPassType::Count); ++i) {
            context.mesh_processors[i] = m_mesh_processors[i].get();
        }
        return context;
    }

    void RenderPipeline::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
        for (auto& view : view_family.getViews()) {
            view.resetFrameData();
        }
        view_family.buildVisiblePrimitives(scene);
    }

    void RenderPipeline::setupMeshPassRelevance(RenderView& view) const {
        auto& pass_data = view.getPassData();
        const auto& visibility_data = view.getVisibilityData();
        pass_data.primitive_mesh_pass_relevance.clear();
        pass_data.primitive_mesh_pass_relevance.reserve(visibility_data.visible_primitives.size());

        for (const auto* primitive : visibility_data.visible_primitives) {
            DO_ASSERT(primitive != nullptr, "RenderPipeline visible primitive is null");
            pass_data.primitive_mesh_pass_relevance.push_back(BuildPrimitiveMeshPassRelevance(*primitive));
        }

        pass_data.buildMeshPassPrimitiveIndices();
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
            auto& visibility_data = view.getVisibilityData();
            auto& instance_data = view.getInstanceData();
            auto& shader_data = view.getShaderData();
            auto visible_primitives = std::move(visibility_data.visible_primitives);
            view.resetFrameData();
            visibility_data.visible_primitives = std::move(visible_primitives);
            shader_data.frame_time_data = Vector4f(view_family.getTimeSeconds(), view_family.getDeltaSeconds(), 0.0f, 0.0f);
            instance_data.primitive_first_instance_offsets.reserve(visibility_data.visible_primitives.size());
            Size_t total_instance_count = 0;
            for (const auto* primitive : visibility_data.visible_primitives) {
                const auto* render_object = primitive ? primitive->getRenderObject() : nullptr;
                total_instance_count += render_object ? static_cast<const PrimitiveRenderObject*>(render_object)->getInstanceCount() : 1;
            }
            instance_data.instance_scene_data.reserve(total_instance_count);
            for (const auto* primitive : visibility_data.visible_primitives) {
                instance_data.primitive_first_instance_offsets.push_back(static_cast<UInt32>(instance_data.instance_scene_data.size()));
                const auto* render_object = primitive ? primitive->getRenderObject() : nullptr;
                if (render_object) {
                    static_cast<const PrimitiveRenderObject*>(render_object)->appendInstanceSceneData(instance_data.instance_scene_data, primitive->getWorldTransform());
                } else {
                    InstanceSceneData inst_scene_data{};
                    inst_scene_data.model = primitive ? primitive->getWorldTransform() : Matrix4f(1.0f);
                    instance_data.instance_scene_data.push_back(inst_scene_data);
                }
            }
            shader_data.directional_shadow_view_projection = directional_light_view_projection;
            setupMeshPassRelevance(view);
        }
    }

    void RenderPipeline::buildMeshDrawCommands(RenderViewFamily& view_family) const {
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

        auto* pipeline_state_cache = m_shared_render_service->getPipelineStateCache();
        const auto gbuffer_pipeline = pipeline_state_cache->resolveGraphicsPipeline(
            MeshPassType::GBuffer,
            build_gbuffer_pipeline_desc(),
            build_gbuffer_framebuffer_info()
        );
        const auto shadow_pipeline = pipeline_state_cache->resolveGraphicsPipeline(
            MeshPassType::DirectionalShadow,
            build_shadow_pipeline_desc(),
            build_shadow_framebuffer_info()
        );

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& visibility_data = view.getVisibilityData();
            auto& instance_data = view.getInstanceData();
            auto& pass_data = view.getPassData();
            auto& shader_data = view.getShaderData();
            gbuffer_mesh_processor.buildCommands(
                visibility_data,
                instance_data,
                pass_data,
                shader_data,
                view.getViewProjectionMatrix(),
                pass_data,
                shader_data
            );
            assign_pipeline(pass_data.getMeshPassCommands(MeshPassType::GBuffer), gbuffer_pipeline);
            directional_shadow_mesh_processor.buildCommands(
                visibility_data,
                instance_data,
                pass_data,
                shader_data.directional_shadow_view_projection,
                pass_data
            );
            assign_pipeline(pass_data.getMeshPassCommands(MeshPassType::DirectionalShadow), shadow_pipeline);
        }
    }

    Bool RenderPipeline::initialize(const RenderPipelineCreateInfo& info) {
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
        m_local_vertex_factory->initialize(
            *m_gfx_context,
            shader_library->getGBufferVertexShader(),
            shader_library->getShadowVertexShader()
        );
        m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)] = create_scope<GBufferMeshProcessor>();
        static_cast<GBufferMeshProcessor*>(m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)].get())->initialize(*m_gfx_context, m_shared_render_service->getDescriptorTable(), m_shared_render_service->getTextureManager());
        m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)] = create_scope<DirectionalShadowMeshProcessor>();
        static_cast<DirectionalShadowMeshProcessor*>(m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)].get())->initialize(*m_gfx_context);
        m_features.push_back(create_scope<BaseSceneFeature>());
        m_features.push_back(create_scope<LightingFeature>());
        m_features.push_back(create_scope<PostProcessFeature>());
        m_features.push_back(create_scope<SpriteFeature>());
        m_features.push_back(create_scope<ImGuiFeature>());
        m_features.push_back(create_scope<PresentFeature>());
        m_deferred_light_constant_buffer = m_gfx_context->getDevice()->createBuffer(
            GfxBufferDesc()
                .setByteSize(256)
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(128)
                .setDebugName("DeferredLightPass ConstantBuffer")
        );
        return true;
    }

    void RenderPipeline::shutdown() {
        m_deferred_light_constant_buffer = nullptr;
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

    DrawCommandList RenderPipeline::render(RenderViewFamily& view_family, RenderScene& scene, const UInt32 swapchain_image_index) {
        initViews(scene, view_family);
        setupMeshPassContexts(scene, view_family);
        buildMeshDrawCommands(view_family);
        return buildFrameCommandList(view_family, scene, swapchain_image_index);
    }

    DrawCommandList RenderPipeline::buildFrameCommandList(
        const RenderViewFamily& view_family,
        RenderScene& scene,
        const UInt32 swapchain_image_index) const
    {
        DrawCommandList frame_command_list{};
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
            DO_DEBUG("RenderPipeline: About to compile graph for view_index={}", view_index);
            graph.compile();
            DO_DEBUG("RenderPipeline: Graph compiled for view_index={}", view_index);
            graphs.push_back(std::move(graph));
        }

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            DO_DEBUG("RenderPipeline: About to execute graph for view_index={}", view_index);
            frame_command_list.append(
                executeFrameGraph(graphs[view_index], view_family, scene,
                                view_family.getView(view_index), view_index, swapchain_image_index)
            );
            DO_DEBUG("RenderPipeline: Graph executed for view_index={}", view_index);
        }

        return frame_command_list;
    }

    DrawCommandList RenderPipeline::executeFrameGraph(
        RenderGraphBuilder& graph,
        const RenderViewFamily& view_family,
        RenderScene& scene,
        const RenderView& view,
        const Size_t view_index,
        const UInt32 swapchain_image_index) const
    {
        RenderGraphExecuteContext context{};
        context.view_family = &view_family;
        context.scene = &scene;
        context.view = &view;
        context.view_index = view_index;
        context.gfx_context = m_gfx_context;
        context.swapchain_image_index = swapchain_image_index;
        return graph.execute(*m_thread_pool, context);
    }

} // dodoe
