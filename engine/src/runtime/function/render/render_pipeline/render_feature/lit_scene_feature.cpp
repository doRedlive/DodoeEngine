// do@Redlive

#include "runtime/function/render/render_pipeline/render_feature/lit_scene_feature.h"


#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/render_pipeline/shadow/shadow_system.h"
#include "runtime/core/thread/thread_pool.h"

namespace dodoe {

    static DynamicArray<GfxVertexAttributeDesc> BuildMeshVertexAttributes() {
        constexpr Size_t kVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        constexpr Size_t kInstanceStride = sizeof(InstanceSceneData);
        return {
            GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("a_Normal").setFormat(GfxFormat::RGBA8_SNORM).setOffset(sizeof(Vector3f)).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(Vector3f) + sizeof(UInt32)).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("TEXCOORD3").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(0).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD4").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD5").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 2).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD6").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 3).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("a_InstanceColorTint").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("a_InstanceParams").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) + sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD9").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) + sizeof(Vector4f) * 2).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD10").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) + sizeof(Vector4f) * 3).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD11").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) * 2).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD12").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) * 2 + sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
        };
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

    void LitSceneFeature::initialize(SharedRenderService& resources) {
        m_shared_render_service = &resources;
    }

    void LitSceneFeature::shutdown() {
        m_shared_render_service = nullptr;
    }

    MeshPassProcessor* LitSceneFeature::getMeshProcessor() const {
        if (!m_shared_render_service || !m_shared_render_service->getMeshPassRegistry()) {
            return nullptr;
        }
        return m_shared_render_service->getMeshPassRegistry()->find(getMeshPassType());
    }

    MeshPassCommandStorage* LitSceneFeature::getCommandStorage() const {
        if (!m_shared_render_service || !m_shared_render_service->getMeshPassRegistry()) {
            return nullptr;
        }
        return m_shared_render_service->getMeshPassRegistry()->getCommandStorage(getMeshPassType());
    }

    const MeshDrawCommandCache& LitSceneFeature::getMeshDrawCache() const {
        static const MeshDrawCommandCache empty_cache{};
        const auto* storage = getCommandStorage();
        return storage ? storage->getCache() : empty_cache;
    }

    const DynamicArray<MeshDrawList>& LitSceneFeature::getLitDrawLists() const {
        static const DynamicArray<MeshDrawList> empty_lists{};
        const auto* storage = getCommandStorage();
        return storage ? storage->getDrawLists() : empty_lists;
    }

    const DynamicArray<MeshDrawGpuBucket>& LitSceneFeature::getGpuBuckets(const Size_t view_index) const {
        static const DynamicArray<MeshDrawGpuBucket> empty_buckets{};
        const auto* storage = getCommandStorage();
        return storage ? storage->getGpuBuckets(view_index) : empty_buckets;
    }

    void LitSceneFeature::setupMeshPassContexts(const RenderScene& scene,
                                                RenderViewFamily& view_family) const {
        PrimitiveSceneInfo::beginMotionFrame();
        for (auto& view : view_family.getViews()) {
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
            mesh_ext.frame_time_data = Vector4f(view_family.getTimeSeconds(),
                                                 view_family.getDeltaSeconds(), 0.0f, 0.0f);
            Size_t total_instance_count = 0;
            for (const auto* primitive : mesh_ext.visible_primitives) {
                if (primitive) {
                    primitive->advanceMotionFrame();
                }
                total_instance_count += primitive ? primitive->getInstanceCount() : 1;
            }
            mesh_ext.instance_scene_data.reserve(total_instance_count);
            mesh_ext.primitive_instance_offsets.resize(mesh_ext.visible_primitives.size());
            UInt32 instance_offset = 0;
            for (Size_t primitive_index = 0; primitive_index < mesh_ext.visible_primitives.size(); ++primitive_index) {
                const auto* primitive = mesh_ext.visible_primitives[primitive_index];
                mesh_ext.primitive_instance_offsets[primitive_index] = instance_offset;
                if (primitive) {
                    instance_offset += primitive->getInstanceCount();
                    for (const auto& inst_data : primitive->getInstanceSceneData()) {
                        mesh_ext.instance_scene_data.push_back(inst_data);
                    }
                } else {
                    instance_offset += 1;
                    InstanceSceneData inst_scene_data{};
                    mesh_ext.instance_scene_data.push_back(inst_scene_data);
                }
            }
            auto& ext = view.getOrCreateExtension<MeshViewExtension>();
            ext.primitive_mesh_pass_relevance.clear();
            ext.primitive_mesh_pass_relevance.reserve(mesh_ext.visible_primitives.size());
            for (const auto* primitive : ext.visible_primitives) {
                DO_ASSERT(primitive != nullptr, "LitSceneFeature visible primitive is null");
                ext.primitive_mesh_pass_relevance.push_back(BuildPrimitiveMeshPassRelevance(*primitive));
            }
            ext.buildMeshPassPrimitiveIndices();
        }
        ShadowSystem::SetupView(scene, view_family);
    }

    void LitSceneFeature::buildMeshDrawCommands(RenderViewFamily& view_family,
                                                DrawCommandList& cmd_list,
                                                ThreadPool* thread_pool) {
        DO_ASSERT(m_shared_render_service != nullptr, "LitSceneFeature shared render service is null");
        auto* processor = getMeshProcessor();
        DO_ASSERT(processor != nullptr, "LitSceneFeature mesh pass processor is null");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "LitSceneFeature shader library is null");
        DO_ASSERT(m_shared_render_service->getPipelineStateCache() != nullptr, "LitSceneFeature pipeline cache is null");

        const auto& shader_library = *m_shared_render_service->getShaderLibrary();

        const auto mesh_vertex_attributes = BuildMeshVertexAttributes();

        auto* input_layout_cache = m_shared_render_service->getInputLayoutCache();
        const auto mesh_input_layout = input_layout_cache
            ? input_layout_cache->getOrCreate(mesh_vertex_attributes, shader_library.getLitVertexShader())
            : GfxInputLayoutHandle{};

        auto* pso_cache = m_shared_render_service->getPipelineStateCache();
        DO_ASSERT(pso_cache != nullptr, "LitSceneFeature PSO cache is null");

        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        auto* descriptor_table = m_shared_render_service->getDescriptorTable();

        GfxBindingLayoutHandle pass_binding_layout{};
        if (usesPassBindingLayout()) {
            pass_binding_layout = binding_layout_cache->getOrCreate(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::Pixel)
                    .setRegisterSpaceIsDescriptorSet(true)
                    .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                    .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(3))
                    .addItem(GfxBindingLayoutItem::Sampler(9)));
        }

        const MeshPassPipelineContext pipeline_context{
            shader_library.getLitVertexShader(),
            getPixelShader(shader_library),
            mesh_input_layout,
            pass_binding_layout,
            descriptor_table,
            binding_layout_cache};
        auto pipeline_desc = processor->buildPipelineDescription(pipeline_context);
        const auto pipeline = pso_cache->resolveGraphicsPipeline(
            getMeshPassType(),
            pipeline_desc,
            getFramebufferInfo(),
            cmd_list);
        if (!pipeline) {
            DO_ERROR("LitSceneFeature: failed to resolve graphics pipeline");
            return;
        }
        auto* command_storage = getCommandStorage();
        if (!command_storage) {
            DO_ERROR("LitSceneFeature: mesh pass command storage is unavailable");
            return;
        }
        command_storage->prepare(pipeline, view_family.getSize());
        const auto pass_type = getMeshPassType();

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

            auto& draw_list = command_storage->beginView(view_index);

            const auto& primitive_indices =
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(pass_type)];
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
                        view.getViewProjectionMatrix(), view.getViewMatrix(), pipeline,
                        local.sources};
                    processor->buildMeshDrawCommands(context);
                });
                command_storage->mergeThreadLocal(view_index, local_storages);
            } else {
                const MeshPassCommandBuildContext context{
                    mesh_ext.visible_primitives, mesh_ext.primitive_mesh_pass_relevance,
                    primitive_indices, &mesh_ext.primitive_instance_offsets,
                    view.getViewProjectionMatrix(), view.getViewMatrix(), pipeline,
                    draw_list.sources};
                processor->buildMeshDrawCommands(context);
            }
        }
        command_storage->sort();
        command_storage->materializeSources();
    }

} // namespace dodoe
