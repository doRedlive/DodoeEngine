// do@Redlive

#include "directional_light_shadow_pass.h"

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

	DirectionalLightShadowPass::DirectionalLightShadowPass(RhiContext* rhi) {
		m_rhi = rhi;
	}

	void DirectionalLightShadowPass::setup() {
		createFramebuffer();
		createShaders();
		createBuffers();
		createInputLayout();
		createBindingLayout();
		createBindingSet();
		createGraphicsPipeline();

		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void DirectionalLightShadowPass::execute(size_t index) {
		(void)index;
		if (!m_graphics_pipeline) {
			createGraphicsPipeline();
		}

		if (!m_constant_buffer || !m_shadow_target || !m_framebuffer || !m_binding_set) {
			return;
		}

		auto* render_scene = g_RenderResource ? &g_RenderResource->getRenderScene() : nullptr;
		auto scene_graph = render_scene ? render_scene->getSceneGraph() : nullptr;
		if (!scene_graph) {
			return;
		}

		Ref<DirectionalLight> directional_light;
		Ref<SceneGraphNode> directional_light_node;
		for (const auto& light_leaf : scene_graph->getLights()) {
			directional_light = std::dynamic_pointer_cast<DirectionalLight>(light_leaf);
			if (directional_light && directional_light->getNode()) {
				directional_light_node = directional_light->getNodePtr();
				break;
			}
		}
		if (!directional_light || !directional_light_node) {
			return;
		}

		const Vector3f light_dir = BuildDirectionalLightDirection(directional_light_node);
		const Matrix4f light_view_projection = BuildDirectionalLightViewProjection(light_dir);

		m_cmd_list->open();
		m_cmd_list->beginMarker("DirectionalLightShadowPass");
		m_cmd_list->setTextureState(m_shadow_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
		m_cmd_list->commitBarriers();
		m_cmd_list->clearDepthStencilTexture(m_shadow_target, rhi::AllSubresources, true, 1.0f, false, 0);

		render_scene->prepareBuffers(m_cmd_list);

		DirectionalLightShadowPassConstants constants{};
		constants.light_view_projection = light_view_projection;
		m_cmd_list->writeBuffer(m_constant_buffer, &constants, sizeof(constants));

		auto viewport_state = rhi::ViewportState().addViewportAndScissorRect(
			rhi::Viewport(static_cast<float>(graph().getViewportExtent().x), static_cast<float>(graph().getViewportExtent().y))
		);

		m_cmd_list->setGraphicsState(
			rhi::GraphicsState()
				.setPipeline(m_graphics_pipeline)
				.setFramebuffer(m_framebuffer)
				.setViewport(viewport_state)
				.addBindingSet(m_binding_set)
		);

		const auto& instances = render_scene->mainCameraInstances();
		for (size_t begin = 0; begin < instances.size();) {
			const auto& first_instance = instances[begin];
			if (!first_instance || !first_instance->getMesh()) {
				++begin;
				continue;
			}

			const auto& mesh = first_instance->getMesh();
			size_t end = begin + 1;
			while (end < instances.size()) {
				const auto& instance = instances[end];
				if (!instance || instance->getMesh() != mesh) {
					break;
				}
				++end;
			}

			if (!mesh || !mesh->buffers || !mesh->buffers->vertex_buffer || !mesh->buffers->index_buffer || !mesh->buffers->instance_buffer) {
				begin = end;
				continue;
			}

			for (const auto& geometry : mesh->geometries) {
				if (!geometry || geometry->index_count == 0) {
					continue;
				}

				m_cmd_list->setGraphicsState(
					rhi::GraphicsState()
						.setPipeline(m_graphics_pipeline)
						.setFramebuffer(m_framebuffer)
						.setViewport(viewport_state)
						.addBindingSet(m_binding_set)
						.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(mesh->buffers->vertex_buffer).setSlot(0).setOffset(0))
						.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(mesh->buffers->instance_buffer).setSlot(1).setOffset(0))
						.setIndexBuffer(rhi::IndexBufferBinding().setBuffer(mesh->buffers->index_buffer).setFormat(rhi::Format::R32_UINT).setOffset(0))
				);

				auto draw_args = rhi::DrawArguments()
					.setVertexCount(geometry->index_count)
					.setInstanceCount(static_cast<ui32>(end - begin))
					.setStartIndexLocation(geometry->index_offset)
					.setStartVertexLocation(geometry->vertex_offset);
				m_cmd_list->drawIndexed(draw_args);
			}

			begin = end;
		}

		m_cmd_list->setTextureState(m_shadow_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();
		m_cmd_list->endMarker();
		m_cmd_list->close();
		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void DirectionalLightShadowPass::cleanup() {
	}

	void DirectionalLightShadowPass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		createFramebuffer();
		m_binding_set = nullptr;
		createBindingSet();
		m_graphics_pipeline = nullptr;
	}

	void DirectionalLightShadowPass::createBuffers() {
		auto buffer_desc = rhi::BufferDesc()
			.setByteSize(sizeof(DirectionalLightShadowPassConstants))
			.setIsConstantBuffer(true)
			.setIsVolatile(true)
			.setMaxVersions(kVolatileConstantBufferVersions)
			.setDebugName("DirectionalLightShadowPass Constants");
		m_constant_buffer = m_rhi->getDevice()->createBuffer(buffer_desc);
	}

	void DirectionalLightShadowPass::createShaders() {
		auto vert_source = dodoe::ReadShaderFile("engine/res/shaders/bin/directional_light_shadow_pass.vert.spv");
		auto frag_source = dodoe::ReadShaderFile("engine/res/shaders/bin/directional_light_shadow_pass.frag.spv");
		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("DirectionalLightShadowPass VS"),
			vert_source.data(), vert_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("DirectionalLightShadowPass PS"),
			frag_source.data(), frag_source.size());
	}

	void DirectionalLightShadowPass::createInputLayout() {
		rhi::VertexAttributeDesc attributes[] = {
			rhi::VertexAttributeDesc()
				.setName("a_Position")
				.setFormat(rhi::Format::RGB32_FLOAT)
				.setOffset(0)
				.setElementStride(sizeof(Vector3f) + sizeof(ui32) + sizeof(Vector2f)),
			rhi::VertexAttributeDesc()
				.setName("a_Normal")
				.setFormat(rhi::Format::RGBA8_SNORM)
				.setOffset(sizeof(Vector3f))
				.setElementStride(sizeof(Vector3f) + sizeof(ui32) + sizeof(Vector2f)),
			rhi::VertexAttributeDesc()
				.setName("a_UV")
				.setFormat(rhi::Format::RG32_FLOAT)
				.setOffset(sizeof(Vector3f) + sizeof(ui32))
				.setElementStride(sizeof(Vector3f) + sizeof(ui32) + sizeof(Vector2f)),
			rhi::VertexAttributeDesc()
				.setName("a_Model0")
				.setFormat(rhi::Format::RGBA32_FLOAT)
				.setBufferIndex(1)
				.setOffset(0)
				.setElementStride(sizeof(Matrix4f))
				.setIsInstanced(true),
			rhi::VertexAttributeDesc()
				.setName("a_Model1")
				.setFormat(rhi::Format::RGBA32_FLOAT)
				.setBufferIndex(1)
				.setOffset(sizeof(Vector4f))
				.setElementStride(sizeof(Matrix4f))
				.setIsInstanced(true),
			rhi::VertexAttributeDesc()
				.setName("a_Model2")
				.setFormat(rhi::Format::RGBA32_FLOAT)
				.setBufferIndex(1)
				.setOffset(sizeof(Vector4f) * 2)
				.setElementStride(sizeof(Matrix4f))
				.setIsInstanced(true),
			rhi::VertexAttributeDesc()
				.setName("a_Model3")
				.setFormat(rhi::Format::RGBA32_FLOAT)
				.setBufferIndex(1)
				.setOffset(sizeof(Vector4f) * 3)
				.setElementStride(sizeof(Matrix4f))
				.setIsInstanced(true),
		};

		m_input_layout = m_rhi->getDevice()->createInputLayout(
			attributes,
			static_cast<ui32>(std::size(attributes)),
			m_vertex_shader
		);
	}

	void DirectionalLightShadowPass::createBindingLayout() {
		auto layout_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(layout_desc);
	}

	void DirectionalLightShadowPass::createBindingSet() {
		if (!m_constant_buffer) {
			return;
		}
		auto binding_set_desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::ConstantBuffer(0, m_constant_buffer));
		m_binding_set = m_rhi->getDevice()->createBindingSet(binding_set_desc, m_binding_layout);
	}

	void DirectionalLightShadowPass::createFramebuffer() {
		m_shadow_target = getTextureResource(kSceneShadowMapName);
		m_framebuffer = nullptr;
		if (!m_shadow_target) {
			return;
		}

		auto framebuffer_desc = rhi::FramebufferDesc().setDepthAttachment(m_shadow_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void DirectionalLightShadowPass::createGraphicsPipeline() {
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
		depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(rhi::ComparisonFunc::Less).disableStencil();
		rhi::RasterState raster_state;
		raster_state
			.setCullBack()
			.setDepthBiasClamp(0.0f)
			.setDepthBias(6)
			.setSlopeScaleDepthBias(1.5f);
		rhi::RenderState render_state;
		render_state.setDepthStencilState(depth_stencil_state);
		render_state.setRasterState(raster_state);
		pipeline_desc.setRenderState(render_state);

		m_graphics_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, framebuffer_info);
	}

} // dodoe
