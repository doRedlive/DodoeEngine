// Created by Redlive on 2026/5/7.

#include "color_grading_pass.h"

#include "../render_graph.h"

#include "runtime/core/utils/common.h"

namespace dodoe {

	void ColorGradingPass::setup() {
		createFramebuffer();
		createShaders();
		createSampler();
		createBindingLayout();
		createBindingSet();
		createGraphicsPipeline();
		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void ColorGradingPass::execute(size_t index) {
		(void)index;
		if (!m_graphics_pipeline) { createGraphicsPipeline(); }

		if (!m_input_color_target || !m_output_color_target || !m_framebuffer || !m_binding_set) {
			return;
		}

		ColorGradingConstants constants{};
		m_cmd_list->open();
		m_cmd_list->beginMarker("ColorGradingPass");
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
		m_cmd_list->setPushConstants(&constants, sizeof(constants));
		m_cmd_list->draw(rhi::DrawArguments().setVertexCount(6).setInstanceCount(1));

		m_cmd_list->setTextureState(m_output_color_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();
		m_cmd_list->endMarker();
		m_cmd_list->close();
		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void ColorGradingPass::cleanup() {
	}

	void ColorGradingPass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		refreshInputResources();
		createFramebuffer();
		createBindingSet();
		m_graphics_pipeline = nullptr;
	}

	void ColorGradingPass::refreshInputResources() {
		m_input_color_target = getTextureResource(kInputColorResourceName);
		m_output_color_target = getTextureResource(kOutputColorResourceName);
	}

	void ColorGradingPass::createShaders() {
		auto vert_source = ReadShaderFile("engine/res/shaders/bin/fullscreen.vert.spv");
		auto frag_source = ReadShaderFile("engine/res/shaders/bin/color_grading_pass.frag.spv");
		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("ColorGradingPass VS"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("ColorGradingPass PS"),
			frag_source.data(), frag_source.size());
	}

	void ColorGradingPass::createSampler() {
		m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
	}

	void ColorGradingPass::createBindingLayout() {
		auto layout_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::PushConstants(0, sizeof(ColorGradingConstants)))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(0))
			.addItem(rhi::BindingLayoutItem::Sampler(0));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(layout_desc);
	}

	void ColorGradingPass::createBindingSet() {
		if (!m_input_color_target || !m_sampler || !m_binding_layout) {
			return;
		}

		auto desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::PushConstants(0, sizeof(ColorGradingConstants)))
			.addItem(rhi::BindingSetItem::Texture_SRV(0, m_input_color_target))
			.addItem(rhi::BindingSetItem::Sampler(0, m_sampler));
		m_binding_set = m_rhi->getDevice()->createBindingSet(desc, m_binding_layout);
	}

	void ColorGradingPass::createFramebuffer() {
		refreshInputResources();
		m_framebuffer = nullptr;
		if (!m_output_color_target) {
			return;
		}

		auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(m_output_color_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void ColorGradingPass::createGraphicsPipeline() {
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
