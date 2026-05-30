// do@Redlive

#include "pick_pass.h"

#include "../render_graph.h"
#include "../render_resource.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 256;
    }

    void PickPass::setup() {
        createFramebuffer();

        MeshPipelineStateDesc desc;
        desc.vertex_shader_path = "engine/res/shaders/bin/pick_pass.vert.spv";
        desc.pixel_shader_path  = "engine/res/shaders/bin/pick_pass.frag.spv";
        desc.constant_buffer_size = sizeof(PickPassConstants);
        desc.constant_buffer_max_versions = kVolatileConstantBufferVersions;
        desc.debug_name = "PickPass";

        desc.extra_vertex_attributes = {
            rhi::VertexAttributeDesc()
                .setName("a_NodeId").setFormat(rhi::Format::R32_UINT)
                .setBufferIndex(2).setOffset(0)
                .setElementStride(sizeof(UInt32)).setIsInstanced(true)
        };

        rhi::DepthStencilState ds;
        ds.enableDepthTest().enableDepthWrite()
          .setDepthFunc(rhi::ComparisonFunc::LessOrEqual).disableStencil();
        rhi::BlendState bs;
        bs.targets[0].disableBlend();
        desc.render_state.setDepthStencilState(ds).setBlendState(bs);

        m_mesh_processor.initialize(m_rhi, desc);

        m_cmd_list = m_rhi->getDevice()->createCommandList();
    }

    void PickPass::cleanup() {
    }

    void PickPass::execute(size_t index) {
        (void)index;

        auto& scene = g_RenderResource->getRenderScene();
        const auto& camera = scene.mainCamera();
        const auto& instances = scene.mainCameraInstances();

        m_cmd_list->open();
        m_cmd_list->beginMarker("PickPass");
        m_cmd_list->setTextureState(m_pick_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
        m_cmd_list->setTextureState(m_depth_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
        m_cmd_list->commitBarriers();
        m_cmd_list->clearTextureUInt(m_pick_target, rhi::AllSubresources, 0);
        m_cmd_list->clearDepthStencilTexture(m_depth_target, rhi::AllSubresources, true, 1.0f, false, 0);

        if (!camera || !camera->isValid() || instances.empty()) {
            m_cmd_list->setTextureState(m_pick_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
            m_cmd_list->setTextureState(m_depth_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
            m_cmd_list->commitBarriers();
            m_cmd_list->endMarker();
            m_cmd_list->close();
            m_rhi->getDevice()->executeCommandList(m_cmd_list);
            return;
        }

        scene.prepareBuffers(m_cmd_list);

        PickPassConstants constants{};
        constants.view_projection = camera->getViewProjectionMatrix();
        m_cmd_list->writeBuffer(m_mesh_processor.getConstantBuffer(), &constants, sizeof(constants));

        m_mesh_processor.createGraphicsPipeline(m_framebuffer);

        auto batches = m_mesh_processor.buildMeshBatches(instances);
        auto commands = m_mesh_processor.buildDrawCommands(batches, m_cmd_list);
        m_mesh_processor.submitDrawCommands(commands, m_framebuffer, graph().getViewportExtent(), m_cmd_list);

        m_cmd_list->setTextureState(m_pick_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->setTextureState(m_depth_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->commitBarriers();
        m_cmd_list->endMarker();
        m_cmd_list->close();
        m_rhi->getDevice()->executeCommandList(m_cmd_list);
    }

    void PickPass::onViewportResize(const Vector2i& viewport_extent) {
        (void)viewport_extent;
        createFramebuffer();
        m_mesh_processor.invalidatePipeline();
    }

    void PickPass::createFramebuffer() {
        m_pick_target = getTextureResource(kPickColorName);
        m_depth_target = getTextureResource(kPickDepthName);
        m_framebuffer = nullptr;
        if (!m_pick_target || !m_depth_target) {
            return;
        }

        auto framebuffer_desc = rhi::FramebufferDesc()
            .addColorAttachment(m_pick_target)
            .setDepthAttachment(m_depth_target);
        m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
    }

} // dodoe
