// do@Redlive

#include "combine_pass.h"

#include "../render_graph.h"
#include "runtime/core/utils/common.h"

namespace dodoe {
    namespace {
        struct CombinePushConstants {
            float viewport_pos[2];
            float viewport_size[2];
        };
    }

    void CombinePass::setup() {
        createShaders();
        createSampler();
        createBindingLayout();
        createFramebuffers();
        refreshInputResources();
        createBindingSet();
        createGraphicsPipeline();
        m_cmd_list = m_rhi->getDevice()->createCommandList();
    }

    void CombinePass::cleanup() {
        m_framebuffers.clear();
    }

    void CombinePass::execute(size_t index) {
        if (!m_graphics_pipeline) {
            createGraphicsPipeline();
        }

        const auto& swapchain_targets = m_rhi->getSwapchainTextures();
        if (swapchain_targets.empty() || m_framebuffers.empty()) {
            return;
        }

        const size_t framebuffer_index = index % swapchain_targets.size();
        const auto target_texture = swapchain_targets[framebuffer_index];
        const auto framebuffer = m_framebuffers[framebuffer_index];

        m_cmd_list->open();
        m_cmd_list->beginMarker("CombinePass");
        m_cmd_list->setTextureState(m_scene_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->setTextureState(m_imgui_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->setTextureState(target_texture, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
        m_cmd_list->commitBarriers();

        auto state = rhi::GraphicsState()
            .setPipeline(m_graphics_pipeline)
            .setFramebuffer(framebuffer)
            .setViewport(rhi::ViewportState().addViewportAndScissorRect(
                rhi::Viewport(static_cast<float>(m_rhi->getSwapchainExtent2d().x), static_cast<float>(m_rhi->getSwapchainExtent2d().y))))
            .addBindingSet(m_binding_set);

        rhi::DrawArguments args;
        args.vertexCount = 6;
        args.instanceCount = 1;

        const auto& viewport_rect = graph().getViewportRect();
        CombinePushConstants push_constants{};
        push_constants.viewport_pos[0] = viewport_rect.pos.x;
        push_constants.viewport_pos[1] = viewport_rect.pos.y;
        push_constants.viewport_size[0] = viewport_rect.size.x;
        push_constants.viewport_size[1] = viewport_rect.size.y;

        m_cmd_list->setGraphicsState(state);
        m_cmd_list->setPushConstants(&push_constants, sizeof(push_constants));
        m_cmd_list->draw(args);
        m_cmd_list->setTextureState(target_texture, rhi::AllSubresources, rhi::ResourceStates::Present);
        m_cmd_list->commitBarriers();
        m_cmd_list->endMarker();
        m_cmd_list->close();
        m_rhi->getDevice()->executeCommandList(m_cmd_list);
    }

    void CombinePass::createGraphicsPipeline() {
		auto framebuffer_info = m_framebuffers.front()->getFramebufferInfo();

		auto pipeline_desc = rhi::GraphicsPipelineDesc()
			.setVertexShader(m_vertex_shader)
			.setPixelShader(m_pixel_shader)
			.addBindingLayout(m_binding_layout)
            .setPrimType(rhi::PrimitiveType::TriangleList);

		rhi::DepthStencilState depth_stencil_state;
		depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();
		rhi::BlendState blend_state;
		blend_state.targets[0].disableBlend();
		rhi::RasterState raster_state;
		raster_state.setCullNone();
		rhi::RenderState render_state;
		render_state.setBlendState(blend_state);
		render_state.setDepthStencilState(depth_stencil_state);
		render_state.setRasterState(raster_state);
		pipeline_desc.setRenderState(render_state);

		m_graphics_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, framebuffer_info);
    }

    void CombinePass::createFramebuffers() {
        m_framebuffers.clear();

        const auto& swapchain_targets = m_rhi->getSwapchainTextures();
        m_framebuffers.reserve(swapchain_targets.size());

        for (const auto& target_texture : swapchain_targets) {
            auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(target_texture);
            m_framebuffers.push_back(m_rhi->getDevice()->createFramebuffer(framebuffer_desc));
        }
    }

    void CombinePass::createShaders() {
        auto vert_source = ReadShaderFile("engine/res/shaders/bin/fullscreen.vert.spv");
        auto frag_source = ReadShaderFile("engine/res/shaders/bin/combine_pass.frag.spv");

		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("CombinePass Vertex Shader"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("CombinePass Pixel Shader"),
			frag_source.data(), frag_source.size());
		if (!m_vertex_shader || !m_pixel_shader) {
			DO_ASSERT(false, "CombinePass: createShader failed.");
			return;
		}
    }

    void CombinePass::createBindingLayout() {
        rhi::BindingLayoutDesc desc;
        desc.setVisibility(rhi::ShaderType::All);
        desc.addItem(rhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 4));
        desc.addItem(rhi::BindingLayoutItem::Texture_SRV(0));
        desc.addItem(rhi::BindingLayoutItem::Texture_SRV(1));
        desc.addItem(rhi::BindingLayoutItem::Sampler(0));

        m_binding_layout = m_rhi->getDevice()->createBindingLayout(desc);
    }

    void CombinePass::createBindingSet() {
        auto desc = rhi::BindingSetDesc()
            .addItem(rhi::BindingSetItem::PushConstants(0, sizeof(float) * 4))
            .addItem(rhi::BindingSetItem::Texture_SRV(0, m_scene_target))
            .addItem(rhi::BindingSetItem::Texture_SRV(1, m_imgui_target));
        desc.addItem(rhi::BindingSetItem::Sampler(0, m_sampler));
        
        m_binding_set = m_rhi->getDevice()->createBindingSet(desc, m_binding_layout);
    }

    void CombinePass::refreshInputResources() {
        m_scene_target = getTextureResource(kInputSceneColorResourceName);
        m_imgui_target = getTextureResource(kInputImGuiColorResourceName);
    }

    void CombinePass::createSampler() {
        m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
    }

    void CombinePass::onViewportResize(const Vector2i& viewport_extent) {
        (void)viewport_extent;
        refreshInputResources();
        createBindingSet();
    }

    void CombinePass::onWindowResize(const Vector2i& window_extent) {
        (void)window_extent;
        refreshInputResources();
        createFramebuffers();
        createBindingSet();
        m_graphics_pipeline = nullptr;
    }

} // dodoe
