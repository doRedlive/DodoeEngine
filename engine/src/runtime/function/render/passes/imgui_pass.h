// do@GreenMuffin

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "imgui/imgui.h"

namespace dodoe {

	class RhiContext;

	class ImGuiPass : public RenderPass {
		inline static const std::string kOutputImGuiColorResourceName = "ImGuiColor";

		rhi::BufferHandle m_vertex_buffer;
		rhi::BufferHandle m_index_buffer;
		rhi::ShaderHandle m_vertex_shader;
		rhi::ShaderHandle m_pixel_shader;
		rhi::TextureHandle m_font_texture;
		rhi::InputLayoutHandle m_input_layout;
		rhi::BindingLayoutHandle m_binding_layout;
		rhi::GraphicsPipelineHandle m_pipeline;
		rhi::SamplerHandle m_font_sampler;
		rhi::CommandListHandle m_cmd_list;
		rhi::TextureHandle m_render_target{};
		rhi::FramebufferHandle m_framebuffer{};
		std::unordered_map<rhi::ITexture*, rhi::BindingSetHandle> m_binding_sets{};

        std::vector<ImDrawVert> m_vtx_buffer;
        std::vector<ImDrawIdx> m_idx_buffer;
	public:
		explicit ImGuiPass(RhiContext* rhi);

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;
		void onWindowResize(const Vector2i& window_extent) override;

	private:
		void reallocateBuffer(rhi::BufferHandle& buffer, size_t required_size, size_t reallocate_size, bool is_ib);
		void updateGeometry();
		void createBuffers();
		void createShaders(); 
		void createFontTexture();
		void createFontSampler();
		void createInputLayout();
		void createBindingLayout();
		void createFramebuffer();
		void createGraphicsPipeline();

		rhi::IBindingSet* getBindingSet(rhi::ITexture* texture);
	};

} // dodoe
