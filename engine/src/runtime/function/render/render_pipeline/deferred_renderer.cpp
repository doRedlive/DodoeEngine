// do@Redlive

#include "deferred_renderer.h"

#include "render_graph/render_graph_builder.h"
#include "passes/render_pipeline_passes.h"
#include "render_pipeline_pass_utils.h"
#include "render_feature/render_builtin_features.h"
#include "render_feature/sprite_feature.h"
#include "render_feature/imgui_feature.h"
#ifdef DODOE_EDITOR_ENABLED
#include "render_feature/gizmo_feature.h"
#endif
#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command.h"
#include "runtime/function/render/render_scene/render_object.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/common.h"

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

    Bool DeferredRenderer::initialize(const RendererCreateInfo& info) {
        if (!initializeBase(info)) {
            return false;
        }

        const auto* shader_library = m_shared_render_service->getShaderLibrary();

        m_local_vertex_factory = create_scope<LocalVertexFactory>();
        m_local_vertex_factory->initialize(
            GDrawCommandList,
            shader_library->getGBufferVertexShader(),
            shader_library->getShadowVertexShader()
        );

        m_gpu_culling = GpuCulling::Create({m_gfx_context, const_cast<ShaderLibrary*>(shader_library)});

        m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)] = create_scope<GBufferMeshProcessor>();
        static_cast<GBufferMeshProcessor*>(m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)].get())->initialize(*m_gfx_context, m_shared_render_service->getDescriptorTable(), m_shared_render_service->getTextureManager());
        m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)] = create_scope<DirectionalShadowMeshProcessor>();
        static_cast<DirectionalShadowMeshProcessor*>(m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)].get())->initialize(*m_gfx_context);

        m_features.push_back(create_scope<BaseSceneFeature>());
        m_features.push_back(create_scope<LightingFeature>());
        m_features.push_back(create_scope<PostProcessFeature>());
        m_features.push_back(create_scope<SpriteFeature>());
#ifdef DODOE_EDITOR_ENABLED
        m_features.push_back(create_scope<GizmoFeature>());
#endif
        m_features.push_back(create_scope<ImGuiFeature>());
        m_features.push_back(create_scope<PresentFeature>());
        return true;
    }

    void DeferredRenderer::shutdown() {
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
        GpuCulling::Destroy(m_gpu_culling);
        shutdownBase();
    }

    RenderPassContext DeferredRenderer::buildPassContext(const RenderScene& scene) const {
        auto context = RendererBase::buildPassContext(scene);
        context.local_vertex_factory = m_local_vertex_factory.get();
        for (size_t i = 0; i < static_cast<size_t>(MeshPassType::Count); ++i) {
            context.mesh_processors[i] = m_mesh_processors[i].get();
        }
        return context;
    }

    void DeferredRenderer::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
        RendererBase::initViews(scene, view_family);
        view_family.buildVisiblePrimitives(scene);
    }

    void DeferredRenderer::render(RenderViewFamily& view_family, RenderScene& scene,
                                   const UInt32 swapchain_image_index, DrawCommandList& out_commands) {
        initViews(scene, view_family);
        setupMeshPassContexts(scene, view_family);

        const auto culling_path = RenderSettings::GetFeatureSettings().culling_path;
        if (culling_path == CullingPath::GpuOnly || culling_path == CullingPath::CpuThenGpuVerify) {
            executeGpuCulling(view_family, scene, out_commands);
            buildGpuDrivenDrawCommands(scene, view_family, out_commands);
        }

        if (culling_path == CullingPath::CpuOnly || culling_path == CullingPath::CpuThenGpuVerify) {
            buildMeshDrawCommands(view_family, out_commands);
        }

        buildFrameDrawCommandList(view_family, scene, swapchain_image_index, out_commands);
    }

    void DeferredRenderer::executeGpuCulling(RenderViewFamily& view_family, RenderScene& scene, DrawCommandList& cmd_list) const {
        if (!m_gpu_culling || !m_gpu_culling->isEnabled()) {
            return;
        }

        auto* gpu_scene = scene.getGpuScene();
        if (!gpu_scene) {
            return;
        }

        const auto scene_resources = gpu_scene->getPassResources();
        const UInt32 object_count = gpu_scene->getObjectCount();

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            const auto& view = view_family.getView(view_index);
            m_gpu_culling->executeCulling(cmd_list, scene_resources,
                                          view.getViewProjectionMatrix(), object_count);
            m_gpu_culling->executeBucketBuild(cmd_list, scene_resources, object_count);
        }
    }

    void DeferredRenderer::setupMeshPassRelevance(RenderView& view) const {
        auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
        mesh_ext.primitive_mesh_pass_relevance.clear();
        mesh_ext.primitive_mesh_pass_relevance.reserve(mesh_ext.visible_primitives.size());

        for (const auto* primitive : mesh_ext.visible_primitives) {
            DO_ASSERT(primitive != nullptr, "DeferredRenderer visible primitive is null");
            mesh_ext.primitive_mesh_pass_relevance.push_back(BuildPrimitiveMeshPassRelevance(*primitive));
        }

        mesh_ext.buildMeshPassPrimitiveIndices();
    }

    void DeferredRenderer::setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const {
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
            setupMeshPassRelevance(view);
        }
    }

    void DeferredRenderer::buildMeshDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list) const {
        DO_ASSERT(m_shared_render_service != nullptr, "DeferredRenderer shared render service is null");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "DeferredRenderer shader library is null");
        DO_ASSERT(m_shared_render_service->getPipelineStateCache() != nullptr, "DeferredRenderer pipeline cache is null");
        DO_ASSERT(m_local_vertex_factory != nullptr, "DeferredRenderer local vertex factory is null");
        DO_ASSERT(m_mesh_processors[static_cast<size_t>(MeshPassType::GBuffer)] != nullptr, "DeferredRenderer GBuffer mesh processor is null");
        DO_ASSERT(m_mesh_processors[static_cast<size_t>(MeshPassType::DirectionalShadow)] != nullptr, "DeferredRenderer directional shadow mesh processor is null");

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
        auto* pso_cache = m_shared_render_service->getPipelineStateCache();
        DO_ASSERT(pso_cache != nullptr, "DeferredRenderer PSO cache is null");

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
            mesh_ext.cached_commands = &m_mesh_draw_cache.getCommands();

            gbuffer_mesh_processor.buildCachedCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::GBuffer)],
                view.getViewProjectionMatrix(),
                m_mesh_draw_cache,
                mesh_ext.cached_draw_instances[static_cast<size_t>(MeshPassType::GBuffer)],
                mesh_ext.gbuffer_shader_data
            );
            gbuffer_mesh_processor.buildDynamicCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::GBuffer)],
                view.getViewProjectionMatrix(),
                mesh_ext.frame_commands,
                mesh_ext.dynamic_draw_instances[static_cast<size_t>(MeshPassType::GBuffer)],
                mesh_ext.dynamic_shader_data
            );
            directional_shadow_mesh_processor.buildCachedCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                mesh_ext.directional_shadow_view_projection,
                m_mesh_draw_cache,
                mesh_ext.cached_draw_instances[static_cast<size_t>(MeshPassType::DirectionalShadow)]
            );
            directional_shadow_mesh_processor.buildDynamicCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                mesh_ext.directional_shadow_view_projection,
                mesh_ext.frame_commands,
                mesh_ext.dynamic_draw_instances[static_cast<size_t>(MeshPassType::DirectionalShadow)]
            );
        }
    }

    void DeferredRenderer::buildGpuDrivenDrawCommands(const RenderScene& scene, RenderViewFamily& view_family, DrawCommandList& cmd_list) const {
        if (!m_gpu_culling || !m_gpu_culling->isEnabled()) {
            return;
        }

        auto* gpu_scene = scene.getGpuScene();
        if (!gpu_scene) {
            return;
        }

        const auto indirect_args = m_gpu_culling->getIndirectArgsBuffer();
        if (!indirect_args) {
            return;
        }

        const UInt32 object_count = gpu_scene->getObjectCount();

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

            const auto viewport = GfxViewportState()
                .setViewport(view.getViewportWidth(), view.getViewportHeight());

            for (Size_t pass_idx = 0; pass_idx < static_cast<Size_t>(MeshPassType::Count); pass_idx++) {
                const auto& instances = mesh_ext.cached_draw_instances[pass_idx];
                if (instances.empty()) {
                    continue;
                }

                const auto& cached_cmd = m_mesh_draw_cache.getCommand(instances[0].cmd_index);

                auto graphics_state = GfxGraphicsState()
                    .setViewport(viewport)
                    .setPipeline(cached_cmd.pipeline->getRHIHandle());

                for (const auto& binding_set : cached_cmd.binding_sets) {
                    if (binding_set && binding_set->isRHIReady()) {
                        graphics_state.addBindingSet(binding_set->getRHIHandle());
                    }
                }

                for (const auto& vertex_binding : cached_cmd.vertex_bindings) {
                    graphics_state.addVertexBuffer(vertex_binding);
                }

                const auto gpu_scene_resources = gpu_scene->getPassResources();
                if (gpu_scene_resources.primitive_instance && gpu_scene_resources.primitive_instance->getRHI()) {
                    graphics_state.addVertexBuffer(
                        GfxVertexBufferBinding()
                            .setBuffer(gpu_scene_resources.primitive_instance->getRHI())
                            .setSlot(1)
                            .setOffset(instances[0].instance_offset)
                    );
                }

                graphics_state.setIndexBuffer(cached_cmd.index_binding);
                cmd_list.setGraphicsState(graphics_state);

                cmd_list.setBufferState(indirect_args, GfxResourceStates::IndirectArgument);
                cmd_list.commitBarriers();
                cmd_list.drawIndexedIndirect(0, object_count);
            }
        }
    }

} // dodoe
