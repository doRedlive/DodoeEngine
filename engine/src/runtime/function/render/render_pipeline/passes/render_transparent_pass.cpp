// do@Redlive

#include "render_transparent_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"

#include "runtime/function/render/render_frame/frame_staging_allocator.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_pipeline/render_feature/lit_scene_feature.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/mesh_draw/lit_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/core/math/math.h"

#include <cstring>

namespace dodoe {

    struct TransparentPassParameters {
        RenderGraphTextureHandle hdr_color{};
        RenderGraphTextureHandle depth{};
        RenderGraphTextureHandle shadow_map{};
        RenderGraphTextureHandle skybox_texture{};
        RenderGraphBufferHandle primitive_scene_buffer{};
    };

    void TransparentPass::build(RenderGraphBuilder& graph,
                                const RenderPassBuildContext& context) {
        auto* binding_layout_cache = context.shared_render_service->getBindingLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "TransparentPass binding layout cache is null");

        const auto binding_layout = MakeLitPassBindingLayout(*binding_layout_cache);

        graph.addPass<TransparentPassParameters>(
            "TransparentPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, TransparentPassParameters& parameters) {
                const auto* hdr = pass_builder.blackboard().get<SceneHdrKey>();
                const auto* shadow = pass_builder.blackboard().get<ShadowMapKey>();
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey>();
                DO_ASSERT(hdr && shadow && scene_textures, "TransparentPass blackboard resources are missing");

                RenderGraphAttachmentInfo hdr_attachment{};
                hdr_attachment.load_op = LoadOp::Load;
                parameters.hdr_color = pass_builder.writeColor(*hdr, hdr_attachment);
                parameters.shadow_map = pass_builder.read(*shadow);
                parameters.depth = pass_builder.read(scene_textures->depth);
                parameters.primitive_scene_buffer = pass_builder.read(scene_textures->instance_scene_data);

                if (context.scene) {
                    for (const auto& light_info : context.scene->getLightSceneInfos()) {
                        if (light_info.getLightType() != LightType::Sky || !light_info.isEnabled()) {
                            continue;
                        }
                        const auto cubemap = light_info.getSkyLightData().cubemap;
                        if (cubemap && cubemap->getGpuHandle()) {
                            parameters.skybox_texture = pass_builder.read(pass_builder.importTexture(
                                cubemap->getGpuHandle(), "TransparentSkyboxCubemap"));
                        }
                        break;
                    }
                }
            },
            [this, binding_layout](const TransparentPassParameters& parameters,
                                   const RenderGraphPassContext& ctx,
                                   DrawCommandList& command_list) {
                DO_ASSERT(ctx.getView() != nullptr, "TransparentPass view is null");

                auto* feature = static_cast<LitSceneFeature*>(m_owning_feature);
                DO_ASSERT(feature != nullptr, "TransparentPass owning feature is null");
                auto* processor = feature->getLitProcessor();
                if (!processor) {
                    DO_ERROR("TransparentPass lit processor is null");
                    return;
                }

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                    *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());

                const auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();
                if (!mesh_ext) {
                    return;
                }
                const auto resolved_psb = ctx.resolveBuffer(parameters.primitive_scene_buffer);

                const GlobalMeshShaderData global_data{mesh_ext->frame_time_data};
                command_list.writeBuffer(processor->getGlobalConstantBuffer(), &global_data, sizeof(global_data));
                const ViewMeshShaderData view_data{ctx.getView()->getViewProjectionMatrix()};
                command_list.writeBuffer(processor->getViewConstantBuffer(), &view_data, sizeof(view_data));

                const auto camera_position = rendering_pipeline_utils::ExtractCameraPosition(*ctx.getView());
                LitPassConstantBuffer pass_cb{};
                BuildLitPassConstantBuffer(pass_cb, *ctx.getScene(), camera_position);

                auto* staging = ctx.getFrameStagingAllocator();
                if (!staging) {
                    DO_ERROR("TransparentPass: frame staging allocator is null");
                    return;
                }
                const auto allocation = staging->allocate(kLitPassConstantBufferSize);
                if (!allocation.buffer || !allocation.mapped_data) {
                    DO_ERROR("TransparentPass: unable to allocate pass constant buffer");
                    return;
                }
                std::memset(allocation.mapped_data, 0, static_cast<Size_t>(allocation.size));
                std::memcpy(allocation.mapped_data, &pass_cb, sizeof(pass_cb));

                const auto shadow_handle = ctx.resolveTexture(parameters.shadow_map);
                GfxTextureHandle skybox_texture{};
                if (parameters.skybox_texture.isValid()) {
                    skybox_texture = ctx.resolveTexture(parameters.skybox_texture);
                } else if (const auto* fallback_cubemap = ctx.getTextureManager()->getFallbackCubemap()) {
                    skybox_texture = fallback_cubemap->getGpuHandle();
                }

                const auto pass_binding_set = CreateLitPassBindingSet(
                    command_list, allocation, shadow_handle, skybox_texture, binding_layout);
                if (!pass_binding_set) {
                    DO_ERROR("TransparentPass: failed to create pass binding set");
                    return;
                }

                const auto fb = ctx.getFramebuffer();
                const auto& draw_list = feature->getLitDrawLists()[ctx.getViewIndex()];
                SubmitMeshDrawCommands(draw_list.cached_instances, *draw_list.cached_commands,
                    draw_list.cached_shader_data, processor->getPrimitiveConstantBuffer(),
                    fb, viewport_state, resolved_psb, &pass_binding_set, command_list);
                SubmitMeshDrawCommands(draw_list.dynamic_instances, draw_list.frame_commands,
                    draw_list.dynamic_shader_data, processor->getPrimitiveConstantBuffer(),
                    fb, viewport_state, resolved_psb, &pass_binding_set, command_list);
            });
    }

} // namespace dodoe
