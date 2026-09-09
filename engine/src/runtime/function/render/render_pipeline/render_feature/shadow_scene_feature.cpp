// do@Redlive

#include "shadow_scene_feature.h"


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
#include "runtime/core/thread/thread_pool.h"

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

    void ShadowSceneFeature::initialize(SharedRenderService& resources) {
        auto* gfx = resources.getGfxContext();
        auto* deletion_queue = resources.getRenderTargetSystem()
            ? resources.getRenderTargetSystem()->getDeletionQueue()
            : nullptr;
        m_shared_render_service = &resources;

        m_shadow_map = create_scope<RenderTargetHandle>();
        m_shadow_map->initialize(BuildShadowMapDesc(), *gfx, deletion_queue);

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
        m_shared_render_service = nullptr;
    }

    MeshPassProcessor* ShadowSceneFeature::getMeshProcessor() const {
        if (!m_shared_render_service || !m_shared_render_service->getMeshPassRegistry()) {
            return nullptr;
        }
        return m_shared_render_service->getMeshPassRegistry()->find(MeshPassType::Shadow);
    }

    const MeshDrawCommandCache& ShadowSceneFeature::getMeshDrawCache() const {
        static const MeshDrawCommandCache empty_cache{};
        const auto* registry = m_shared_render_service ? m_shared_render_service->getMeshPassRegistry() : nullptr;
        const auto* storage = registry ? registry->getCommandStorage(MeshPassType::Shadow) : nullptr;
        return storage ? storage->getCache() : empty_cache;
    }

    const DynamicArray<MeshDrawList>& ShadowSceneFeature::getShadowDrawLists() const {
        static const DynamicArray<MeshDrawList> empty_lists{};
        const auto* registry = m_shared_render_service ? m_shared_render_service->getMeshPassRegistry() : nullptr;
        const auto* storage = registry ? registry->getCommandStorage(MeshPassType::Shadow) : nullptr;
        return storage ? storage->getDrawLists() : empty_lists;
    }

    const DynamicArray<MeshDrawGpuBucket>& ShadowSceneFeature::getGpuBuckets(const Size_t view_index) const {
        static const DynamicArray<MeshDrawGpuBucket> empty_buckets{};
        const auto* registry = m_shared_render_service ? m_shared_render_service->getMeshPassRegistry() : nullptr;
        const auto* storage = registry ? registry->getCommandStorage(MeshPassType::Shadow) : nullptr;
        return storage ? storage->getGpuBuckets(view_index) : empty_buckets;
    }

    void ShadowSceneFeature::registerGraphImports(RenderGraphImportRegistry& imports,
                                                  const RenderView& view) {
        (void)view;
        if (m_shadow_map) {
            imports.publish<ShadowMapRenderTargetKey>(m_shadow_map.get());
        }
    }

    void ShadowSceneFeature::collectPasses(PassCollector& collector) {
        auto* processor = getMeshProcessor();
        DO_ASSERT(processor != nullptr, "ShadowSceneFeature shadow processor is null");
        collector.addPass<ShadowPass>(processor);
    }

    void ShadowSceneFeature::buildShadowDrawCommands(RenderViewFamily& view_family,
                                                     DrawCommandList& cmd_list,
                                                     ThreadPool* thread_pool) {
        DO_ASSERT(m_shared_render_service != nullptr, "ShadowSceneFeature shared render service is null");
        auto* processor = getMeshProcessor();
        DO_ASSERT(processor != nullptr, "ShadowSceneFeature mesh pass processor is null");
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
        const MeshPassPipelineContext pipeline_context{
            shader_library.getShadowVertexShader(),
            shader_library.getShadowPixelShader(),
            shadow_input_layout,
            {},
            nullptr,
            nullptr};
        const auto shadow_pipeline = pso_cache->resolveGraphicsPipeline(
            MeshPassType::Shadow,
            processor->buildPipelineDescription(pipeline_context),
            shadow_fb_info,
            cmd_list);
        if (!shadow_pipeline) {
            DO_ERROR("ShadowSceneFeature: failed to resolve shadow graphics pipeline");
            return;
        }
        auto* command_storage = m_shared_render_service->getMeshPassRegistry()->getCommandStorage(MeshPassType::Shadow);
        if (!command_storage) {
            DO_ERROR("ShadowSceneFeature: mesh pass definition is unavailable");
            return;
        }
        command_storage->prepare(shadow_pipeline, view_family.getSize());

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

            auto& shadow_list = command_storage->beginView(view_index);

            const auto& primitive_indices =
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(MeshPassType::Shadow)];
            const Size_t chunk_size = 64;
            const Size_t chunk_count = (primitive_indices.size() + chunk_size - 1) / chunk_size;
            if (thread_pool && chunk_count > 1) {
                DynamicArray<MeshPassThreadLocalCommandStorage> local_storages;
                local_storages.resize(chunk_count);
                thread_pool->parallelFor(chunk_count, [&](const Size_t chunk_index) {
                    const Size_t begin = chunk_index * chunk_size;
                    const Size_t end = std::min(begin + chunk_size, primitive_indices.size());
                    DynamicArray<UInt32> chunk_indices(
                        primitive_indices.begin() + begin, primitive_indices.begin() + end);
                    auto& local = local_storages[chunk_index];
                    const MeshPassCommandBuildContext context{
                        mesh_ext.visible_primitives, mesh_ext.primitive_mesh_pass_relevance,
                        chunk_indices, &mesh_ext.primitive_instance_offsets,
                        mesh_ext.directional_shadow_view_projection, view.getViewMatrix(),
                        shadow_pipeline, local.sources};
                    processor->buildMeshDrawCommands(context);
                });
                command_storage->mergeThreadLocal(view_index, local_storages);
            } else {
                const MeshPassCommandBuildContext context{
                    mesh_ext.visible_primitives, mesh_ext.primitive_mesh_pass_relevance,
                    primitive_indices, &mesh_ext.primitive_instance_offsets,
                    mesh_ext.directional_shadow_view_projection, view.getViewMatrix(),
                    shadow_pipeline, shadow_list.sources};
                processor->buildMeshDrawCommands(context);
            }
        }
        command_storage->sort();
        command_storage->materializeSources();
    }

} // namespace dodoe
