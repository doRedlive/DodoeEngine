// Created by Redlive on 2026/4/8.

#include "sprite_pass.h"

#include "../render_resource.h"
#include "../render_helper.h"
#include "../renderer_2d.h"
#include "../camera/camera.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/resource_type.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {

	SpritePass::SpritePass(const RenderPassCreateInfo& info, size_t target_count, const Vector2i& target_extent, Camera* camera)
		: RenderPass(info), target_extent_(target_extent), target_count_(target_count), camera_(camera) {
		setName("SpritePass");
	}

	void SpritePass::setup() {
		createBuffers();
		createBindingLayout();
		createSampler();
		createBindingSet({});
		createFramebuffers();
		createGraphicsPipeline();
	}

	void SpritePass::execute(size_t index) {
		const auto& batches = Renderer2d::swapQuadCpuBatches();
		const size_t framebuffer_index = index % framebuffers_.size();
		auto framebuffer = framebuffers_[framebuffer_index];
		auto scene_target = scene_targets_[framebuffer_index];

		std::vector<rhi::BindingSetHandle> batch_binding_sets;
		batch_binding_sets.reserve(batches.size());
		for (const auto& batch : batches) {
			if (!checkBindingSet(batch.textures)) {
				createBindingSet(batch.textures);
			}
			batch_binding_sets.push_back(binding_set_);
		}

		auto cmd_list = device_->createCommandList();
		cmd_list->open();
		cmd_list->setTextureState(scene_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		cmd_list->commitBarriers();
		cmd_list->clearTextureFloat(scene_target, rhi::AllSubresources, rhi::Color(0.1f));

		if (batches.empty()) {
			cmd_list->setTextureState(scene_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
			cmd_list->commitBarriers();
			cmd_list->close();
			device_->executeCommandList(cmd_list);
			return;
		}

		const Matrix4f view_projection = camera_ ? camera_->view_projection_matrix() : Matrix4f(1.0f);
		cmd_list->writeBuffer(camera_buffer_, &view_projection, sizeof(Matrix4f));

		for (size_t batch_index = 0; batch_index < batches.size(); ++batch_index) {
			const auto& batch = batches[batch_index];
			if (batch.indices.empty() || batch.vertices.empty()) {
				continue;
			}
			if (batch_index >= batch_binding_sets.size() || !batch_binding_sets[batch_index]) {
				continue;
			}

			const size_t vertex_byte_size = sizeof(QuadVertex) * batch.vertices.size();
			const size_t index_byte_size = sizeof(ui32) * batch.indices.size();
			cmd_list->writeBuffer(vertex_buffer_, batch.vertices.data(), vertex_byte_size);
			cmd_list->writeBuffer(index_buffer_, batch.indices.data(), index_byte_size);

			auto state = rhi::GraphicsState()
				.setPipeline(graphics_pipeline_)
				.setFramebuffer(framebuffer)
				.setViewport(rhi::ViewportState().addViewportAndScissorRect(
					rhi::Viewport(static_cast<float>(target_extent_.x), static_cast<float>(target_extent_.y))))
				.addBindingSet(batch_binding_sets[batch_index])
				.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(vertex_buffer_).setSlot(0).setOffset(0))
				.setIndexBuffer(rhi::IndexBufferBinding().setBuffer(index_buffer_).setFormat(rhi::Format::R32_UINT).setOffset(0));

			cmd_list->setGraphicsState(state);
			auto args = rhi::DrawArguments()
				.setVertexCount(static_cast<ui32>(batch.indices.size()))
				.setInstanceCount(1);
			cmd_list->drawIndexed(args);
		}

		cmd_list->setTextureState(scene_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		cmd_list->commitBarriers();

		cmd_list->close();
		device_->executeCommandList(cmd_list);
	}

	void SpritePass::cleanup() {
		scene_targets_.clear();
		framebuffers_.clear();
	}

	void SpritePass::createFramebuffers() {
		scene_targets_.clear();
		framebuffers_.clear();
		scene_targets_.reserve(target_count_);
		framebuffers_.reserve(target_count_);

		for (size_t index = 0; index < target_count_; ++index) {
			auto texture_desc = rhi::TextureDesc()
				.setDimension(rhi::TextureDimension::Texture2D)
				.setFormat(rhi::Format::RGBA8_UNORM)
				.setWidth(target_extent_.x)
				.setHeight(target_extent_.y)
				.setIsRenderTarget(true)
				.enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource)
				.setDebugName("SpritePass Scene Target");
			auto target = device_->createTexture(texture_desc);
			if (!target) { continue; }
			scene_targets_.push_back(target);

			auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(target);
			auto framebuffer = device_->createFramebuffer(framebuffer_desc);
			framebuffers_.push_back(framebuffer);
		}

	}

	void SpritePass::createGraphicsPipeline() {
		auto vert_source = ReadShaderFile("engine/res/shaders/sprite_pass.vert.spv");
		auto frag_source = ReadShaderFile("engine/res/shaders/sprite_pass.frag.spv");

		auto vertex_shader = device_->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("SpritePass Vertex Shader"),
			vert_source.data(), vert_source.size());
		auto pixel_shader = device_->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("SpritePass Pixel Shader"),
			frag_source.data(), frag_source.size());
		if (!vertex_shader || !pixel_shader) {
			DoError("SpritePass: createShader failed.");
			return;
		}

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

		auto input_layout = device_->createInputLayout(
			attributes,
			static_cast<ui32>(std::size(attributes)),
			vertex_shader);
		DoAssert(input_layout, "SpritePass: createInputLayout failed!");

		auto framebuffer_info = framebuffers_.front()->getFramebufferInfo();

		auto pipeline_desc = rhi::GraphicsPipelineDesc()
			.setInputLayout(input_layout)
			.setVertexShader(vertex_shader)
			.setPixelShader(pixel_shader)
			.addBindingLayout(binding_layout_);

		rhi::DepthStencilState depth_stencil_state;
		depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();
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

		graphics_pipeline_ = device_->createGraphicsPipeline(pipeline_desc, framebuffer_info);
		if (!graphics_pipeline_) {
			DoError("SpritePass: createGraphicsPipeline failed.");
		}
	}

	void SpritePass::createBuffers() {
        auto vertex_buffer_desc = rhi::BufferDesc()
            .setByteSize(sizeof(QuadVertex) * Renderer2d::MaxQuadCount * 4)
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
            .setDebugName("SpritePass Vertex Buffer");
        vertex_buffer_ = device_->createBuffer(vertex_buffer_desc); 
        
        auto index_buffer_desc = rhi::BufferDesc()
            .setByteSize(sizeof(ui32) * Renderer2d::MaxQuadCount * 6)
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::IndexBuffer)
            .setDebugName("SpritePass Index Buffer");
        index_buffer_ = device_->createBuffer(index_buffer_desc);

		auto camera_buffer_desc = rhi::BufferDesc()
			.setByteSize(sizeof(Matrix4f))
			.setIsConstantBuffer(true)
			.setIsVolatile(true)
			.setMaxVersions(16)
			.setDebugName("SpritePass Camera Buffer");
		camera_buffer_ = device_->createBuffer(camera_buffer_desc);
	}

	void SpritePass::createSampler() {
		sampler_ = device_->createSampler(rhi::SamplerDesc());
	}

	void SpritePass::createBindingLayout() {
		auto binding_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::Texture_SRV(0).setSize(Renderer2d::MaxTextureCount))
			.addItem(rhi::BindingLayoutItem::Sampler(0))
			.addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0));
		binding_layout_ = device_->createBindingLayout(binding_desc);
		bound_texture_ids_.clear();
	}

	void SpritePass::createBindingSet(const std::vector<identifier>& texture_ids) {
		const size_t texture_count = (std::min)(texture_ids.size(), static_cast<size_t>(Renderer2d::MaxTextureCount));

		auto fallback = TextureManager::self().getFallbackTexture();

		rhi::BindingSetDesc binding_set_desc;
		for (uint32_t i = 0; i < static_cast<uint32_t>(Renderer2d::MaxTextureCount); ++i) {
			rhi::TextureHandle texture = fallback;
			if (i < texture_count) {
				const identifier texture_id = texture_ids[i];
				if (texture_id != 0) {
					const auto texture_res = ResourceManager::self().getTextureRes(texture_id);
					texture = TextureManager::self().getTexture(texture_id, texture_res);
				}
			}
			binding_set_desc.addItem(rhi::BindingSetItem::Texture_SRV(0, texture).setArrayElement(i));
		}
		binding_set_desc.addItem(rhi::BindingSetItem::Sampler(0, sampler_));
		binding_set_desc.addItem(rhi::BindingSetItem::ConstantBuffer(0, camera_buffer_));

		binding_set_ = device_->createBindingSet(binding_set_desc, binding_layout_);
		bound_texture_ids_.assign(texture_ids.begin(), texture_ids.begin() + texture_count);
	}

	bool SpritePass::checkBindingSet(const std::vector<identifier>& texture_ids) {
		if (!binding_set_) {
			return false;
		}
		const size_t texture_count = (std::min)(texture_ids.size(), static_cast<size_t>(Renderer2d::MaxTextureCount));
		if (bound_texture_ids_.size() == texture_count) {
			for (size_t i = 0; i < texture_count; ++i) {
				if (bound_texture_ids_[i] != texture_ids[i]) {
					return false;
				}
			}
			return true;
		}
		return false;
	}

} // dodoe
