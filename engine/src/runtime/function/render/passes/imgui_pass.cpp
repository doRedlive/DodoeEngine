// do: GreenMuffin

#ifdef DODOE_EDITOR

#include "imgui_pass.h"

#include "../interface/rhi_context.h"

#include "runtime/core/utils/common.h"

#include "imgui/imgui.h"

namespace dodoe {

	namespace {
		struct ImGuiPushConstants {
			float inv_display_size[2];
			float display_pos[2];
		};
	}

	ImGuiPass::ImGuiPass(RhiContext* rhi) {
		m_rhi = rhi;
	}

	void ImGuiPass::setup() {
		createFramebuffer();
		createShaders();
		createFontTexture();
		createFontSampler();
		createBuffers();
		createInputLayout();
		createBindingLayout();
		createGraphicsPipeline();
		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void ImGuiPass::execute(size_t index) {
		if (!m_font_texture && ImGui::GetCurrentContext()) {
			createFontTexture();
		}

		ImGui::Render();
		(void)index;

		if (!m_pipeline) { createGraphicsPipeline(); }

		ImDrawData* draw_data = ImGui::GetDrawData();
		if (!draw_data || !m_pipeline || !m_render_target || !m_framebuffer) {
			return;
		}

		const int framebuffer_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
		const int framebuffer_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
		if (framebuffer_width <= 0 || framebuffer_height <= 0) {
			return;
		}

		m_cmd_list->open();
		m_cmd_list->beginMarker("ImGuiPass");
		m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->commitBarriers();
		m_cmd_list->clearTextureFloat(m_render_target, rhi::AllSubresources, rhi::Color(0.0f, 0.0f, 0.0f, 0.0f));

		if (draw_data->TotalVtxCount <= 0 || draw_data->TotalIdxCount <= 0) {
			m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
			m_cmd_list->commitBarriers();
			m_cmd_list->endMarker();
			m_cmd_list->close();
			m_rhi->getDevice()->executeCommandList(m_cmd_list);
			return;
		}

		updateGeometry();

		rhi::GraphicsState state;
		state.framebuffer = m_framebuffer;
		state.pipeline = m_pipeline;
		state.viewport.viewports.push_back(rhi::Viewport(static_cast<float>(framebuffer_width), static_cast<float>(framebuffer_height)));
		state.viewport.scissorRects.resize(1);
		state.vertexBuffers.push_back(rhi::VertexBufferBinding().setBuffer(m_vertex_buffer).setSlot(0).setOffset(0));
		state.indexBuffer = rhi::IndexBufferBinding()
			.setBuffer(m_index_buffer)
			.setFormat(sizeof(ImDrawIdx) == 2 ? rhi::Format::R16_UINT : rhi::Format::R32_UINT)
			.setOffset(0);

		const ImVec2 clip_off = draw_data->DisplayPos;
		const ImVec2 clip_scale = draw_data->FramebufferScale;

		ImGuiPushConstants push_constants{};
		push_constants.inv_display_size[0] = 1.0f / draw_data->DisplaySize.x;
		push_constants.inv_display_size[1] = 1.0f / draw_data->DisplaySize.y;
		push_constants.display_pos[0] = draw_data->DisplayPos.x;
		push_constants.display_pos[1] = draw_data->DisplayPos.y;

		int vtx_offset = 0;
		int idx_offset = 0;
		for (int n = 0; n < draw_data->CmdListsCount; ++n) {
			const ImDrawList* draw_list = draw_data->CmdLists[n];
			for (int cmd_index = 0; cmd_index < draw_list->CmdBuffer.Size; ++cmd_index) {
				const ImDrawCmd* draw_cmd = &draw_list->CmdBuffer[cmd_index];

				if (draw_cmd->UserCallback) {
					draw_cmd->UserCallback(draw_list, draw_cmd);
					continue;
				}

				ImVec2 clip_min(
					(draw_cmd->ClipRect.x - clip_off.x) * clip_scale.x,
					(draw_cmd->ClipRect.y - clip_off.y) * clip_scale.y
				);
				ImVec2 clip_max(
					(draw_cmd->ClipRect.z - clip_off.x) * clip_scale.x,
					(draw_cmd->ClipRect.w - clip_off.y) * clip_scale.y
				);

				clip_min.x = std::clamp(clip_min.x, 0.0f, static_cast<float>(framebuffer_width));
				clip_min.y = std::clamp(clip_min.y, 0.0f, static_cast<float>(framebuffer_height));
				clip_max.x = std::clamp(clip_max.x, 0.0f, static_cast<float>(framebuffer_width));
				clip_max.y = std::clamp(clip_max.y, 0.0f, static_cast<float>(framebuffer_height));

				if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
					continue;
				}

				auto* texture = reinterpret_cast<rhi::ITexture*>(draw_cmd->GetTexID());
				auto* binding_set = getBindingSet(texture);
				if (!binding_set) {
					continue;
				}

				state.bindings = { binding_set };
				state.viewport.scissorRects[0] = rhi::Rect(
					static_cast<int>(clip_min.x),
					static_cast<int>(clip_max.x),
					static_cast<int>(clip_min.y),
					static_cast<int>(clip_max.y)
				);

				rhi::DrawArguments args;
				args.vertexCount = draw_cmd->ElemCount;
				args.instanceCount = 1;
				args.startIndexLocation = idx_offset + static_cast<int>(draw_cmd->IdxOffset);
				args.startVertexLocation = vtx_offset + static_cast<int>(draw_cmd->VtxOffset);

				m_cmd_list->setGraphicsState(state);
				m_cmd_list->setPushConstants(&push_constants, sizeof(push_constants));
				m_cmd_list->drawIndexed(args);
			}

			idx_offset += draw_list->IdxBuffer.Size;
			vtx_offset += draw_list->VtxBuffer.Size;
		}

		m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();
		m_cmd_list->endMarker();
		m_cmd_list->close();
		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void ImGuiPass::cleanup() {
		if (ImGui::GetCurrentContext()) {
			ImGui::GetIO().Fonts->SetTexID(ImTextureID_Invalid);
		}

		m_binding_sets.clear();
		m_vtx_buffer.clear();
		m_idx_buffer.clear();
	}

	void ImGuiPass::onWindowResize(const Vector2i& window_extent) {
		(void)window_extent;
		createFramebuffer();
		m_pipeline = nullptr;
	}

	void ImGuiPass::reallocateBuffer(rhi::BufferHandle& buffer, size_t required_size, const size_t reallocate_size, const bool is_ib) {
		if (buffer && size_t(buffer->getDesc().byteSize) >= required_size) {
			return;
		}

		auto desc = rhi::BufferDesc()
			.setByteSize(static_cast<uint32_t>(reallocate_size))
			.setIsVertexBuffer(!is_ib)
			.setIsIndexBuffer(is_ib)
			.enableAutomaticStateTracking(is_ib ? rhi::ResourceStates::IndexBuffer : rhi::ResourceStates::VertexBuffer)
			.setDebugName(is_ib ? "ImGui Index Buffer" : "ImGui Vertex Buffer");

		buffer = m_rhi->getDevice()->createBuffer(desc);
	}

	void ImGuiPass::createBuffers() {
	}

	void ImGuiPass::createShaders() {
		auto vert_source = ReadShaderFile("engine/res/shaders/bin/imgui_pass.vert.spv");
		auto frag_source = ReadShaderFile("engine/res/shaders/bin/imgui_pass.frag.spv");
		if (vert_source.empty() || frag_source.empty()) {
			DO_ERROR("ImGuiPass: shader files are missing.");
			return;
		}

		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("ImGuiPass Vertex Shader"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("ImGuiPass Pixel Shader"),
			frag_source.data(), frag_source.size());
	}

	void ImGuiPass::createFontTexture() {
		if (!ImGui::GetCurrentContext()) {
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		unsigned char* pixels = nullptr;
		int width = 0;
		int height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		if (!pixels || width <= 0 || height <= 0) {
			return;
		}

		auto texture_desc = rhi::TextureDesc()
			.setDimension(rhi::TextureDimension::Texture2D)
			.setWidth(width)
			.setHeight(height)
			.setFormat(rhi::Format::RGBA8_UNORM)
			.enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource)
			.setDebugName("ImGui Font Texture");
		m_font_texture = m_rhi->getDevice()->createTexture(texture_desc);
		if (!m_font_texture) {
			return;
		}

		auto upload_cmd = m_rhi->getDevice()->createCommandList();
		upload_cmd->open();
		upload_cmd->writeTexture(m_font_texture, 0, 0, pixels, static_cast<size_t>(width) * 4u);
		upload_cmd->close();
		m_rhi->getDevice()->executeCommandList(upload_cmd);
		upload_cmd = nullptr;

		io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(m_font_texture.Get()));
	}

	void ImGuiPass::createFontSampler() {
		m_font_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
	}

	void ImGuiPass::createInputLayout() {
		rhi::VertexAttributeDesc attributes[] = {
			rhi::VertexAttributeDesc()
				.setName("a_Position")
				.setFormat(rhi::Format::RG32_FLOAT)
				.setOffset(offsetof(ImDrawVert, pos))
				.setElementStride(sizeof(ImDrawVert)),
			rhi::VertexAttributeDesc()
				.setName("a_UV")
				.setFormat(rhi::Format::RG32_FLOAT)
				.setOffset(offsetof(ImDrawVert, uv))
				.setElementStride(sizeof(ImDrawVert)),
			rhi::VertexAttributeDesc()
				.setName("a_Color")
				.setFormat(rhi::Format::RGBA8_UNORM)
				.setOffset(offsetof(ImDrawVert, col))
				.setElementStride(sizeof(ImDrawVert)),
		};

		m_input_layout = m_rhi->getDevice()->createInputLayout(
			attributes,
			static_cast<ui32>(std::size(attributes)),
			m_vertex_shader);
	}

	void ImGuiPass::createBindingLayout() {
		auto desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 4))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(0))
			.addItem(rhi::BindingLayoutItem::Sampler(0));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(desc);
	}

	void ImGuiPass::createFramebuffer() {
		m_render_target = getTextureResource(kOutputImGuiColorResourceName);
		m_framebuffer = nullptr;

		auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(m_render_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void ImGuiPass::createGraphicsPipeline() {
		if (!m_framebuffer || !m_input_layout || !m_vertex_shader || !m_pixel_shader || !m_binding_layout) {
			return;
		}

		auto framebuffer_info = m_framebuffer->getFramebufferInfo();

		auto pipeline_desc = rhi::GraphicsPipelineDesc()
			.setInputLayout(m_input_layout)
			.setVertexShader(m_vertex_shader)
			.setPixelShader(m_pixel_shader)
			.addBindingLayout(m_binding_layout)
			.setPrimType(rhi::PrimitiveType::TriangleList);

		rhi::DepthStencilState depth_stencil_state;
		depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();

		rhi::BlendState blend_state;
		blend_state.targets[0]
			.enableBlend()
			.setSrcBlend(rhi::BlendFactor::SrcAlpha)
			.setDestBlend(rhi::BlendFactor::InvSrcAlpha)
			.setBlendOp(rhi::BlendOp::Add)
			.setSrcBlendAlpha(rhi::BlendFactor::One)
			.setDestBlendAlpha(rhi::BlendFactor::InvSrcAlpha)
			.setBlendOpAlpha(rhi::BlendOp::Add);

		rhi::RasterState raster_state;
		raster_state.setCullNone().setScissorEnable(true);

		rhi::RenderState render_state;
		render_state.setBlendState(blend_state);
		render_state.setDepthStencilState(depth_stencil_state);
		render_state.setRasterState(raster_state);
		pipeline_desc.setRenderState(render_state);

		m_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, framebuffer_info);
	}

	void ImGuiPass::updateGeometry() {
		ImDrawData* draw_data = ImGui::GetDrawData();
		if (!draw_data || draw_data->TotalVtxCount <= 0 || draw_data->TotalIdxCount <= 0) {
			return;
		}

		const size_t vertex_byte_size = static_cast<size_t>(draw_data->TotalVtxCount) * sizeof(ImDrawVert);
		const size_t index_byte_size = static_cast<size_t>(draw_data->TotalIdxCount) * sizeof(ImDrawIdx);
		reallocateBuffer(m_vertex_buffer, vertex_byte_size, (static_cast<size_t>(draw_data->TotalVtxCount) + 5000) * sizeof(ImDrawVert), false);
		reallocateBuffer(m_index_buffer, index_byte_size, (static_cast<size_t>(draw_data->TotalIdxCount) + 10000) * sizeof(ImDrawIdx), true);

		m_vtx_buffer.resize(draw_data->TotalVtxCount);
		m_idx_buffer.resize(draw_data->TotalIdxCount);

		ImDrawVert* vtx_dst = m_vtx_buffer.data();
		ImDrawIdx* idx_dst = m_idx_buffer.data();
		for (int n = 0; n < draw_data->CmdListsCount; ++n) {
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			std::memcpy(vtx_dst, cmd_list->VtxBuffer.Data, static_cast<size_t>(cmd_list->VtxBuffer.Size) * sizeof(ImDrawVert));
			std::memcpy(idx_dst, cmd_list->IdxBuffer.Data, static_cast<size_t>(cmd_list->IdxBuffer.Size) * sizeof(ImDrawIdx));
			vtx_dst += cmd_list->VtxBuffer.Size;
			idx_dst += cmd_list->IdxBuffer.Size;
		}

		m_cmd_list->writeBuffer(m_vertex_buffer, m_vtx_buffer.data(), vertex_byte_size);
		m_cmd_list->writeBuffer(m_index_buffer, m_idx_buffer.data(), index_byte_size);
	}

	rhi::IBindingSet* ImGuiPass::getBindingSet(rhi::ITexture* texture) {
		if (!texture || !m_binding_layout || !m_font_sampler) {
			return nullptr;
		}

		const auto it = m_binding_sets.find(texture);
		if (it != m_binding_sets.end()) {
			return it->second.Get();
		}

		auto desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::PushConstants(0, sizeof(float) * 4))
			.addItem(rhi::BindingSetItem::Texture_SRV(0, texture))
			.addItem(rhi::BindingSetItem::Sampler(0, m_font_sampler));

		auto binding = m_rhi->getDevice()->createBindingSet(desc, m_binding_layout);
		if (!binding) {
			return nullptr;
		}

		auto* binding_ptr = binding.Get();
		m_binding_sets.emplace(texture, binding);
		return binding_ptr;
	}

} // dodoe

#endif
