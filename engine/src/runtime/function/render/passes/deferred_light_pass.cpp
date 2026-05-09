// do@Redlive

#include "deferred_light_pass.h"

#include "../render_graph.h"
#include "../render_resource.h"
#include "../framework/scene_graph.h"

#include "runtime/core/utils/common.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {
	namespace {
		constexpr ui32 kVolatileConstantBufferVersions = 128;

		Vector3f BuildDirectionalLightDirection(const Ref<SceneGraphNode>& light_node) {
			constexpr Vector3f kFallbackDirection = Vector3f(0.3f, 0.8f, 0.5f);
			if (!light_node) {
				return glm::normalize(kFallbackDirection);
			}

			const auto light_matrix = light_node->getGlobalMatrix();
			Vector3f direction = Vector3f(light_matrix * Vector4f(0.0f, 0.0f, -1.0f, 0.0f));
			if (glm::length(direction) < 0.0001f) {
				return glm::normalize(kFallbackDirection);
			}
			return glm::normalize(direction);
		}

		Matrix4f BuildDirectionalLightViewProjection(const Vector3f& light_dir) {
			const Vector3f light_eye = Vector3f(0.0f, 0.0f, 0.0f) - light_dir * 30.0f;
			const Matrix4f view = glm::lookAt(light_eye, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));
			const Matrix4f proj = glm::orthoRH_ZO(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 80.0f);
			return proj * view;
		}
	}

	DeferredLightPass::DeferredLightPass(RhiContext* rhi) {
		m_rhi = rhi;
	}

	void DeferredLightPass::setup() {
		createFramebuffer();
		createShaders();
		createBuffers();
		createSampler();
		createBindingLayout();
		createGraphicsPipeline();

		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void DeferredLightPass::execute(size_t index) {
		(void)index;
		if (!m_graphics_pipeline) {
			createGraphicsPipeline();
		}

		if (!m_constant_buffer) {
			createBuffers();
		}

		if (!m_constant_buffer || !m_render_target || !m_albedo_target || !m_normal_target || !m_position_target || !m_material_target || !m_shadow_target || !m_skybox_texture || !m_framebuffer) {
			return;
		}

		auto& render_scene = g_RenderResource->getRenderScene();
		auto scene_graph = render_scene.getSceneGraph();
		if (!scene_graph) {
			return;
		}
		const auto camera = render_scene.mainCamera();
		const Vector3f camera_position = (camera && camera->isValid()) ? camera->getPosition() : Vector3f(0.0f);

		std::vector<Ref<Light>> lights = scene_graph->getLights();

		m_cmd_list->open();
		m_cmd_list->beginMarker("DeferredLightPass");
		m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->setTextureState(m_albedo_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->setTextureState(m_normal_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->setTextureState(m_position_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->setTextureState(m_material_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->setTextureState(m_shadow_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		if (m_skybox_texture) {
			m_cmd_list->setTextureState(m_skybox_texture, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		}
		m_cmd_list->commitBarriers();

		if (lights.empty()) {
			m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
			m_cmd_list->commitBarriers();
			m_cmd_list->endMarker();
			m_cmd_list->close();
			m_rhi->getDevice()->executeCommandList(m_cmd_list);
			return;
		}

		auto viewport_state = rhi::ViewportState().addViewportAndScissorRect(
			rhi::Viewport(static_cast<float>(graph().getViewportExtent().x), static_cast<float>(graph().getViewportExtent().y))
		);

		bool directional_light_emitted = false;
		for (const auto& light_leaf : lights) {
			if (const auto directional_light = std::dynamic_pointer_cast<DirectionalLight>(light_leaf)) {
				if (directional_light_emitted || !directional_light->getNode()) {
					continue;
				}

				const auto light_node = directional_light->getNodePtr();
				const Vector3f light_dir = BuildDirectionalLightDirection(light_node);

				DeferredLightPassConstants constants{};
				constants.light_color_intensity = Vector4f(directional_light->color.r, directional_light->color.g, directional_light->color.b, directional_light->irradiance);
				constants.light_position_radius = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
				constants.light_direction_type = Vector4f(light_dir.x, light_dir.y, light_dir.z, 0.0f);
				constants.light_view_projection = BuildDirectionalLightViewProjection(light_dir);
				constants.shadow_params = Vector4f(0.005f, 0.2f, 0.005f, 2.0f);
				constants.camera_position = Vector4f(camera_position, 0.0f);
				m_cmd_list->writeBuffer(m_constant_buffer, &constants, sizeof(constants));
				if (!m_binding_set) { createBindingSet(); }

				m_cmd_list->setGraphicsState(
					rhi::GraphicsState()
						.setPipeline(m_graphics_pipeline)
						.setFramebuffer(m_framebuffer)
						.setViewport(viewport_state)
						.addBindingSet(m_binding_set)
				);

				auto draw_args = rhi::DrawArguments().setVertexCount(6).setInstanceCount(1);
				m_cmd_list->draw(draw_args);
				directional_light_emitted = true;
				continue;
			}

			const auto point_light = std::dynamic_pointer_cast<PointLight>(light_leaf);
			if (!point_light || !point_light->getNode()) {
				continue;
			}

			const auto light_node = point_light->getNodePtr();
			const auto translation = light_node ? light_node->getTranslation() : Vector3f(0.0f);

			DeferredLightPassConstants constants{};
			constants.light_color_intensity = Vector4f(point_light->color.r, point_light->color.g, point_light->color.b, point_light->intensity);
			constants.light_position_radius = Vector4f(translation.x, translation.y, translation.z, point_light->radius);
			constants.light_direction_type = Vector4f(0.0f, 0.0f, 0.0f, point_light->range);
			constants.light_view_projection = Matrix4f(1.0f);
			constants.shadow_params = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
			constants.camera_position = Vector4f(camera_position, 0.0f);
			m_cmd_list->writeBuffer(m_constant_buffer, &constants, sizeof(constants));
			if (!m_binding_set) { createBindingSet(); }

			m_cmd_list->setGraphicsState(
				rhi::GraphicsState()
					.setPipeline(m_graphics_pipeline)
					.setFramebuffer(m_framebuffer)
					.setViewport(viewport_state)
					.addBindingSet(m_binding_set)
			);

			auto draw_args = rhi::DrawArguments().setVertexCount(6).setInstanceCount(1);
			m_cmd_list->draw(draw_args);
		}

		m_cmd_list->setTextureState(m_render_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();
		m_cmd_list->endMarker();
		m_cmd_list->close();

		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void DeferredLightPass::cleanup() {
	}

	void DeferredLightPass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		createFramebuffer();
		m_binding_set = nullptr;
		m_graphics_pipeline = nullptr;
	}

	void DeferredLightPass::createShaders() {
		auto vert_source = dodoe::ReadShaderFile("engine/res/shaders/bin/fullscreen.vert.spv");
		auto frag_source = dodoe::ReadShaderFile("engine/res/shaders/bin/deferred_light_pass.frag.spv");
		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("DeferredLightPass VS"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("DeferredLightPass PS"),
			frag_source.data(), frag_source.size());
	}

	void DeferredLightPass::createBuffers() {
		auto buffer_desc = rhi::BufferDesc()
			.setByteSize(sizeof(DeferredLightPassConstants))
			.setIsConstantBuffer(true)
			.setIsVolatile(true)
			.setMaxVersions(kVolatileConstantBufferVersions)
			.setDebugName("DeferredLightPass Constants");
		m_constant_buffer = m_rhi->getDevice()->createBuffer(buffer_desc);
	}

	void DeferredLightPass::createSampler() {
		m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
	}

	void DeferredLightPass::createBindingLayout() {
		auto layout_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::Pixel)
			.addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0))
			.addItem(rhi::BindingLayoutItem::Sampler(0))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(0))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(1))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(2))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(3))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(4))
			.addItem(rhi::BindingLayoutItem::Texture_SRV(5));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(layout_desc);
	}

	void DeferredLightPass::createBindingSet() {
		if (!m_constant_buffer || !m_albedo_target || !m_normal_target || !m_position_target || !m_material_target || !m_shadow_target || !m_skybox_texture) {
			return;
		}

		auto binding_set_desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::ConstantBuffer(0, m_constant_buffer))
			.addItem(rhi::BindingSetItem::Sampler(0, m_sampler))
			.addItem(rhi::BindingSetItem::Texture_SRV(0, m_albedo_target))
			.addItem(rhi::BindingSetItem::Texture_SRV(1, m_normal_target))
			.addItem(rhi::BindingSetItem::Texture_SRV(2, m_position_target))
			.addItem(rhi::BindingSetItem::Texture_SRV(3, m_shadow_target))
			.addItem(rhi::BindingSetItem::Texture_SRV(4, m_material_target))
			.addItem(rhi::BindingSetItem::Texture_SRV(5, m_skybox_texture));
		m_binding_set = m_rhi->getDevice()->createBindingSet(binding_set_desc, m_binding_layout);
	}

	void DeferredLightPass::createFramebuffer() {
		m_albedo_target = getTextureResource(kSceneAlbedoName);
		m_normal_target = getTextureResource(kSceneNormalName);
		m_position_target = getTextureResource(kScenePositionName);
		m_material_target = getTextureResource(kSceneMaterialName);
		m_shadow_target = getTextureResource(kSceneShadowMapName);
		m_skybox_texture = g_RenderResource->getSkyboxTexture();
		m_render_target = getTextureResource(kSceneColorName);
		if (!m_albedo_target || !m_normal_target || !m_position_target || !m_material_target || !m_shadow_target || !m_render_target) {
			return;
		}

		auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(m_render_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void DeferredLightPass::createGraphicsPipeline() {
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
		rhi::BlendState blend_state;
		blend_state.targets[0]
			.enableBlend()
			.setSrcBlend(rhi::BlendFactor::One)
			.setDestBlend(rhi::BlendFactor::One)
			.setBlendOp(rhi::BlendOp::Add)
			.setSrcBlendAlpha(rhi::BlendFactor::One)
			.setDestBlendAlpha(rhi::BlendFactor::One)
			.setBlendOpAlpha(rhi::BlendOp::Add);
		rhi::RasterState raster_state;
		raster_state.setCullNone();

		rhi::RenderState render_state;
		render_state.setBlendState(blend_state);
		render_state.setDepthStencilState(depth_stencil_state);
		render_state.setRasterState(raster_state);
		pipeline_desc.setRenderState(render_state);

		m_graphics_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, m_framebuffer->getFramebufferInfo());
	}

} // dodoe
