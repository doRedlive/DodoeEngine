// Created by Redlive on 2026/4/8.

#include "sprite_pass.h"

#include "../render_graph.h"
#include "../render_resource.h"
#include "../renderer_2d.h"
#include "../framework/camera.h"
#include "../framework/descriptor_table_manager.h"
#include "../interface/rhi_context.h"

#include "runtime/core/utils/common.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {
	SpritePass::SpritePass(RhiContext* rhi, Camera* camera, DescriptorTableManager* descriptor_manager)
		: camera_(camera), m_descriptor_table(descriptor_manager) {
		m_rhi = rhi;
	}

	void SpritePass::setup() {
		createFramebuffer();
		createShaders();
		createSampler();
		createInputLayout();
		createBindingLayout();
		createGraphicsPipeline();
		createBuffers();
		createBindingSet();
		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void SpritePass::execute(size_t index) {
		(void)index;
		if (!m_graphics_pipeline) { createGraphicsPipeline(); }

		const auto& batches = Renderer2d::swapQuadCpuBatches();
		if (batches.empty()) {
			return;
		}

		m_cmd_list->open();
		m_cmd_list->setTextureState(m_scene_color_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->setTextureState(m_scene_depth_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
		m_cmd_list->commitBarriers();

		const Matrix4f view_projection = camera_->getViewProjectionMatrix();
		for (size_t batch_index = 0; batch_index < batches.size(); ++batch_index) {
			const auto& batch = batches[batch_index];
			if (batch.indices.empty() || batch.vertices.empty()) {
				continue;
			}
			drawQuadBatch(batch, view_projection);
		}

		m_cmd_list->setTextureState(m_scene_color_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();

		m_cmd_list->close();
		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void SpritePass::cleanup() {
		m_scene_color_target = nullptr;
		m_scene_depth_target = nullptr;
		m_framebuffer = nullptr;
	}

	void SpritePass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		createFramebuffer();
		m_graphics_pipeline = nullptr;
	}

	void SpritePass::createGraphicsPipeline() {
		auto framebuffer_info = m_framebuffer->getFramebufferInfo();

		auto pipeline_desc = rhi::GraphicsPipelineDesc()
			.setInputLayout(m_input_layout)
			.setVertexShader(m_vertex_shader)
			.setPixelShader(m_pixel_shader)
			.addBindingLayout(m_binding_layout)
			.addBindingLayout(m_descriptor_table->getDescriptorTable()->getLayout());

		rhi::DepthStencilState depth_stencil_state;
		depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(rhi::ComparisonFunc::LessOrEqual).disableStencil();
		rhi::BlendState blend_state;
		blend_state.targets[0]
			.enableBlend()
			.setSrcBlend(rhi::BlendFactor::SrcAlpha)
			.setDestBlend(rhi::BlendFactor::OneMinusSrcAlpha)
			.setBlendOp(rhi::BlendOp::Add)
			.setSrcBlendAlpha(rhi::BlendFactor::One)
			.setDestBlendAlpha(rhi::BlendFactor::OneMinusSrcAlpha)
			.setBlendOpAlpha(rhi::BlendOp::Add);
		rhi::RasterState raster_state;
		raster_state.setCullNone();
		rhi::RenderState render_state;
		render_state.setBlendState(blend_state);
		render_state.setDepthStencilState(depth_stencil_state);
		render_state.setRasterState(raster_state);
		pipeline_desc.setRenderState(render_state);

		m_graphics_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, framebuffer_info);
	}

	void SpritePass::createBuffers() {
        auto vertex_buffer_desc = rhi::BufferDesc()
            .setByteSize(sizeof(QuadVertex) * Renderer2d::MaxQuadCount * 4)
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
            .setDebugName("SpritePass Vertex Buffer");
        m_vertex_buffer = m_rhi->getDevice()->createBuffer(vertex_buffer_desc); 
        
        auto index_buffer_desc = rhi::BufferDesc()
            .setByteSize(sizeof(ui32) * Renderer2d::MaxQuadCount * 6)
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::IndexBuffer)
            .setDebugName("SpritePass Index Buffer");
        m_index_buffer = m_rhi->getDevice()->createBuffer(index_buffer_desc);

		auto camera_buffer_desc = rhi::BufferDesc()
			.setByteSize(sizeof(Matrix4f))
			.setIsConstantBuffer(true)
			.setIsVolatile(true)
			.setMaxVersions(16)
			.setDebugName("SpritePass Camera Buffer");
		m_camera_buffer = m_rhi->getDevice()->createBuffer(camera_buffer_desc);
	}

	void SpritePass::createShaders() {
		auto vert_source = ReadShaderFile("engine/res/shaders/sprite_pass.vert.spv");
		auto frag_source = ReadShaderFile("engine/res/shaders/sprite_pass.frag.spv");

		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("SpritePass Vertex Shader"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("SpritePass Pixel Shader"),
			frag_source.data(), frag_source.size());
		if (!m_vertex_shader || !m_pixel_shader) {
			DO_ASSERT(false, "SpritePass: createShader failed.");
			return;
		}
	}

	void SpritePass::createSampler() {
		m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
	}

	void SpritePass::createInputLayout() {
		rhi::VertexAttributeDesc attributes[] = {
			rhi::VertexAttributeDesc()
				.setName("a_Position")
				.setFormat(rhi::Format::RGB32_FLOAT)
				.setOffset(offsetof(QuadVertex, position))
				.setElementStride(sizeof(QuadVertex)),
			rhi::VertexAttributeDesc()
				.setName("a_UV")
				.setFormat(rhi::Format::RG32_FLOAT)
				.setOffset(offsetof(QuadVertex, uv))
				.setElementStride(sizeof(QuadVertex)),
			rhi::VertexAttributeDesc()
				.setName("a_Color")
				.setFormat(rhi::Format::RGBA32_FLOAT)
				.setOffset(offsetof(QuadVertex, color))
				.setElementStride(sizeof(QuadVertex)),
			rhi::VertexAttributeDesc()
				.setName("a_TexIndex")
				.setFormat(rhi::Format::R32_UINT)
				.setOffset(offsetof(QuadVertex, texture_index))
				.setElementStride(sizeof(QuadVertex)),
		};

		m_input_layout = m_rhi->getDevice()->createInputLayout(
			attributes,
			static_cast<ui32>(std::size(attributes)),
			m_vertex_shader);
	}

	void SpritePass::createBindingLayout() {
		auto binding_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0))
			.addItem(rhi::BindingLayoutItem::Sampler(0));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(binding_desc);
	}

	void SpritePass::createBindingSet() {
		auto binding_set_desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::ConstantBuffer(0, m_camera_buffer))
			.addItem(rhi::BindingSetItem::Sampler(0, m_sampler));
		m_binding_set = m_rhi->getDevice()->createBindingSet(binding_set_desc, m_binding_layout);
	}

	void SpritePass::createFramebuffer() {
		m_scene_color_target = nullptr;
		m_scene_depth_target = nullptr;
		m_framebuffer = nullptr;

		m_scene_color_target = getTextureResource(kInputSceneColorResourceName);
		m_scene_depth_target = getTextureResource(kInputSceneDepthResourceName);

		auto framebuffer_desc = rhi::FramebufferDesc()
			.addColorAttachment(m_scene_color_target)
			.setDepthAttachment(m_scene_depth_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void SpritePass::drawQuadBatch(const QuadCpuData& batch, const Matrix4f& view_projection) {
		m_cmd_list->writeBuffer(m_camera_buffer, &view_projection, sizeof(Matrix4f));

		const size_t vertex_byte_size = sizeof(QuadVertex) * batch.vertices.size();
		const size_t index_byte_size = sizeof(ui32) * batch.indices.size();
		m_cmd_list->writeBuffer(m_vertex_buffer, batch.vertices.data(), vertex_byte_size);
		m_cmd_list->writeBuffer(m_index_buffer, batch.indices.data(), index_byte_size);

		auto state = rhi::GraphicsState()
			.setPipeline(m_graphics_pipeline)
			.setFramebuffer(m_framebuffer)
			.setViewport(rhi::ViewportState().addViewportAndScissorRect(
				rhi::Viewport(static_cast<float>(graph().getViewportExtent().x), static_cast<float>(graph().getViewportExtent().y))))
			.addBindingSet(m_binding_set)
			.addBindingSet(m_descriptor_table->getDescriptorTable())
			.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(m_vertex_buffer).setSlot(0).setOffset(0))
			.setIndexBuffer(rhi::IndexBufferBinding().setBuffer(m_index_buffer).setFormat(rhi::Format::R32_UINT).setOffset(0));

		m_cmd_list->setGraphicsState(state);
		auto args = rhi::DrawArguments()
			.setVertexCount(static_cast<ui32>(batch.indices.size()))
			.setInstanceCount(1);
		m_cmd_list->drawIndexed(args);
	}
} // dodoe
