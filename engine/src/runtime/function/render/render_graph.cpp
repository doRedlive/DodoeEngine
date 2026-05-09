// do@Redlive

#include "render_graph.h"

#include "passes/main_camera_pass.h"
#include "passes/skybox_pass.h"
#include "passes/directional_light_shadow_pass.h"
#include "passes/deferred_light_pass.h"
#include "passes/tone_mapping_pass.h"
#include "passes/color_grading_pass.h"
#include "passes/point_light_shadow_pass.h"
#include "passes/sprite_pass.h"
#include "passes/fxaa_pass.h"
#include "passes/imgui_pass.h"
#include "passes/combine_pass.h"
#include "framework/camera.h"

#include "runtime/core/utils/common.h"

namespace dodoe {
		
	Scope<RenderGraph> RenderGraph::create(const RenderGraphCreateInfo& info) {
		if (auto graph = create_scope<RenderGraph>(); graph->initialize(info)) 
			return graph;
		return nullptr;
	}

	void RenderGraph::destroy(Scope<RenderGraph>& graph) {
		if (!graph) { return; }
		graph->shutdown();
		graph.reset();
	}

	bool RenderGraph::initialize(const RenderGraphCreateInfo& info) {
		m_rhi = info.rhi;
		m_camera = info.camera;
		m_descriptor_manager = info.descriptor_manager;
		if (m_camera) {
			const auto& logical_size = m_camera->getLogicalSize();
			m_viewport_rect = Rect(Vector2f(0.0f, 0.0f), logical_size);
			m_viewport_extent = {
				static_cast<int>((std::max)(1.0f, logical_size.x)),
				static_cast<int>((std::max)(1.0f, logical_size.y))
			};
		}
		m_backbuffer_extent = m_rhi ? m_rhi->getSwapchainExtent2d() : Vector2i(1, 1);
		return m_rhi != nullptr;
	}

	RenderGraphPass& RenderGraph::addPass(const std::string& name, Ref<RenderPass> pass_implementation) {
		auto existing = m_pass_map.find(name);
		if (existing != m_pass_map.end()) {
			DO_ERROR("RenderGraph: pass '{}' already exists.", name);
			return *existing->second;
		}

		auto pass = create_scope<RenderGraphPass>(name, std::move(pass_implementation));
		auto* pass_ptr = pass.get();
		pass_ptr->bind(*this);
		m_pass_map.emplace(name, std::move(pass));
		m_registered_passes.push_back(pass_ptr);
		return *pass_ptr;
	}

	RenderGraphPass* RenderGraph::findPass(const std::string& name) {
		const auto it = m_pass_map.find(name);
		return it != m_pass_map.end() ? it->second.get() : nullptr;
	}

	const RenderGraphPass* RenderGraph::findPass(const std::string& name) const {
		const auto it = m_pass_map.find(name);
		return it != m_pass_map.end() ? it->second.get() : nullptr;
	}

	rhi::TextureHandle RenderGraph::getTextureResource(const std::string& name) const {
		const auto it = m_graph_render_res_umap.find(name);
		return (it != m_graph_render_res_umap.end() && it->second.kind == RenderGraphResourceKind::Texture) ? it->second.texture : nullptr;
	}

	rhi::BufferHandle RenderGraph::getBufferResource(const std::string& name) const {
		const auto it = m_graph_render_res_umap.find(name);
		return (it != m_graph_render_res_umap.end() && it->second.kind == RenderGraphResourceKind::Buffer) ? it->second.buffer : nullptr;
	}
	
	void RenderGraph::shutdown() {
		for (auto* pass : m_registered_passes) {
			pass->cleanup();
		}
		m_sorted_passes.clear();
		m_registered_passes.clear();
		m_graph_render_res_umap.clear();
		m_pass_map.clear();
	}

	void RenderGraph::setup() {
		TextureResourceDesc main_camera_color_desc{};
		main_camera_color_desc.format = rhi::Format::RGBA8_UNORM;
		main_camera_color_desc.viewport_relative = true;
		main_camera_color_desc.shader_resource = true;
		main_camera_color_desc.render_target = true;
		main_camera_color_desc.debug_name = "MainCameraPass Scene Target";

		TextureResourceDesc main_camera_hdr_color_desc{};
		main_camera_hdr_color_desc.format = rhi::Format::RGBA16_FLOAT;
		main_camera_hdr_color_desc.viewport_relative = true;
		main_camera_hdr_color_desc.shader_resource = true;
		main_camera_hdr_color_desc.render_target = true;
		main_camera_hdr_color_desc.debug_name = "MainCameraPass HDR Color Target";

		TextureResourceDesc main_camera_tonemapped_color_desc{};
		main_camera_tonemapped_color_desc.format = rhi::Format::RGBA8_UNORM;
		main_camera_tonemapped_color_desc.viewport_relative = true;
		main_camera_tonemapped_color_desc.shader_resource = true;
		main_camera_tonemapped_color_desc.render_target = true;
		main_camera_tonemapped_color_desc.debug_name = "MainCameraPass ToneMapped Color Target";

		TextureResourceDesc main_camera_fxaa_color_desc{};
		main_camera_fxaa_color_desc.format = rhi::Format::RGBA8_UNORM;
		main_camera_fxaa_color_desc.viewport_relative = true;
		main_camera_fxaa_color_desc.shader_resource = true;
		main_camera_fxaa_color_desc.render_target = true;
		main_camera_fxaa_color_desc.debug_name = "MainCameraPass FXAA Color Target";

		TextureResourceDesc main_camera_albedo_desc{};
		main_camera_albedo_desc.format = rhi::Format::RGBA8_UNORM;
		main_camera_albedo_desc.viewport_relative = true;
		main_camera_albedo_desc.shader_resource = true;
		main_camera_albedo_desc.render_target = true;
		main_camera_albedo_desc.debug_name = "MainCameraPass Albedo Target";

		TextureResourceDesc main_camera_normal_desc{};
		main_camera_normal_desc.format = rhi::Format::RGBA16_FLOAT;
		main_camera_normal_desc.viewport_relative = true;
		main_camera_normal_desc.shader_resource = true;
		main_camera_normal_desc.render_target = true;
		main_camera_normal_desc.debug_name = "MainCameraPass Normal Target";

		TextureResourceDesc main_camera_position_desc{};
		main_camera_position_desc.format = rhi::Format::RGBA32_FLOAT;
		main_camera_position_desc.viewport_relative = true;
		main_camera_position_desc.shader_resource = true;
		main_camera_position_desc.render_target = true;
		main_camera_position_desc.debug_name = "MainCameraPass Position Target";

		TextureResourceDesc main_camera_material_desc{};
		main_camera_material_desc.format = rhi::Format::RGBA8_UNORM;
		main_camera_material_desc.viewport_relative = true;
		main_camera_material_desc.shader_resource = true;
		main_camera_material_desc.render_target = true;
		main_camera_material_desc.debug_name = "MainCameraPass Material Target";

		TextureResourceDesc main_camera_depth_desc{};
		main_camera_depth_desc.format = rhi::Format::D32;
		main_camera_depth_desc.viewport_relative = true;
		main_camera_depth_desc.render_target = true;
		main_camera_depth_desc.depth_stencil = true;
		main_camera_depth_desc.shader_resource = true;
		main_camera_depth_desc.debug_name = "MainCameraPass Depth Target";

		TextureResourceDesc directional_shadow_desc{};
		directional_shadow_desc.format = rhi::Format::D32;
		directional_shadow_desc.viewport_relative = true;
		directional_shadow_desc.render_target = true;
		directional_shadow_desc.depth_stencil = true;
		directional_shadow_desc.shader_resource = true;
		directional_shadow_desc.debug_name = "DirectionalLightShadowPass Depth Target";

		TextureResourceDesc imgui_color_desc{};
		imgui_color_desc.format = rhi::Format::RGBA8_UNORM;
		imgui_color_desc.backbuffer_relative = true;
		imgui_color_desc.render_target = true;
		imgui_color_desc.depth_stencil = false;
		imgui_color_desc.shader_resource = true;
		imgui_color_desc.debug_name = "ImGuiPass Color Target";

		addPass("MainCameraPass", create_ref<MainCameraPass>(m_rhi, m_descriptor_manager))
			.addTextureWrite("MainCameraAlbedo", main_camera_albedo_desc)
			.addTextureWrite("MainCameraNormal", main_camera_normal_desc)
			.addTextureWrite("MainCameraPosition", main_camera_position_desc)
			.addTextureWrite("MainCameraMaterial", main_camera_material_desc)
			.addTextureWrite("MainCameraDepth", main_camera_depth_desc);

		addPass("DirectionalLightShadowPass", create_ref<DirectionalLightShadowPass>(m_rhi))
			.addTextureWrite("ShadowMap", directional_shadow_desc);

		addPass("PointLightShadowPass", create_ref<PointLightShadowPass>(m_rhi));

		addPass("SkyboxPass", create_ref<SkyboxPass>(m_rhi))
			.addTextureRead("MainCameraDepth", main_camera_depth_desc)
			.addTextureWrite("MainCameraHdrColor", main_camera_hdr_color_desc);

		addPass("DeferredLightPass", create_ref<DeferredLightPass>(m_rhi))
			.addTextureRead("MainCameraAlbedo", main_camera_albedo_desc)
			.addTextureRead("MainCameraNormal", main_camera_normal_desc)
			.addTextureRead("MainCameraPosition", main_camera_position_desc)
			.addTextureRead("MainCameraMaterial", main_camera_material_desc)
			.addTextureRead("ShadowMap", directional_shadow_desc)
			.addTextureWrite("MainCameraHdrColor", main_camera_hdr_color_desc);

		addPass("ToneMappingPass", create_ref<ToneMappingPass>(m_rhi))
			.addTextureRead("MainCameraHdrColor", main_camera_hdr_color_desc)
			.addTextureWrite("MainCameraToneMappedColor", main_camera_tonemapped_color_desc);

		addPass("ColorGradingPass", create_ref<ColorGradingPass>(m_rhi))
			.addTextureRead("MainCameraToneMappedColor", main_camera_tonemapped_color_desc)
			.addTextureWrite("MainCameraColor", main_camera_color_desc);

		addPass("SpritePass", create_ref<SpritePass>(m_rhi, m_descriptor_manager))
			.addTextureWrite("MainCameraColor", main_camera_color_desc)
			.addTextureRead("MainCameraDepth", main_camera_depth_desc); // For depth test

		addPass("FXAAPass", create_ref<FXAAPass>(m_rhi))
			.addTextureRead("MainCameraColor", main_camera_color_desc)
			.addTextureWrite("MainCameraFxaaColor", main_camera_fxaa_color_desc);

		addPass("ImGuiPass", create_ref<ImGuiPass>(m_rhi))
			.addTextureWrite("ImGuiColor", imgui_color_desc);

		addPass("CombinePass", create_ref<CombinePass>(m_rhi))
			.addTextureRead("MainCameraFxaaColor", main_camera_fxaa_color_desc)
			.addTextureRead("ImGuiColor", imgui_color_desc);
	}

	void RenderGraph::compile() {
		m_sorted_passes.clear();
		if (m_registered_passes.empty()) {
			return;
		}

		buildDeclaredResources();

		const size_t pass_count = m_registered_passes.size();
		std::vector<std::unordered_set<size_t>> outgoing_edges(pass_count);
		std::vector<size_t> indegree(pass_count, 0);
		std::unordered_map<std::string, std::vector<size_t>> writers_by_resource{};

		for (size_t pass_index = 0; pass_index < pass_count; ++pass_index) {
			for (const auto& write_resource : m_registered_passes[pass_index]->getWriteResources()) {
				writers_by_resource[write_resource.name].push_back(pass_index);
			}
		}

		auto add_dependency = [&](const size_t from, const size_t to) {
			if (from == to) {
				return;
			}
			if (outgoing_edges[from].insert(to).second) {
				++indegree[to];
			}
		};

		for (const auto& [resource_name, writers] : writers_by_resource) {
			if (writers.size() <= 1) {
				continue;
			}

			for (size_t writer_index = 1; writer_index < writers.size(); ++writer_index) {
				add_dependency(writers[writer_index - 1], writers[writer_index]);
			}
		}

		for (size_t pass_index = 0; pass_index < pass_count; ++pass_index) {
			for (const auto& read_resource : m_registered_passes[pass_index]->getReadResources()) {
				const auto writer_it = writers_by_resource.find(read_resource.name);
				if (writer_it == writers_by_resource.end() || writer_it->second.empty()) {
					continue;
				}

				add_dependency(writer_it->second.back(), pass_index);
			}
		}

		std::vector<bool> emitted(pass_count, false);
		bool has_cycle = false;
		for (size_t emitted_count = 0; emitted_count < pass_count; ++emitted_count) {
			size_t candidate_index = pass_count;
			for (size_t pass_index = 0; pass_index < pass_count; ++pass_index) {
				if (!emitted[pass_index] && indegree[pass_index] == 0) {
					candidate_index = pass_index;
					break;
				}
			}

			if (candidate_index == pass_count) {
				DO_ERROR("RenderGraph: dependency cycle detected, falling back to registration order.");
				m_sorted_passes = m_registered_passes;
				has_cycle = true;
				break;
			}

			emitted[candidate_index] = true;
			m_sorted_passes.push_back(m_registered_passes[candidate_index]);
			for (const size_t next_index : outgoing_edges[candidate_index]) {
				if (indegree[next_index] > 0) {
					--indegree[next_index];
				}
			}
		}

		if (has_cycle) {
			m_sorted_passes = m_registered_passes;
		}

		rebuildAllocatedResources();

		const auto& setup_order = m_sorted_passes.empty() ? m_registered_passes : m_sorted_passes;
		for (auto* pass : setup_order) {
			pass->setup();
		}
	}

	void RenderGraph::execute(uint32_t swapchain_image_index) {
		const auto& execution_order = m_sorted_passes.empty() ? m_registered_passes : m_sorted_passes;
		for (auto* pass : execution_order) {
			pass->execute(swapchain_image_index);
		}
	}

	void RenderGraph::onViewportResize(const Rect& viewport) {
		m_viewport_rect = viewport;
		m_viewport_extent = viewport.size;
		rebuildAllocatedResources(ResourceRebuildMode::ViewportRelativeOnly);
		for (auto* pass : m_registered_passes) {
			pass->onViewportResize(viewport.size);
		}
	}

	void RenderGraph::onWindowResize(const Vector2i& size) {
		(void)size;
		m_backbuffer_extent = m_rhi->getSwapchainExtent2d();
		rebuildAllocatedResources(ResourceRebuildMode::BackbufferRelativeOnly);
		for (auto* pass : m_registered_passes) {
			pass->onWindowResize(m_backbuffer_extent);
		}
	}

	void RenderGraph::buildDeclaredResources() {
		m_graph_render_res_umap.clear();

		const auto& pass_order = m_sorted_passes.empty() ? m_registered_passes : m_sorted_passes;
		for (auto* pass : pass_order) {
			for (const auto& usage : pass->getReadResources()) {
				auto& resource = m_graph_render_res_umap[usage.name];
				resource.name = usage.name;
				resource.kind = usage.kind;
				if (usage.kind == RenderGraphResourceKind::Texture) {
					resource.texture_desc = usage.texture_desc;
				}
				else {
					resource.buffer_desc = usage.buffer_desc;
				}
			}

			for (const auto& usage : pass->getWriteResources()) {
				auto& resource = m_graph_render_res_umap[usage.name];
				resource.name = usage.name;
				resource.kind = usage.kind;
				if (usage.kind == RenderGraphResourceKind::Texture) {
					resource.texture_desc = usage.texture_desc;
				}
				else {
					resource.buffer_desc = usage.buffer_desc;
				}
			}
		}
	}

	void RenderGraph::rebuildAllocatedResources(const ResourceRebuildMode mode) {
		for (auto& [_, resource] : m_graph_render_res_umap) {
			if (resource.kind == RenderGraphResourceKind::Texture) {
				const auto& desc = resource.texture_desc;
				const bool rebuild_this_resource =
					mode == ResourceRebuildMode::All ||
					(mode == ResourceRebuildMode::ViewportRelativeOnly && desc.viewport_relative) ||
					(mode == ResourceRebuildMode::BackbufferRelativeOnly && desc.backbuffer_relative);
				if (!rebuild_this_resource) {
					continue;
				}

				resource.texture = nullptr;
				if (!desc.render_target && !desc.depth_stencil) {
					continue;
				}

				auto texture_desc = rhi::TextureDesc()
					.setDimension(rhi::TextureDimension::Texture2D)
					.setFormat(desc.format)
					.setDebugName(desc.debug_name);
				if (desc.viewport_relative) {
					texture_desc.setWidth(m_viewport_extent.x).setHeight(m_viewport_extent.y);
				}
				if (desc.backbuffer_relative) {
					texture_desc.setWidth(m_backbuffer_extent.x).setHeight(m_backbuffer_extent.y);
				}
				if (desc.render_target) {
					texture_desc.setIsRenderTarget(true);
				}
				if (desc.depth_stencil) {
					texture_desc.setIsTypeless(false);
				}
				if (desc.depth_stencil) {
					texture_desc.enableAutomaticStateTracking(rhi::ResourceStates::DepthWrite);
				}
				else if (desc.shader_resource) {
					texture_desc.enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource);
				}

				resource.texture = m_rhi->getDevice()->createTexture(texture_desc);
			}
			else {
				if (mode != ResourceRebuildMode::All) {
					continue;
				}

				resource.buffer = nullptr;
				const auto& desc = resource.buffer_desc;
				if (desc.byte_size == 0) {
					continue;
				}

				auto buffer_desc = rhi::BufferDesc()
					.setByteSize(static_cast<uint32_t>(desc.byte_size))
					.setDebugName(desc.debug_name)
					.setIsConstantBuffer(desc.constant_buffer)
					.setIsVertexBuffer(desc.vertex_buffer)
					.setIsIndexBuffer(desc.index_buffer);
				resource.buffer = m_rhi->getDevice()->createBuffer(buffer_desc);
			}
		}
	}

} // dodoe
