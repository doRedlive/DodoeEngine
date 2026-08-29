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
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/core/math/math.h"

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
        };
    }

    static GfxGraphicsPipelineDesc MakeLitPipelineDesc(const GfxShaderHandle& vertex_shader,
                                                       const GfxShaderHandle& pixel_shader,
                                                       GfxInputLayoutHandle mesh_input_layout,
                                                       const GfxBindingLayoutHandle& global_binding_layout,
                                                       const GfxBindingLayoutHandle& view_binding_layout,
                                                       const GfxBindingLayoutHandle& primitive_binding_layout,
                                                       const GfxBindingLayoutHandle& sampler_binding_layout,
                                                       const GfxBindingLayoutHandle& pass_binding_layout,
                                                       DescriptorTableManager* descriptor_table,
                                                       BindingLayoutCache* binding_layout_cache) {
        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(vertex_shader)
            .setPixelShader(pixel_shader)
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
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state);
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

    void LitSceneFeature::initialize(SharedRenderService& resources) {
        m_shared_render_service = &resources;

        GfxBindingSetHandle descriptor_binding_set{};
        auto* descriptor_table = resources.getDescriptorTable();
        if (descriptor_table && descriptor_table->getDescriptorTable()) {
            descriptor_binding_set = create_ref<GfxBindingSet>(
                cutie::BindingSetHandle(descriptor_table->getDescriptorTable()));
        }
        auto* binding_layout_cache = resources.getBindingLayoutCache();
        auto* binding_set_cache = resources.getBindingSetCache();
        DO_ASSERT(binding_layout_cache != nullptr, "LitSceneFeature binding layout cache is null");
        DO_ASSERT(binding_set_cache != nullptr, "LitSceneFeature binding set cache is null");
        m_lit_processor = create_scope<LitMeshProcessor>(
            getMeshPassType(), descriptor_binding_set, *binding_layout_cache, *binding_set_cache);
    }

    void LitSceneFeature::shutdown() {
        if (m_lit_processor) {
            m_lit_processor->reset();
            m_lit_processor.reset();
        }
    }

    void LitSceneFeature::setupMeshPassContexts(const RenderScene& scene,
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
                DO_ASSERT(primitive != nullptr, "LitSceneFeature visible primitive is null");
                ext.primitive_mesh_pass_relevance.push_back(BuildPrimitiveMeshPassRelevance(*primitive));
            }
            ext.buildMeshPassPrimitiveIndices();
        }
    }

    void LitSceneFeature::buildMeshDrawCommands(RenderViewFamily& view_family,
                                                DrawCommandList& cmd_list) {
        DO_ASSERT(m_shared_render_service != nullptr, "LitSceneFeature shared render service is null");
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
                    .addItem(GfxBindingLayoutItem::Sampler(9)));
        }

        auto pipeline_desc = MakeLitPipelineDesc(
            shader_library.getLitVertexShader(),
            getPixelShader(shader_library),
            mesh_input_layout,
            m_lit_processor->getGlobalBindingLayout(),
            m_lit_processor->getViewBindingLayout(),
            m_lit_processor->getPrimitiveBindingLayout(),
            m_lit_processor->getSamplerBindingLayout(),
            pass_binding_layout,
            descriptor_table,
            binding_layout_cache);
        modifyPipelineDesc(pipeline_desc);

        (void)pso_cache->resolveGraphicsPipeline(
            getMeshPassType(),
            pipeline_desc,
            getFramebufferInfo(),
            cmd_list);
        m_draw_lists.resize(view_family.getSize());
        const auto pass_type = getMeshPassType();

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            auto& view = view_family.getView(view_index);
            auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

            auto& draw_list = m_draw_lists[view_index];
            draw_list.reset();
            draw_list.cached_commands = &m_mesh_draw_cache.getCommands();

            m_lit_processor->buildCachedCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(pass_type)],
                view.getViewProjectionMatrix(),
                m_mesh_draw_cache,
                draw_list.cached_instances,
                draw_list.cached_shader_data
            );
            m_lit_processor->buildDynamicCommands(
                mesh_ext.visible_primitives,
                mesh_ext.primitive_mesh_pass_relevance,
                mesh_ext.mesh_pass_primitive_indices[static_cast<size_t>(pass_type)],
                view.getViewProjectionMatrix(),
                draw_list.frame_commands,
                draw_list.dynamic_instances,
                draw_list.dynamic_shader_data
            );
        }
    }

} // namespace dodoe
