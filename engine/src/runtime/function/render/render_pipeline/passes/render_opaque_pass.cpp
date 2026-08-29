#include "runtime/function/render/render_pipeline/passes/render_opaque_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"

#include "runtime/function/render/render_frame/frame_staging_allocator.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_pipeline/render_feature/lit_scene_feature.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/mesh_draw/lit_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/core/math/math.h"

#include <cstring>

namespace dodoe {

    static_assert(sizeof(LitPassConstantBuffer) <= kLitPassConstantBufferSize);

    void BuildLitPassConstantBuffer(LitPassConstantBuffer& pass_cb,
                                                const RenderScene& scene,
                                                const Vector3f& camera_position) {
        pass_cb.camera_position = Vector4f(camera_position, 0.0f);
        UInt32 point_count = 0;
        for (const auto& light_info : scene.getLightSceneInfos()) {
            if (!light_info.isEnabled()) {
                continue;
            }
            switch (light_info.getLightType()) {
            case LightType::Directional: {
                const auto& data = light_info.getDirectionalLightData();
                pass_cb.directional_color_intensity = Vector4f(data.color, data.irradiance);
                pass_cb.directional_direction_flags = Vector4f(Math::Normalize(data.direction), 0.0f);
                pass_cb.dir_light_view_projection = rendering_pipeline_utils::BuildDirectionalLightViewProjection(data.direction);
                pass_cb.shadow_params = Vector4f(0.005f, 0.2f, 0.005f, 2.0f);
                break;
            }
            case LightType::Point: {
                if (point_count >= 4) {
                    break;
                }
                const auto& data = light_info.getPointLightData();
                const Vector3f light_position = Vector3f(light_info.getWorldTransform()[3]);
                pass_cb.point_light_positions[point_count] = Vector4f(light_position, data.range);
                pass_cb.point_light_colors[point_count] = Vector4f(data.color, data.intensity);
                point_count++;
                break;
            }
            default:
                break;
            }
        }
        pass_cb.light_count_flags.x = static_cast<Float>(point_count);
    }

    GfxBindingLayoutHandle MakeLitPassBindingLayout(BindingLayoutCache& binding_layout_cache) {
        return binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                .addItem(GfxBindingLayoutItem::Sampler(9)));
    }

    GfxBindingSetHandle CreateLitPassBindingSet(DrawCommandList& command_list,
                                                            const FrameStagingAllocator::Allocation& allocation,
                                                            const GfxTextureHandle& shadow_handle,
                                                            const GfxTextureHandle& skybox_texture,
                                                            const GfxBindingLayoutHandle& binding_layout) {
        return command_list.createBindingSet(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::ConstantBuffer(
                    0, allocation.buffer->getRHIHandle().Get(),
                    GfxBufferRange(allocation.offset, allocation.size)))
                .addItem(GfxBindingSetItem::Texture_SRV(1, shadow_handle->getRHIHandle().Get()))
                .addItem(GfxBindingSetItem::Texture_SRV(
                    2, skybox_texture ? skybox_texture->getRHIHandle().Get() : nullptr,
                    GfxFormat::UNKNOWN, GfxAllSubresources, GfxTextureDimension::TextureCube))
                .addItem(GfxBindingSetItem::Sampler(9, GlobalSamplers::screen().Get())),
            binding_layout);
    }

    struct OpaquePassParameters {
        RenderGraphTextureHandle hdr_color{};
        RenderGraphTextureHandle depth{};
        RenderGraphTextureHandle shadow_map{};
        RenderGraphTextureHandle skybox_texture{};
        RenderGraphBufferHandle primitive_scene_buffer{};
    };

    void OpaquePass::build(RenderGraphBuilder& graph,
                           const RenderPassBuildContext& context) {
        auto* binding_layout_cache = context.shared_render_service->getBindingLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "OpaquePass binding layout cache is null");

        const auto binding_layout = MakeLitPassBindingLayout(*binding_layout_cache);

        graph.addPass<OpaquePassParameters>(
            "OpaquePass",
            RenderGraphPassFlags::Raster,
            [&context, view = &context.view, imports = context.graph_imports]
            (RenderGraphPassBuilder& pass_builder, OpaquePassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2D();
                const auto* mesh_ext = view->getExtension<MeshViewExtension>();
                const Size_t visible_instance_count = mesh_ext ? mesh_ext->instance_scene_data.size() : 0;

                RenderGraphAttachmentInfo hdr_attachment{};
                hdr_attachment.load_op = LoadOp::Clear;
                hdr_attachment.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 1.0f);
                parameters.hdr_color = pass_builder.writeColor(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA16_FLOAT, "RDG SceneHdrColor"),
                    "SceneHdrColor"), hdr_attachment);
                pass_builder.blackboard().set<SceneHdrKey>(parameters.hdr_color);

                RenderGraphAttachmentInfo depth_attachment{};
                depth_attachment.load_op = LoadOp::Clear;
                parameters.depth = pass_builder.writeDepth(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::D32, "RDG SceneDepth"),
                    "SceneDepth"), depth_attachment);

                RenderGraphBufferDesc primitive_scene_buffer_desc{};
                primitive_scene_buffer_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(std::max<Size_t>(visible_instance_count, 1) * sizeof(InstanceSceneData)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                    .setDebugName("RDG OpaquePass PrimitiveSceneBuffer");
                parameters.primitive_scene_buffer = pass_builder.write(pass_builder.createTransientBuffer(
                    primitive_scene_buffer_desc, "OpaquePrimitiveSceneBuffer"));

                SceneTextures scene_textures;
                scene_textures.depth = parameters.depth;
                scene_textures.instance_scene_data = parameters.primitive_scene_buffer;
                pass_builder.blackboard().set<SceneTexturesKey>(scene_textures);

                DO_ASSERT(imports != nullptr, "OpaquePass graph imports are null");
                auto* shadow_rt = imports->require<ShadowMapRenderTargetKey>();
                DO_ASSERT(shadow_rt != nullptr, "OpaquePass requires a ShadowMap RenderTargetHandle");
                parameters.shadow_map = pass_builder.read(pass_builder.importTexture(
                    shadow_rt->getDepthTexture(), "OpaqueShadowMap"));

                if (context.scene) {
                    for (const auto& light_info : context.scene->getLightSceneInfos()) {
                        if (light_info.getLightType() != LightType::Sky || !light_info.isEnabled()) {
                            continue;
                        }
                        const auto cubemap = light_info.getSkyLightData().cubemap;
                        if (cubemap && cubemap->getGpuHandle()) {
                            parameters.skybox_texture = pass_builder.read(pass_builder.importTexture(
                                cubemap->getGpuHandle(), "OpaqueSkyboxCubemap"));
                        }
                        break;
                    }
                }
            },
            [this, binding_layout](const OpaquePassParameters& parameters,
                                   const RenderGraphPassContext& ctx,
                                   DrawCommandList& command_list) {
                DO_ASSERT(ctx.getView() != nullptr, "OpaquePass view is null");

                auto* feature = static_cast<LitSceneFeature*>(m_owning_feature);
                DO_ASSERT(feature != nullptr, "OpaquePass owning feature is null");
                auto* processor = feature->getLitProcessor();
                if (!processor) {
                    DO_ERROR("OpaquePass lit processor is null");
                    return;
                }

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                    *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());

                const auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();
                if (!mesh_ext) {
                    return;
                }
                const auto& instance_data = mesh_ext->instance_scene_data;
                const auto resolved_psb = ctx.resolveBuffer(parameters.primitive_scene_buffer);
                command_list.setBufferState(resolved_psb, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(resolved_psb, instance_data.data(), instance_data.size() * sizeof(InstanceSceneData));
                command_list.setBufferState(resolved_psb, GfxResourceStates::VertexBuffer);

                const GlobalMeshShaderData global_data{mesh_ext->frame_time_data};
                command_list.writeBuffer(processor->getGlobalConstantBuffer(), &global_data, sizeof(global_data));
                const ViewMeshShaderData view_data{ctx.getView()->getViewProjectionMatrix()};
                command_list.writeBuffer(processor->getViewConstantBuffer(), &view_data, sizeof(view_data));

                const auto camera_position = rendering_pipeline_utils::ExtractCameraPosition(*ctx.getView());
                LitPassConstantBuffer pass_cb{};
                BuildLitPassConstantBuffer(pass_cb, *ctx.getScene(), camera_position);

                auto* staging = ctx.getFrameStagingAllocator();
                if (!staging) {
                    DO_ERROR("OpaquePass: frame staging allocator is null");
                    return;
                }
                const auto allocation = staging->allocate(kLitPassConstantBufferSize);
                if (!allocation.buffer || !allocation.mapped_data) {
                    DO_ERROR("OpaquePass: unable to allocate pass constant buffer");
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
                    DO_ERROR("OpaquePass: failed to create pass binding set");
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
