// Created by Redlive on 2026/4/8.

#include "sprite_pass.h"

#include "../render_resource.h"
#include "../render_helper.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/resource/resource_type.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {

	namespace {
		const int k_MaxQuadCount = 2048;
		const int k_MaxIndexCount = k_MaxQuadCount * 6;
		const int k_MaxVertexCount = k_MaxQuadCount * 4;
		const int k_MaxTextures = 1024;
	}

	static std::vector<char> ReadShaderFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in.is_open()) {
			DoError("Open shader file {} failed!", path);
			return {};
		}

		const std::streamsize size = in.tellg();
		in.seekg(0, std::ios::beg);

		std::vector<char> content(static_cast<size_t>(size));
		in.read(content.data(), size);
		return content;
	}

	SpritePass::SpritePass(const RenderPassCreateInfo& info, const std::vector<rhi::TextureHandle>& swapchain_targets, const Vector2i& target_extent)
		: RenderPass(info), swapchain_targets_(swapchain_targets), target_extent_(target_extent) {
		setName("SpritePass");
	}

	void SpritePass::setup() {
		if (initialized_) {
			return;
		}

		if (!device_ || swapchain_targets_.empty()) {
			return;
		}

		createBuffers();
		createBindingLayout();
		createSampler();
		createBindingSet();
		createFramebuffers();
		createGraphicsPipeline();
		cmd_list_ = device_->createCommandList();
		initialized_ = true;
	}

	void SpritePass::execute() {
		const auto& context = Renderer2d::gainRenderCpuData();
		if (framebuffers_.empty() || context.indices.empty() || context.vertices.empty()) {
			return;
		}

		ensureBufferCapacity(context.vertices.size(), context.indices.size());

		const size_t framebuffer_index = current_framebuffer_index_ % framebuffers_.size();
		auto framebuffer = framebuffers_[framebuffer_index];

		createBindingSet();

		cmd_list_->open();

		const size_t vertex_byte_size = sizeof(QuadVertex) * context.vertices.size();
		if (vertex_byte_size <= vertex_buffer_->getDesc().byteSize) {
			cmd_list_->writeBuffer(vertex_buffer_, context.vertices.data(), vertex_byte_size);
		} else {
			DoError("SpritePass: vertex buffer is not large enough.");
		}

		const size_t index_byte_size = sizeof(ui32) * context.indices.size();
		if (index_byte_size <= index_buffer_->getDesc().byteSize) {
			cmd_list_->writeBuffer(index_buffer_, context.indices.data(), index_byte_size);
		} else {
			DoError("SpritePass: index buffer is not large enough.");
		}

		auto state = rhi::GraphicsState()
			.setPipeline(graphics_pipeline_)
			.setFramebuffer(framebuffer)
			.setViewport(rhi::ViewportState().addViewportAndScissorRect(
				rhi::Viewport(static_cast<float>(target_extent_.x), static_cast<float>(target_extent_.y))))
			.addBindingSet(binding_set_)
			.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(vertex_buffer_).setSlot(0).setOffset(0))
			.setIndexBuffer(rhi::IndexBufferBinding().setBuffer(index_buffer_).setFormat(rhi::Format::R32_UINT).setOffset(0));

		cmd_list_->setGraphicsState(state);
		auto args = rhi::DrawArguments()
			.setVertexCount(static_cast<ui32>(context.indices.size()))
			.setInstanceCount(1);
		cmd_list_->drawIndexed(args);

		cmd_list_->close();		

		device_->executeCommandList(cmd_list_);
		++current_framebuffer_index_;
	}

	void SpritePass::cleanup() {

	}

	void SpritePass::createFramebuffers() {
		framebuffers_.clear();
		framebuffers_.reserve(swapchain_targets_.size());
		current_framebuffer_index_ = 0;

		for (const auto& target : swapchain_targets_) {
			auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(target);
			auto framebuffer = device_->createFramebuffer(framebuffer_desc);
			if (framebuffer) {
				framebuffers_.push_back(framebuffer);
			}
		}
	}

	void SpritePass::createGraphicsPipeline() {
		auto vert_source = ReadShaderFile("engine/res/shaders/sprite_pass.vert.spv");
		auto frag_source = ReadShaderFile("engine/res/shaders/sprite_pass.frag.spv");

		auto vertex_shader = device_->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("SpritePass VS"),
			vert_source.data(), vert_source.size());
		auto pixel_shader = device_->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("SpritePass PS"),
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
		rhi::RenderState render_state;
		render_state.setDepthStencilState(depth_stencil_state);
		pipeline_desc.setRenderState(render_state);

		graphics_pipeline_ = device_->createGraphicsPipeline(pipeline_desc, framebuffer_info);
		if (!graphics_pipeline_) {
			DoError("SpritePass: createGraphicsPipeline failed.");
		}
	}

	void SpritePass::ensureBufferCapacity(size_t vertex_count, size_t index_count) {
		const size_t required_vertex_bytes = sizeof(QuadVertex) * vertex_count;
		const size_t required_index_bytes = sizeof(ui32) * index_count;

		auto grow_to_fit = [](size_t required_bytes, size_t current_bytes) {
			size_t new_size = (std::max)(current_bytes, size_t(1));
			while (new_size < required_bytes) {
				new_size *= 2;
			}
			return new_size;
		};

		if (required_vertex_bytes > vertex_buffer_->getDesc().byteSize) {
			auto vertex_buffer_desc = rhi::BufferDesc()
				.setByteSize(grow_to_fit(required_vertex_bytes, vertex_buffer_->getDesc().byteSize))
				.setIsVertexBuffer(true)
				.enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
				.setDebugName("SpritePass Vertex Buffer");
			vertex_buffer_ = device_->createBuffer(vertex_buffer_desc);
		}

		if (required_index_bytes > index_buffer_->getDesc().byteSize) {
			auto index_buffer_desc = rhi::BufferDesc()
				.setByteSize(grow_to_fit(required_index_bytes, index_buffer_->getDesc().byteSize))
				.setIsIndexBuffer(true)
				.enableAutomaticStateTracking(rhi::ResourceStates::IndexBuffer)
				.setDebugName("SpritePass Index Buffer");
			index_buffer_ = device_->createBuffer(index_buffer_desc);
		}
	}

	void SpritePass::createBuffers() {
        auto vertex_buffer_desc = rhi::BufferDesc()
            .setByteSize(sizeof(QuadVertex) * k_MaxVertexCount)
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
            .setDebugName("SpritePass Vertex Buffer");
        vertex_buffer_ = device_->createBuffer(vertex_buffer_desc); 
        
        auto index_buffer_desc = rhi::BufferDesc()
            .setByteSize(sizeof(ui32) * k_MaxIndexCount)
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::IndexBuffer)
            .setDebugName("SpritePass Index Buffer");
        index_buffer_ = device_->createBuffer(index_buffer_desc);
	}

	void SpritePass::createSampler() {
		sampler_ = device_->createSampler(rhi::SamplerDesc());
	}

	void SpritePass::createBindingLayout() {
		auto binding_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::Texture_SRV(0).setSize(k_MaxTextures))
			.addItem(rhi::BindingLayoutItem::Sampler(0));
		binding_layout_ = device_->createBindingLayout(binding_desc);		
		DoInfo("createBindingLayout called. new layout ptr: {}", (void*)binding_layout_.Get());
	}

	void SpritePass::createBindingSet() {
		if (!binding_layout_ || !sampler_) {
			return;
		}

		const auto& cpu_texture_ids = Renderer2d::gainRenderCpuData().textures;
		const size_t texture_count = (std::min)(cpu_texture_ids.size(), static_cast<size_t>(k_MaxTextures));

		const bool layout_matches = binding_set_ && (binding_set_->getLayout() == binding_layout_.Get());
		if (layout_matches && bound_texture_ids_.size() == texture_count) {
			bool same = true;
			for (size_t i = 0; i < texture_count; ++i) {
				if (bound_texture_ids_[i] != cpu_texture_ids[i]) {
					same = false;
					break;
				}
			}
			if (same) return;
		}

		auto fallback = TextureManager::self().getFallbackTexture();
		if (!fallback) {
			DoError("SpritePass: fallback texture is null.");
			return;
		}

		rhi::BindingSetDesc binding_set_desc;
		for (uint32_t i = 0; i < static_cast<uint32_t>(k_MaxTextures); ++i) {
			rhi::TextureHandle texture = fallback;
			if (i < texture_count) {
				const identifier texture_id = cpu_texture_ids[i];
				if (texture_id != 0) {
					const auto texture_res = ResourceManager::self().get_texture(texture_id);
					texture = TextureManager::self().getTexture(texture_id, texture_res);
				}
			}
			binding_set_desc.addItem(rhi::BindingSetItem::Texture_SRV(0, texture).setArrayElement(i));
		}
		binding_set_desc.addItem(rhi::BindingSetItem::Sampler(0, sampler_));

		binding_set_ = device_->createBindingSet(binding_set_desc, binding_layout_);
		bound_texture_ids_.assign(cpu_texture_ids.begin(), cpu_texture_ids.begin() + texture_count);
		DoInfo("createBindingSet. passed layout ptr: {}, returned set layout ptr: {}", (void*)binding_layout_.Get(), (void*)binding_set_->getLayout());
	}

} // dodoe