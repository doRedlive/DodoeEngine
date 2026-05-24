// do@Redlive

#pragma once

#include "dopch.h"

#include "render_pass.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

	class Camera;
	class RhiContext;
	class UiSystem;
	class DescriptorTableManager;

	struct RenderGraphCreateInfo {
		RhiContext* rhi{nullptr};
		Camera* camera{nullptr};
		UiSystem* ui_system{nullptr};
		DescriptorTableManager* descriptor_manager{nullptr};
	};

	class RenderGraph : public Managed<RenderGraph, RenderGraphCreateInfo> {
		friend class Managed<RenderGraph, RenderGraphCreateInfo>;
		enum class ResourceRebuildMode {
			All,
			ViewportRelativeOnly,
			BackbufferRelativeOnly,
		};

		RhiContext* m_rhi{nullptr};

		Camera* m_camera{nullptr};
		DescriptorTableManager* m_descriptor_manager{nullptr};
		Rect m_viewport_rect{};
		Vector2i m_viewport_extent{1, 1};
		Vector2i m_backbuffer_extent{1, 1};

		std::unordered_map<std::string, Scope<RenderGraphPass>> m_pass_map{};
		std::vector<RenderGraphPass*> m_registered_passes{};
		std::vector<RenderGraphPass*> m_sorted_passes{};
		std::unordered_map<std::string, RenderGraphResource> m_graph_render_res_umap{};
	public:


		RenderGraphPass& addPass(const std::string& name, Ref<RenderPass> pass_implementation);
		[[nodiscard]] RenderGraphPass* findPass(const std::string& name);
		[[nodiscard]] const RenderGraphPass* findPass(const std::string& name) const;
		[[nodiscard]] rhi::TextureHandle getTextureResource(const std::string& name) const;
		[[nodiscard]] rhi::BufferHandle getBufferResource(const std::string& name) const;
		[[nodiscard]] const std::unordered_map<std::string, RenderGraphResource>& getRenderResources() const { return m_graph_render_res_umap; }
		[[nodiscard]] const Rect& getViewportRect() const { return m_viewport_rect; }
		[[nodiscard]] const Vector2i& getViewportExtent() const { return m_viewport_extent; }
		[[nodiscard]] const Vector2i& getBackbufferExtent() const { return m_backbuffer_extent; }

		void onViewportResize(const Rect& viewport);
		void onWindowResize(const Vector2i& size);
		
		void compile();
		void execute(uint32_t swapchain_image_index = 0);
			
	private:
		bool initialize(const RenderGraphCreateInfo& info);
		void shutdown();
		void buildDeclaredResources();
		void rebuildAllocatedResources(ResourceRebuildMode mode = ResourceRebuildMode::All);
	};

} // dodoe
