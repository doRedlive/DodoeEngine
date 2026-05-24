// Created by Redlive on 2026/5/7.

#include "skybox_pass.h"

#include "../render_graph.h"
#include "../render_resource.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

	void SkyboxPass::setup() {
		createFramebuffer();
		createShaders();
		createSampler();
		createBindingLayout();
		createBindingSet();
		createGraphicsPipeline();
		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void SkyboxPass::execute(size_t index) {
		(void)index;
		if (!m_graphics_pipeline) {
			createGraphicsPipeline();
		}

		auto& render_scene = g_RenderResource->getRenderScene();
		const auto camera = render_scene.mainCamera();
		if (!camera || !camera->isValid() || !m_hdr_target || !m_binding_set || !m_framebuffer) {
			return;
		}

		SkyboxPushConstants constants{};
		constants.inv_view_projection = Math::Inverse(camera->getViewProjectionMatrix());

		m_cmd_list->open();
		m_cmd_list->beginMarker("SkyboxPass");
		m_cmd_list->setTextureState(m_hdr_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->commitBarriers();
		m_cmd_list->clearTextureFloat(m_hdr_target, rhi::AllSubresources, rhi::Color(0.0f, 0.0f, 0.0f, 1.0f));

		auto state = rhi::GraphicsState()
			.setPipeline(m_graphics_pipeline)
			.setFramebuffer(m_framebuffer)
			.setViewport(rhi::ViewportState().addViewportAndScissorRect(
				rhi::Viewport(static_cast<float>(graph().getViewportExtent().x), static_cast<float>(graph().getViewportExtent().y))))
			.addBindingSet(m_binding_set);

		m_cmd_list->setGraphicsState(state);
		m_cmd_list->setPushConstants(&constants, sizeof(constants));
		m_cmd_list->draw(rhi::DrawArguments().setVertexCount(6).setInstanceCount(1));

		m_cmd_list->setTextureState(m_hdr_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();
		m_cmd_list->endMarker();
		m_cmd_list->close();
		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void SkyboxPass::cleanup() {
	}

	void SkyboxPass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		refreshInputResources();
		createBindingSet();
		createFramebuffer();
		m_graphics_pipeline = nullptr;
	}

	void SkyboxPass::refreshInputResources() {
		m_hdr_target = getTextureResource(kSceneHdrColorName);
		m_depth_target = getTextureResource(kSceneDepthName);
		m_skybox_texture = g_RenderResource->getSkyboxTexture();
	}

	void SkyboxPass::createShaders() {
		auto vert_source = ReadShaderFile("engine/res/shaders/bin/fullscreen.vert.spv");
		auto frag_source = ReadShaderFile("engine/res/shaders/bin/skybox_pass.frag.spv");
		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("SkyboxPass VS"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("SkyboxPass PS"),
			frag_source.data(), frag_source.size());
	}

	void SkyboxPass::createSampler() {
		m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
	}

	void SkyboxPass::createBindingLayout() {
		auto layout_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::PushConstants(0, sizeof(SkyboxPushConstants)))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(0))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(1))
			.addItem(rhi::BindingLayoutItem::Sampler(0));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(layout_desc);
	}

	void SkyboxPass::createBindingSet() {
		if (!m_skybox_texture || !m_depth_target || !m_sampler || !m_binding_layout) {
			return;
		}

		auto desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::PushConstants(0, sizeof(SkyboxPushConstants)))
			.addItem(rhi::BindingSetItem::Texture_SRV(0, m_skybox_texture))
			.addItem(rhi::BindingSetItem::Texture_SRV(1, m_depth_target))
			.addItem(rhi::BindingSetItem::Sampler(0, m_sampler));
		m_binding_set = m_rhi->getDevice()->createBindingSet(desc, m_binding_layout);
	}

	void SkyboxPass::createFramebuffer() {
		refreshInputResources();
		m_framebuffer = nullptr;
		if (!m_hdr_target) {
			return;
		}

		auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(m_hdr_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void SkyboxPass::createGraphicsPipeline() {
		if (!m_framebuffer || !m_vertex_shader || !m_pixel_shader || !m_binding_layout) {
			return;
		}

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

		m_graphics_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, m_framebuffer->getFramebufferInfo());
	}

} // dodoe
