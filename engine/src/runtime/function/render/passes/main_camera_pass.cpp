// Created by Redlive on 2026/4/6.

#include "main_camera_pass.h"

#include "../framework/camera.h"
#include "../render_graph.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/function/render/framework/texture_manager.h"

#include "glm/gtc/matrix_inverse.hpp"

namespace dodoe {

	MainCameraPass::MainCameraPass(RhiContext* rhi, Camera* camera, DescriptorTableManager* descriptor_manager)
		: m_camera(camera), m_descriptor_table(descriptor_manager) {
		m_rhi = rhi;
	}

	void MainCameraPass::setup() {
		createFramebuffer();
		createShaders();
		createInputLayout();
		createBindingLayout();
		createBuffers();
		createSampler();
		createBindingSet();
		createGraphicsPipeline();

		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void MainCameraPass::cleanup() {
		draw_vertices_.clear();
		model_vertex_cache_.clear();
		m_render_target = nullptr;
		m_depth_target = nullptr;
		m_framebuffer = nullptr;
	}

	void MainCameraPass::execute(size_t index) {
		(void)index;
		if (!m_graphics_pipeline) { createGraphicsPipeline(); }

		rebuildDrawVerticesFromScene();

		m_cmd_list->open();
		m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->setTextureState(m_depth_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
		m_cmd_list->commitBarriers();
		m_cmd_list->clearTextureFloat(m_render_target, rhi::AllSubresources, rhi::Color(0.08f, 0.09f, 0.11f, 1.0f));
		m_cmd_list->clearDepthStencilTexture(m_depth_target, rhi::AllSubresources, true, 1.0f, false, 0);

		if (draw_vertices_.empty()) {
			m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
			m_cmd_list->commitBarriers();
			m_cmd_list->close();
			m_rhi->getDevice()->executeCommandList(m_cmd_list);
			return;
		}

		const Matrix4f mvp = m_camera->getViewProjectionMatrix();
		m_cmd_list->writeBuffer(m_constant_buffer, &mvp, sizeof(Matrix4f));
		m_cmd_list->writeBuffer(m_vertex_buffer, draw_vertices_.data(), static_cast<ui32>(draw_vertices_.size() * sizeof(MainCameraVertex)));

		auto graphics_state = rhi::GraphicsState()
			.setPipeline(m_graphics_pipeline)
			.setFramebuffer(m_framebuffer)
			.setViewport(rhi::ViewportState().addViewportAndScissorRect(
				rhi::Viewport(static_cast<float>(graph().getViewportExtent().x), static_cast<float>(graph().getViewportExtent().y))))
			.addBindingSet(m_binding_set)
			.addBindingSet(m_descriptor_table->getDescriptorTable())
			.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(m_vertex_buffer).setSlot(0).setOffset(0));
		m_cmd_list->setGraphicsState(graphics_state);

		auto draw_args = rhi::DrawArguments()
			.setVertexCount(static_cast<ui32>(draw_vertices_.size()));
		m_cmd_list->draw(draw_args);
		m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();

		m_cmd_list->close();
		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void MainCameraPass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		createFramebuffer();
		m_graphics_pipeline = nullptr;
	}

	void MainCameraPass::createShaders() {
		auto vert_source = dodoe::ReadShaderFile("engine/res/shaders/main_camera_pass.vert.spv");
		auto frag_source = dodoe::ReadShaderFile("engine/res/shaders/main_camera_pass.frag.spv");
		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("MainCameraPass VS"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("MainCameraPass PS"),
			frag_source.data(), frag_source.size());

		if (!m_vertex_shader || !m_pixel_shader) {
			DO_ASSERT(false, "MainCameraPass: createShader failed.");
			return;
		}
	}

	void MainCameraPass::createBuffers() {
		auto m_constant_bufferdesc = rhi::BufferDesc()
			.setByteSize(sizeof(Matrix4f))
			.setIsConstantBuffer(true)
			.setIsVolatile(true)
			.setMaxVersions(16);
		m_constant_buffer = m_rhi->getDevice()->createBuffer(m_constant_bufferdesc);

		auto vertex_buffer_desc = rhi::BufferDesc()
			.setByteSize(sizeof(MainCameraVertex) * 1024 * 1024)
			.setIsVertexBuffer(true)
			.enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
			.setDebugName("MainCameraPass Vertex Buffer");
		m_vertex_buffer = m_rhi->getDevice()->createBuffer(vertex_buffer_desc);
	}

	void MainCameraPass::createSampler() {
		m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
	}

	void MainCameraPass::createBindingSet() {
		auto m_binding_setdesc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::ConstantBuffer(0, m_constant_buffer))
			.addItem(rhi::BindingSetItem::Sampler(0, m_sampler));
		m_binding_set = m_rhi->getDevice()->createBindingSet(m_binding_setdesc, m_binding_layout);
	}
	
	void MainCameraPass::createInputLayout() {
		rhi::VertexAttributeDesc attributes[] = {
			rhi::VertexAttributeDesc()
				.setName("a_Position")
				.setFormat(rhi::Format::RGB32_FLOAT)
				.setOffset(offsetof(MainCameraVertex, position))
				.setElementStride(sizeof(MainCameraVertex)),
			rhi::VertexAttributeDesc()
				.setName("a_Normal")
				.setFormat(rhi::Format::RGB32_FLOAT)
				.setOffset(offsetof(MainCameraVertex, normal))
				.setElementStride(sizeof(MainCameraVertex)),
			rhi::VertexAttributeDesc()
				.setName("a_UV")
				.setFormat(rhi::Format::RG32_FLOAT)
				.setOffset(offsetof(MainCameraVertex, uv))
				.setElementStride(sizeof(MainCameraVertex)),
			rhi::VertexAttributeDesc()
				.setName("a_TexIndex")
				.setFormat(rhi::Format::R32_UINT)
				.setOffset(offsetof(MainCameraVertex, texture_index))
				.setElementStride(sizeof(MainCameraVertex)),
		};

		m_input_layout = m_rhi->getDevice()->createInputLayout(
			attributes,
			static_cast<ui32>(std::size(attributes)),
			m_vertex_shader
		);
	}

	void MainCameraPass::createBindingLayout() {
		auto layout_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0))
			.addItem(rhi::BindingLayoutItem::Sampler(0));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(layout_desc);
	}

	void MainCameraPass::createGraphicsPipeline() {
		if (!m_framebuffer || !m_input_layout || !m_vertex_shader || !m_pixel_shader || !m_binding_layout) {
			return;
		}
		auto framebuffer_info = m_framebuffer->getFramebufferInfo();

		auto pipeline_desc = rhi::GraphicsPipelineDesc()
			.setInputLayout(m_input_layout)
			.setVertexShader(m_vertex_shader)
			.setPixelShader(m_pixel_shader)
			.addBindingLayout(m_binding_layout)
			.addBindingLayout(m_descriptor_table->getDescriptorTable()->getLayout());

		rhi::DepthStencilState depth_stencil_state;
		depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(rhi::ComparisonFunc::Less).disableStencil();
		rhi::RenderState render_state;
		render_state.setDepthStencilState(depth_stencil_state);
		pipeline_desc.setRenderState(render_state);

		m_graphics_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, framebuffer_info);
	}

	void MainCameraPass::createFramebuffer() {
		m_render_target = nullptr;
		m_depth_target = nullptr;
		m_framebuffer = nullptr;

		m_render_target = getTextureResource(kSceneColorName);
		m_depth_target = getTextureResource(kSceneDepthName);
		if (!m_render_target || !m_depth_target) {
			return;
		}

		auto framebuffer_desc = rhi::FramebufferDesc()
			.addColorAttachment(m_render_target)
			.setDepthAttachment(m_depth_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void MainCameraPass::rebuildDrawVerticesFromScene() {
		draw_vertices_.clear();

		const auto& packets = g_RenderResource->mainCameraPackets();
		for (const auto& packet : packets) {
			appendModelVertices(packet, draw_vertices_);
		}
	}

	void MainCameraPass::appendModelVertices(const MainCameraDrawPacket& packet, std::vector<MainCameraVertex>& out_vertices) {
		auto cache_it = model_vertex_cache_.find(packet.model_id);
		if (cache_it == model_vertex_cache_.end()) {
			auto model = ResourceManager::self().get_model(packet.model_id);
			if (!model.data) {
				return;
			}

			std::vector<MainCameraVertex> cached_vertices{};
			auto fallback_texture = TextureManager::self().loadFallbackTexture();
			ui32 fallback_texture_index = 0;
			if (fallback_texture && fallback_texture->descriptor_index >= 0) {
				fallback_texture_index = static_cast<ui32>(fallback_texture->descriptor_index);
			}
			for (const identifier mesh_id : model.data->meshes) {
				auto mesh = ResourceManager::self().get_mesh(mesh_id);
				if (!mesh.data) {
					continue;
				}

				ui32 mesh_texture_index = fallback_texture_index;
				if (!mesh.data->textures.empty()) {
					auto texture = TextureManager::self().loadTexture(mesh.data->textures.front());
					if (texture && texture->descriptor_index >= 0) {
						mesh_texture_index = static_cast<ui32>(texture->descriptor_index);
					}
				}

				for (const auto vertex_index : mesh.data->indices) {
					if (vertex_index >= mesh.data->vertices.size()) {
						continue;
					}

					const auto& source_vertex = mesh.data->vertices[vertex_index];
					cached_vertices.push_back({source_vertex.position, source_vertex.normal, source_vertex.tex_coords, mesh_texture_index});
				}
			}

			cache_it = model_vertex_cache_.emplace(packet.model_id, std::move(cached_vertices)).first;
		}

		const auto& model_vertices = cache_it->second;
		if (model_vertices.empty()) {
			return;
		}

		const auto normal_matrix = glm::inverseTranspose(glm::mat3(packet.model_matrix));
		const auto tint = packet.color;
		for (const auto& v : model_vertices) {
			const Vector4f world_position = packet.model_matrix * Vector4f(v.position, 1.0f);
			Vector3f world_normal = normal_matrix * v.normal;
			if (glm::length(world_normal) > std::numeric_limits<float>::epsilon()) {
				world_normal = glm::normalize(world_normal);
			}

			const float color_scale = (std::max)(0.0f, (std::min)(1.0f, (tint.x + tint.y + tint.z) / 3.0f));
			out_vertices.push_back({Vector3f(world_position), world_normal * color_scale, v.uv, v.texture_index});
		}
	}

} // dodoe
