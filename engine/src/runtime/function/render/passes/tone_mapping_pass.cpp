// Created by Redlive on 2026/5/7.

#include "tone_mapping_pass.h"

#include "../render_graph.h"

#include "runtime/core/utils/common.h"

namespace dodoe {

	void ToneMappingPass::setup() {
		createFramebuffer();
		createShaders();
		createSampler();
		createBindingLayout();
		createBindingSet();
		createGraphicsPipeline();
		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void ToneMappingPass::execute(size_t index) {
		(void)index;
		if (!m_graphics_pipeline) { createGraphicsPipeline(); }

		if (!m_input_color_target || !m_output_color_target || !m_framebuffer || !m_binding_set || !m_graphics_pipeline) {
			return;
		}

		m_cmd_list->open();
		m_cmd_list->beginMarker("ToneMappingPass");
		m_cmd_list->setTextureState(m_input_color_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->setTextureState(m_output_color_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->commitBarriers();
		m_cmd_list->clearTextureFloat(m_output_color_target, rhi::AllSubresources, rhi::Color(0.0f, 0.0f, 0.0f, 1.0f));

		auto state = rhi::GraphicsState()
			.setPipeline(m_graphics_pipeline)
			.setFramebuffer(m_framebuffer)
			.setViewport(rhi::ViewportState().addViewportAndScissorRect(
				rhi::Viewport(static_cast<float>(graph().getViewportExtent().x), static_cast<float>(graph().getViewportExtent().y))))
			.addBindingSet(m_binding_set);

		m_cmd_list->setGraphicsState(state);
		m_cmd_list->draw(rhi::DrawArguments().setVertexCount(6).setInstanceCount(1));

		m_cmd_list->setTextureState(m_output_color_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();
		m_cmd_list->endMarker();
		m_cmd_list->close();
		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void ToneMappingPass::cleanup() {
	}

	void ToneMappingPass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		refreshInputResources();
		createFramebuffer();
		createBindingSet();
		m_graphics_pipeline = nullptr;
	}

	void ToneMappingPass::refreshInputResources() {
		m_input_color_target = getTextureResource(kInputColorResourceName);
		m_output_color_target = getTextureResource(kOutputColorResourceName);
	}

	void ToneMappingPass::createShaders() {
		auto vert_source = ReadShaderFile("engine/res/shaders/bin/fullscreen.vert.spv");
		auto frag_source = ReadShaderFile("engine/res/shaders/bin/tone_mapping_pass.frag.spv");
		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("ToneMappingPass VS"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("ToneMappingPass PS"),
			frag_source.data(), frag_source.size());
	}

	void ToneMappingPass::createSampler() {
		m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
	}

	void ToneMappingPass::createBindingLayout() {
		auto layout_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::Texture_SRV(0))
			.addItem(rhi::BindingLayoutItem::Sampler(0));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(layout_desc);
	}

	void ToneMappingPass::createBindingSet() {
		if (!m_input_color_target || !m_sampler || !m_binding_layout) {
			return;
		}

		auto desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::Texture_SRV(0, m_input_color_target))
			.addItem(rhi::BindingSetItem::Sampler(0, m_sampler));
		m_binding_set = m_rhi->getDevice()->createBindingSet(desc, m_binding_layout);
	}

	void ToneMappingPass::createFramebuffer() {
		refreshInputResources();
		m_framebuffer = nullptr;
		if (!m_output_color_target) {
			return;
		}

		auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(m_output_color_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void ToneMappingPass::createGraphicsPipeline() {
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
		rhi::RasterState raster_state;
		raster_state.setCullNone();
		rhi::RenderState render_state;
		render_state.setDepthStencilState(depth_stencil_state);
		render_state.setRasterState(raster_state);
		pipeline_desc.setRenderState(render_state);

		m_graphics_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, m_framebuffer->getFramebufferInfo());
	}

} // dodoe
