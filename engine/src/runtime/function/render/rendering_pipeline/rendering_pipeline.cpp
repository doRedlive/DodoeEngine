// do@Redlive

#include "rendering_pipeline.h"

#include "passes/render_mesh_passes.h"
#include "passes/render_lighting_passes.h"
#include "passes/render_post_process_passes.h"
#include "rendering_pipeline_pass_utils.h"
#include "scene_visibility.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/common.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/view_mesh_draw_context.h"
#include "runtime/function/render/render_object.h"

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

    RenderingPassContext RenderingPipeline::buildPassContext() const {
        RenderingPassContext context{};
        context.gfx_context = m_gfx_context;
        context.shader_library = m_shader_library.get();
        context.pipeline_state_cache = m_pipeline_state_cache.get();
        context.fullscreen_pass_shared_state = m_fullscreen_pass_shared_state.get();
        context.gbuffer_mesh_processor = m_gbuffer_mesh_processor.get();
        context.directional_shadow_mesh_processor = m_directional_shadow_mesh_processor.get();
        return context;
    }

    void RenderingPipeline::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
        for (auto& view : view_family.getViews()) {
            view.resetFrameData();
        }
        scene_visibility::BuildVisiblePrimitiveSets(scene, view_family);
    }

    void RenderingPipeline::setupMeshPassRelevance(ViewMeshDrawContext& view_context) const {
        view_context.primitive_mesh_pass_relevance.clear();
        view_context.primitive_mesh_pass_relevance.reserve(view_context.visible_primitives.size());

        for (const auto* primitive : view_context.visible_primitives) {
            DO_ASSERT(primitive != nullptr, "RenderingPipeline visible primitive is null");
            view_context.primitive_mesh_pass_relevance.push_back(BuildPrimitiveMeshPassRelevance(*primitive));
        }

        view_context.buildMeshPassPrimitiveIndices();
    }

    void RenderingPipeline::setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const {
        const auto& directional_lights = scene.getDirectionalLights();
        const Matrix4f directional_light_view_projection = rendering_pipeline_utils::BuildDirectionalLightViewProjection(
            directional_lights.empty() ? Vector3f(0.3f, -0.8f, -0.5f) : directional_lights[0].direction
        );

        for (auto& view : view_family.getViews()) {
            auto& view_context = view.getMeshDrawContext();
            auto visible_primitives = std::move(view_context.visible_primitives);
            view_context.reset();
            view_context.visible_primitives = std::move(visible_primitives);
            view_context.frame_time_data = Vector4f(view_family.getTimeSeconds(), view_family.getDeltaSeconds(), 0.0f, 0.0f);
            view_context.primitive_first_instance_offsets.reserve(view_context.visible_primitives.size());
            Size_t total_instance_count = 0;
            for (const auto* primitive : view_context.visible_primitives) {
                const auto* render_object = primitive ? primitive->getRenderObject() : nullptr;
                total_instance_count += render_object ? render_object->getInstanceCount() : 1;
            }
            view_context.instance_scene_data.reserve(total_instance_count);
            for (const auto* primitive : view_context.visible_primitives) {
                view_context.primitive_first_instance_offsets.push_back(static_cast<UInt32>(view_context.instance_scene_data.size()));
                const auto* render_object = primitive ? primitive->getRenderObject() : nullptr;
                if (render_object) {
                    render_object->appendInstanceSceneData(view_context.instance_scene_data, primitive->getWorldTransform());
                } else {
                    InstanceSceneData instance_scene_data{};
                    instance_scene_data.model = primitive ? primitive->getWorldTransform() : Matrix4f(1.0f);
                    view_context.instance_scene_data.push_back(instance_scene_data);
                }
            }
            view_context.directional_shadow_view_projection = directional_light_view_projection;
            setupMeshPassRelevance(view_context);
        }
    }

    void RenderingPipeline::buildMeshDrawCommands(RenderViewFamily& view_family) const {
        DO_ASSERT(m_shader_library != nullptr, "RenderingPipeline shader library is null");
        DO_ASSERT(m_local_vertex_factory != nullptr, "RenderingPipeline local vertex factory is null");
        DO_ASSERT(m_gbuffer_mesh_processor != nullptr, "RenderingPipeline GBuffer mesh processor is null");
        DO_ASSERT(m_directional_shadow_mesh_processor != nullptr, "RenderingPipeline directional shadow mesh processor is null");

        const auto& shader_library = *m_shader_library;
        const auto& local_vertex_factory = *m_local_vertex_factory;
        const auto& gbuffer_mesh_processor = *m_gbuffer_mesh_processor;
        const auto& directional_shadow_mesh_processor = *m_directional_shadow_mesh_processor;
        auto build_gbuffer_framebuffer_info = []() {
            nvrhi::FramebufferInfo framebuffer_info{};
            framebuffer_info
                .addColorFormat(Format::RGBA8_UNORM)
                .addColorFormat(Format::RGBA16_FLOAT)
                .addColorFormat(Format::RGBA32_FLOAT)
                .addColorFormat(Format::RGBA8_UNORM)
                .setDepthFormat(Format::D32);
            return framebuffer_info;
        };
        auto build_shadow_framebuffer_info = []() {
            nvrhi::FramebufferInfo framebuffer_info{};
            framebuffer_info.setDepthFormat(Format::D32);
            return framebuffer_info;
        };
        auto build_gbuffer_pipeline_desc = [this, &shader_library, &local_vertex_factory, &gbuffer_mesh_processor]() {
            auto pipeline_desc = GraphicsPipelineDesc()
                .setVertexShader(shader_library.getGBufferVertexShader())
                .setPixelShader(shader_library.getGBufferPixelShader())
                .setInputLayout(local_vertex_factory.getGBufferInputLayout())
                .addBindingLayout(gbuffer_mesh_processor.getBindingLayout())
                .setPrimType(PrimitiveType::TriangleList);
            if (m_descriptor_table && m_descriptor_table->getDescriptorTable()) {
                pipeline_desc.addBindingLayout(m_descriptor_table->getDescriptorTable()->getLayout());
            }
            DepthStencilState depth_stencil_state;
            depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(ComparisonFunc::Less).disableStencil();
            RenderState render_state;
            render_state.setDepthStencilState(depth_stencil_state);
            pipeline_desc.setRenderState(render_state);
            return pipeline_desc;
        };
        auto build_shadow_pipeline_desc = [&shader_library, &local_vertex_factory, &directional_shadow_mesh_processor]() {
            auto pipeline_desc = GraphicsPipelineDesc()
                .setVertexShader(shader_library.getShadowVertexShader())
                .setPixelShader(shader_library.getShadowPixelShader())
                .setInputLayout(local_vertex_factory.getShadowInputLayout())
                .addBindingLayout(directional_shadow_mesh_processor.getBindingLayout())
                .setPrimType(PrimitiveType::TriangleList);
            DepthStencilState depth_stencil_state;
            depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(ComparisonFunc::Less).disableStencil();
            RasterState raster_state;
            raster_state.setCullBack().setDepthBiasClamp(0.0f).setDepthBias(6).setSlopeScaleDepthBias(1.5f);
            RenderState render_state;
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

        DO_ASSERT(m_pipeline_state_cache != nullptr, "RenderingPipeline PipelineStateCache is null");
        const auto gbuffer_pipeline = m_pipeline_state_cache->resolveGraphicsPipeline(
            MeshPassType::GBuffer,
            build_gbuffer_pipeline_desc(),
            build_gbuffer_framebuffer_info()
        );
        const auto shadow_pipeline = m_pipeline_state_cache->resolveGraphicsPipeline(
            MeshPassType::DirectionalShadow,
            build_shadow_pipeline_desc(),
            build_shadow_framebuffer_info()
        );

        for (Size_t view_index = 0; view_index < view_family.size(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& view_context = view.getMeshDrawContext();
            gbuffer_mesh_processor.buildCommands(
                view_context,
                view,
                view_context
            );
            assign_pipeline(view_context.getMeshPassCommands(MeshPassType::GBuffer), gbuffer_pipeline);
            directional_shadow_mesh_processor.buildCommands(
                view_context,
                view_context.directional_shadow_view_projection,
                view_context
            );
            assign_pipeline(view_context.getMeshPassCommands(MeshPassType::DirectionalShadow), shadow_pipeline);
        }
    }

    Bool RenderingPipeline::initialize(const RenderingPipelineCreateInfo& info) {
        Size_t worker_count = info.worker_count;
        if (worker_count == 0) {
            worker_count = std::thread::hardware_concurrency();
        }

        m_thread_pool = create_scope<ThreadPool>(std::max(Size_t{1}, worker_count));
        m_gfx_context = info.gfx_context;
        m_descriptor_table = info.descriptor_table;
        m_texture_manager = info.texture_manager;
        DO_ASSERT(m_gfx_context != nullptr, "RenderingPipeline requires valid gfx_context");
        m_shader_library = create_scope<ShaderLibrary>();
        m_shader_library->initialize(*m_gfx_context);
        m_local_vertex_factory = create_scope<LocalVertexFactory>();
        m_local_vertex_factory->initialize(
            *m_gfx_context,
            m_shader_library->getGBufferVertexShader(),
            m_shader_library->getShadowVertexShader()
        );
        m_gbuffer_mesh_processor = create_scope<GBufferMeshProcessor>();
        m_gbuffer_mesh_processor->initialize(*m_gfx_context, m_descriptor_table, m_texture_manager);
        m_directional_shadow_mesh_processor = create_scope<DirectionalShadowMeshProcessor>();
        m_directional_shadow_mesh_processor->initialize(*m_gfx_context);
        m_pipeline_state_cache = create_scope<PipelineStateCache>(m_gfx_context->getDevice());
        m_fullscreen_pass_shared_state = create_scope<FullscreenPassSharedState>();
        m_fullscreen_pass_shared_state->initialize(*m_gfx_context);
        return true;
    }

    void RenderingPipeline::shutdown() {
        if (m_fullscreen_pass_shared_state) {
            m_fullscreen_pass_shared_state->reset();
            m_fullscreen_pass_shared_state.reset();
        }
        if (m_pipeline_state_cache) {
            m_pipeline_state_cache->clear();
            m_pipeline_state_cache.reset();
        }
        if (m_directional_shadow_mesh_processor) {
            m_directional_shadow_mesh_processor->reset();
            m_directional_shadow_mesh_processor.reset();
        }
        if (m_gbuffer_mesh_processor) {
            m_gbuffer_mesh_processor->reset();
            m_gbuffer_mesh_processor.reset();
        }
        if (m_local_vertex_factory) {
            m_local_vertex_factory->reset();
            m_local_vertex_factory.reset();
        }
        if (m_shader_library) {
            m_shader_library->reset();
            m_shader_library.reset();
        }
        m_texture_manager = nullptr;
        m_descriptor_table = nullptr;
        m_gfx_context = nullptr;
        m_thread_pool.reset();
    }

    DrawCommandList RenderingPipeline::render(RenderViewFamily& view_family, RenderScene& scene, const UInt32 swapchain_image_index) {
        initViews(scene, view_family);
        setupMeshPassContexts(scene, view_family);
        buildMeshDrawCommands(view_family);
        return buildFrameCommandList(view_family, scene, swapchain_image_index);
    }

    DrawCommandList RenderingPipeline::buildFrameCommandList(
        const RenderViewFamily& view_family,
        RenderScene& scene,
        const UInt32 swapchain_image_index) const
    {
        DrawCommandList frame_command_list{};
        const auto pass_context = buildPassContext();
        for (Size_t view_index = 0; view_index < view_family.size(); view_index++) {
            RenderGraphBuilder graph{};
            const auto& view = view_family.getView(view_index);
            RenderingPipelinePasses::AddMeshGraphPasses(graph, view, pass_context);
            RenderingPipelinePasses::AddLightingGraphPasses(graph, pass_context);
            RenderingPipelinePasses::AddPostProcessGraphPasses(graph, pass_context);
            RenderingPipelinePasses::AddPresentGraphPass(graph, pass_context);
            graph.compile();
            frame_command_list.append(
                executeFrameGraph(graph, view_family, scene, view, view_index, swapchain_image_index)
            );
        }
        return frame_command_list;
    }

    DrawCommandList RenderingPipeline::executeFrameGraph(
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
