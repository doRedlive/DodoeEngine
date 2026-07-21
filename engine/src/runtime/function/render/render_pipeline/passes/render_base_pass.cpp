// do@Redlive

#include "render_base_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../../render_view/render_view.h"
#include "../../render_view/mesh_view_extension.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command_dispatcher.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    const String& GBufferPass::getName() const {
        static const String kName = "GBufferPass";
        return kName;
    }

    RenderGraphPassFlags GBufferPass::getFlags() const {
        return RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull;
    }

    void GBufferPass::setup(RenderGraphPassBuilder& builder,
                             const RenderPassContext& context,
                             const RenderView& view) {
        DO_ASSERT(context.isValid(), "RenderPipeline pass context is invalid");

        m_mesh_processor = &context.getMeshProcessor<MeshPassType::GBuffer>();

        const auto* mesh_ext = view.getExtension<MeshViewExtension>();
        if (!mesh_ext) {
            return;
        }
        const Size_t visible_instance_count = mesh_ext->instance_scene_data.size();

        const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
        using namespace rendering_pipeline_utils;

        m_albedo   = builder.write(builder.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM,  "RDG BaseAlbedo"),   "BaseAlbedo"));
        m_normal   = builder.write(builder.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA16_FLOAT, "RDG BaseNormal"),   "BaseNormal"));
        m_position = builder.write(builder.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA32_FLOAT, "RDG BasePosition"), "BasePosition"));
        m_material = builder.write(builder.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM,  "RDG BaseMaterial"), "BaseMaterial"));
        m_depth    = builder.write(builder.createTransientTexture(MakeSwapchainDepth2D(swapchain_extent, GfxFormat::D32, "RDG BaseDepth"), "BaseDepth"));

        RenderGraphBufferDesc primitive_scene_buffer_desc{};
        primitive_scene_buffer_desc.desc = GfxBufferDesc()
            .setByteSize(static_cast<UInt32>(std::max<Size_t>(visible_instance_count, 1) * sizeof(InstanceSceneData)))
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
            .setDebugName("RDG BasePass PrimitiveSceneBuffer");
        m_primitive_scene_buffer = builder.write(builder.createTransientBuffer(primitive_scene_buffer_desc, "BasePrimitiveSceneBuffer"));
        m_constant_buffer = builder.importBuffer(m_mesh_processor->getConstantBuffer(), "GBufferConstantBuffer");

        SceneTextures gbuffer;
        gbuffer.albedo   = m_albedo;
        gbuffer.normal   = m_normal;
        gbuffer.position = m_position;
        gbuffer.material = m_material;
        gbuffer.depth    = m_depth;
        gbuffer.instance_scene_data = m_primitive_scene_buffer;
        builder.blackboard().set<SceneTexturesKey>(gbuffer);
    }

    void GBufferPass::execute(const RenderGraphPassContext& context, DrawCommandList& command_list) {
        DO_ASSERT(context.getView() != nullptr, "BasePass view is null");
        DO_ASSERT(m_mesh_processor != nullptr, "GBufferPass mesh processor is null");

        const auto* view = context.getView();

        const auto albedo = context.resolveTexture(m_albedo);
        const auto normal = context.resolveTexture(m_normal);
        const auto position = context.resolveTexture(m_position);
        const auto material = context.resolveTexture(m_material);
        const auto depth = context.resolveTexture(m_depth);
        const auto primitive_scene_buffer = context.resolveBuffer(m_primitive_scene_buffer);

        auto framebuffer_desc = GfxFramebufferDesc()
            .addColorAttachment(albedo)
            .addColorAttachment(normal)
            .addColorAttachment(position)
            .addColorAttachment(material)
            .setDepthAttachment(depth);
        auto framebuffer = command_list.createFramebuffer(framebuffer_desc);

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
            MeshDrawCommandDispatcher::UploadInstanceTransforms(context, mesh_ext->instance_scene_data, m_primitive_scene_buffer, command_list);
            command_list.setBufferState(primitive_scene_buffer, GfxResourceStates::VertexBuffer);
            command_list.commitBarriers();
            MeshDrawCommandDispatcher::DispatchCached(
                context,
                MeshPassType::GBuffer,
                mesh_ext->gbuffer_shader_data,
                mesh_ext->cached_draw_instances[static_cast<size_t>(MeshPassType::GBuffer)],
                *mesh_ext->cached_commands,
                framebuffer,
                viewport_state,
                GfxGraphicsPipelineHandle{},
                m_primitive_scene_buffer,
                m_constant_buffer,
                command_list
            );
            MeshDrawCommandDispatcher::DispatchCached(
                context,
                MeshPassType::GBuffer,
                mesh_ext->dynamic_shader_data,
                mesh_ext->dynamic_draw_instances[static_cast<size_t>(MeshPassType::GBuffer)],
                mesh_ext->frame_commands,
                framebuffer,
                viewport_state,
                GfxGraphicsPipelineHandle{},
                m_primitive_scene_buffer,
                m_constant_buffer,
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

} // namespace dodoe
