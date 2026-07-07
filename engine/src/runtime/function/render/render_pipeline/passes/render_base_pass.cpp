// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../../render_view/render_view.h"
#include "../../render_view/mesh_view_extension.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command_dispatcher.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe::RenderPipelinePass {

    struct GBufferPassParameters {
        RenderGraphTextureHandle albedo{};
        RenderGraphTextureHandle normal{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle material{};
        RenderGraphTextureHandle depth{};
        RenderGraphBufferHandle primitive_scene_buffer{};
        RenderGraphBufferHandle constant_buffer{};
    };

    void RenderGBufferPass(RenderGraphBuilder& graph, const RenderView& view, const RenderPassContext& pass_context) {
        DO_ASSERT(pass_context.isValid(), "RenderPipeline pass context is invalid");
        const auto& gbuffer_mesh_processor = pass_context.getMeshProcessor<MeshPassType::GBuffer>();

        const auto* mesh_ext = view.getExtension<MeshViewExtension>();
        if (!mesh_ext) {
            return;
        }
        const Size_t visible_instance_count = mesh_ext->instance_scene_data.size();

        graph.addPass<GBufferPassParameters>(
            "GBufferPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context, visible_instance_count, &gbuffer_mesh_processor](RenderGraphPassBuilder& pass_builder, GBufferPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                using namespace rendering_pipeline_utils;

                parameters.albedo   = pass_builder.write(pass_builder.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM,  "RDG BaseAlbedo"),   "BaseAlbedo"));
                parameters.normal   = pass_builder.write(pass_builder.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA16_FLOAT, "RDG BaseNormal"),   "BaseNormal"));
                parameters.position = pass_builder.write(pass_builder.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA32_FLOAT, "RDG BasePosition"), "BasePosition"));
                parameters.material = pass_builder.write(pass_builder.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM,  "RDG BaseMaterial"), "BaseMaterial"));
                parameters.depth    = pass_builder.write(pass_builder.createTransientTexture(MakeSwapchainDepth2D(swapchain_extent, GfxFormat::D32, "RDG BaseDepth"), "BaseDepth"));

                RenderGraphBufferDesc primitive_scene_buffer_desc{};
                primitive_scene_buffer_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(std::max<Size_t>(visible_instance_count, 1) * sizeof(InstanceSceneData)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                    .setDebugName("RDG BasePass PrimitiveSceneBuffer");
                parameters.primitive_scene_buffer = pass_builder.write(pass_builder.createTransientBuffer(primitive_scene_buffer_desc, "BasePrimitiveSceneBuffer"));
                parameters.constant_buffer = pass_builder.importBuffer(gbuffer_mesh_processor.getConstantBuffer(), "GBufferConstantBuffer");

                SceneTextures gbuffer;
                gbuffer.albedo   = parameters.albedo;
                gbuffer.normal   = parameters.normal;
                gbuffer.position = parameters.position;
                gbuffer.material = parameters.material;
                gbuffer.depth    = parameters.depth;
                gbuffer.instance_scene_data = parameters.primitive_scene_buffer;
                pass_builder.blackboard().set<SceneTexturesKey>(gbuffer);
            },
            [&gbuffer_mesh_processor](const GBufferPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                DO_ASSERT(context.getView() != nullptr, "BasePass view is null");
                const auto* view = context.getView();

                const auto albedo = context.resolveTexture(parameters.albedo);
                const auto normal = context.resolveTexture(parameters.normal);
                const auto position = context.resolveTexture(parameters.position);
                const auto material = context.resolveTexture(parameters.material);
                const auto depth = context.resolveTexture(parameters.depth);
                const auto primitive_scene_buffer = context.resolveBuffer(parameters.primitive_scene_buffer);

                auto framebuffer_desc = GfxFramebufferDesc()
                    .addColorAttachment(albedo)
                    .addColorAttachment(normal)
                    .addColorAttachment(position)
                    .addColorAttachment(material)
                    .setDepthAttachment(depth);
                auto framebuffer = command_list.createFramebuffer( framebuffer_desc);

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                command_list.setTextureState(albedo, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(normal, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(position, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(material, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(depth, GfxAllSubresources, GfxResourceStates::DepthWrite);
                command_list.commitBarriers();
                command_list.clearTextureFloat(albedo, GfxAllSubresources, GfxColor(0.08f, 0.09f, 0.11f, 1.0f));
                command_list.clearTextureFloat(normal, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.clearTextureFloat(position, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.clearTextureFloat(material, GfxAllSubresources, GfxColor(0.0f, 1.0f, 1.0f, 1.0f));
                command_list.clearDepthStencilTexture(depth, GfxAllSubresources, true, 1.0f, false, 0);
                command_list.setBufferState(primitive_scene_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();

                const auto* mesh_ext = view->getExtension<MeshViewExtension>();
                if (mesh_ext) {
                    MeshDrawCommandDispatcher::UploadInstanceTransforms(context, mesh_ext->instance_scene_data, parameters.primitive_scene_buffer, command_list);
                    command_list.setBufferState(primitive_scene_buffer, GfxResourceStates::VertexBuffer);
                    command_list.commitBarriers();
                    MeshDrawCommandDispatcher::Dispatch(
                        context,
                        MeshPassType::GBuffer,
                        mesh_ext->gbuffer_shader_data,
                        mesh_ext->mesh_pass_commands[static_cast<size_t>(MeshPassType::GBuffer)],
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        parameters.primitive_scene_buffer,
                        parameters.constant_buffer,
                        command_list
                    );
                }

                command_list.setTextureState(albedo, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(normal, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(position, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(material, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(depth, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe::RenderPipelinePass
