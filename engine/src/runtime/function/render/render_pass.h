// do@Redlive

#pragma once

#include "dopch.h"

#include "interface/rhi.h"
#include "interface/rhi_context.h"
#include "framework/texture_manager.h"

namespace dodoe {
	class RenderGraph;
	class RenderGraphPass;

	enum class RenderGraphResourceKind {
		Texture,
		Buffer,
	};

	struct TextureResourceDesc {
		rhi::Format format{};
		bool viewport_relative{false};
		bool backbuffer_relative{false};
		bool shader_resource{true};
		bool render_target{false};
		bool depth_stencil{false};
		std::string debug_name{};
	};

	struct BufferResourceDesc {
		size_t byte_size{0};
		bool constant_buffer{false};
		bool storage_buffer{false};
		bool vertex_buffer{false};
		bool index_buffer{false};
		std::string debug_name{};
	};

	struct RenderPassResourceUsage {
		std::string name{};
		RenderGraphResourceKind kind{RenderGraphResourceKind::Texture};
		TextureResourceDesc texture_desc{};
		BufferResourceDesc buffer_desc{};
	};

	struct RenderGraphResource {
		std::string name{};
		RenderGraphResourceKind kind{RenderGraphResourceKind::Texture};
		TextureResourceDesc texture_desc{};
		rhi::TextureHandle texture{};
		BufferResourceDesc buffer_desc{};
		rhi::BufferHandle buffer{};
	};

	class RenderPass {
		friend class RenderGraphPass;

		RenderGraphPass* m_pass{nullptr};
		RenderGraph* m_graph{nullptr};
	protected:
		RhiContext* m_rhi{nullptr};
	public:
		virtual ~RenderPass() = default;

		virtual bool isNeedRenderPass() const { return true; }
		virtual void setup() {}
		virtual void execute(size_t index) = 0;
		virtual void cleanup() {}
		virtual void onViewportResize(const Vector2i& viewport_extent) {}
		virtual void onWindowResize(const Vector2i& window_extent) {}

	protected:
		[[nodiscard]] RenderGraph& graph();
		[[nodiscard]] const RenderGraph& graph() const;
		[[nodiscard]] rhi::TextureHandle getTextureResource(const std::string& name) const;
		[[nodiscard]] rhi::BufferHandle getBufferResource(const std::string& name) const;

	private:
		void bind(RenderGraphPass& pass, RenderGraph& graph);

	};

	class RenderGraphPass {
	protected:
		std::string m_name{};
		Ref<RenderPass> m_implementation{};
		std::vector<RenderPassResourceUsage> m_read_resources{};
		std::vector<RenderPassResourceUsage> m_write_resources{};

	public:
		RenderGraphPass(std::string name, Ref<RenderPass> pass_implementation);
		~RenderGraphPass() = default;

		[[nodiscard]] const std::string& name() const { return m_name; }
		[[nodiscard]] RenderPass* implementation() const { return m_implementation.get(); }

		[[nodiscard]] bool isNeedRenderPass() const;

		void setup();
		void execute(size_t index);
		void cleanup();
		void onViewportResize(const Vector2i& viewport_extent);
		void onWindowResize(const Vector2i& window_extent);
		RenderGraphPass& addTextureRead(const std::string& name, const TextureResourceDesc& desc = {});
		RenderGraphPass& addTextureWrite(const std::string& name, const TextureResourceDesc& desc = {});
		RenderGraphPass& addBufferRead(const std::string& name, const BufferResourceDesc& desc = {});
		RenderGraphPass& addBufferWrite(const std::string& name, const BufferResourceDesc& desc = {});
		[[nodiscard]] const std::vector<RenderPassResourceUsage>& getReadResources() const { return m_read_resources; }
		[[nodiscard]] const std::vector<RenderPassResourceUsage>& getWriteResources() const { return m_write_resources; }

	private:
		friend class RenderGraph;
		void bind(RenderGraph& graph);
		void clearResourceUsages();
	};

} // dodoe
