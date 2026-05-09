// do@Redlive

#include "point_light_shadow_pass.h"

#include "../render_resource.h"
#include "../framework/scene_graph.h"

#include "runtime/core/utils/common.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {
	namespace {
		constexpr ui32 kVolatileConstantBufferVersions = 128;
	}

	PointLightShadowPass::PointLightShadowPass(RhiContext* rhi) {
		m_rhi = rhi;
	}

	void PointLightShadowPass::setup() {
		createBuffers();
		createShaders();
		createInputLayout();
		createBindingLayout();

		m_cmd_list = m_rhi->getDevice()->createCommandList();
	}

	void PointLightShadowPass::execute(size_t index) {
		(void)index;

		auto* render_scene = g_RenderResource ? &g_RenderResource->getRenderScene() : nullptr;
		auto scene_graph = render_scene ? render_scene->getSceneGraph() : nullptr;
		if (!scene_graph) {
			return;
		}

		std::vector<Ref<PointLight>> point_lights;
		std::vector<Vector3f> point_light_positions;
		point_lights.reserve(kMaxPointLightCount);
		point_light_positions.reserve(kMaxPointLightCount);
		for (const auto& light_leaf : scene_graph->getLights()) {
			if (const auto point_light = std::dynamic_pointer_cast<PointLight>(light_leaf)) {
				if (!point_light->getNode()) {
					continue;
				}
				point_lights.push_back(point_light);
				point_light_positions.push_back(point_light->getNodePtr()->getTranslation());
				if (point_lights.size() >= kMaxPointLightCount) {
					break;
				}
			}
		}

		const ui32 point_light_count = static_cast<ui32>(point_lights.size());
		if (point_light_count == 0) {
			return;
		}

		const ui32 layer_count = point_light_count * 2;
		if (!m_shadow_target || m_active_layer_count != layer_count) {
			createShadowTarget(layer_count);
			createFramebuffer();
			m_graphics_pipeline = nullptr;
		}

		if (!m_graphics_pipeline) {
			createGraphicsPipeline();
		}

		if (!m_constant_buffer || !m_shadow_target || !m_framebuffer || !m_graphics_pipeline) {
			return;
		}

		m_cmd_list->open();
		m_cmd_list->beginMarker("PointLightShadowPass");
		m_cmd_list->setTextureState(m_shadow_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
		m_cmd_list->commitBarriers();
		m_cmd_list->clearDepthStencilTexture(m_shadow_target, rhi::AllSubresources, true, 1.0f, false, 0);

		render_scene->prepareBuffers(m_cmd_list);

		PointLightShadowPassConstants constants{};
		constants.point_light_count = point_light_count;
		for (ui32 i = 0; i < point_light_count; ++i) {
			const auto& point_light = point_lights[i];
			const auto& translation = point_light_positions[i];
			constants.point_lights_position_and_radius[i] = Vector4f(translation.x, translation.y, translation.z, point_light->radius);
		}
		m_cmd_list->writeBuffer(m_constant_buffer, &constants, sizeof(constants));
		if (!m_binding_set) {
			createBindingSet();
		}

		auto viewport_state = rhi::ViewportState().addViewportAndScissorRect(
			rhi::Viewport(static_cast<float>(kShadowMapSize), static_cast<float>(kShadowMapSize))
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

	void PointLightShadowPass::cleanup() {
	}

	void PointLightShadowPass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		if (m_shadow_target) {
			createFramebuffer();
		}
		m_graphics_pipeline = nullptr;
	}

	void PointLightShadowPass::createBuffers() {
		auto buffer_desc = rhi::BufferDesc()
			.setByteSize(sizeof(PointLightShadowPassConstants))
			.setIsConstantBuffer(true)
			.setIsVolatile(true)
			.setMaxVersions(kVolatileConstantBufferVersions)
			.setDebugName("PointLightShadowPass Constants");
		m_constant_buffer = m_rhi->getDevice()->createBuffer(buffer_desc);
	}

	void PointLightShadowPass::createShaders() {
		auto vert_source = dodoe::ReadShaderFile("engine/res/shaders/bin/point_light_shadow_pass.vert.spv");
		auto geom_source = dodoe::ReadShaderFile("engine/res/shaders/bin/point_light_shadow_pass.geom.spv");
		auto frag_source = dodoe::ReadShaderFile("engine/res/shaders/bin/point_light_shadow_pass.frag.spv");
		m_vertex_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("PointLightShadowPass VS"),
			vert_source.data(), vert_source.size());
		m_geometry_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Geometry).setEntryName("main").setDebugName("PointLightShadowPass GS"),
			geom_source.data(), geom_source.size());
		m_pixel_shader = m_rhi->getDevice()->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("PointLightShadowPass PS"),
			frag_source.data(), frag_source.size());
	}

	void PointLightShadowPass::createInputLayout() {
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

	void PointLightShadowPass::createBindingLayout() {
		auto layout_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0));
		m_binding_layout = m_rhi->getDevice()->createBindingLayout(layout_desc);
	}

	void PointLightShadowPass::createBindingSet() {
		if (!m_constant_buffer) {
			return;
		}
		auto binding_set_desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::ConstantBuffer(0, m_constant_buffer));
		m_binding_set = m_rhi->getDevice()->createBindingSet(binding_set_desc, m_binding_layout);
	}

	void PointLightShadowPass::createShadowTarget(ui32 layer_count) {
		m_shadow_target = nullptr;
		m_active_layer_count = layer_count;
		if (layer_count == 0) {
			return;
		}

		auto texture_desc = rhi::TextureDesc()
			.setDimension(rhi::TextureDimension::Texture2DArray)
			.setWidth(kShadowMapSize)
			.setHeight(kShadowMapSize)
			.setArraySize(layer_count)
			.setFormat(rhi::Format::D32)
			.setIsRenderTarget(true)
			.enableAutomaticStateTracking(rhi::ResourceStates::DepthWrite)
			.setDebugName("PointLightShadowPass Depth Target");
		m_shadow_target = m_rhi->getDevice()->createTexture(texture_desc);
	}

	void PointLightShadowPass::createFramebuffer() {
		m_framebuffer = nullptr;
		if (!m_shadow_target) {
			return;
		}

		auto framebuffer_desc = rhi::FramebufferDesc().setDepthAttachment(m_shadow_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	void PointLightShadowPass::createGraphicsPipeline() {
		if (!m_framebuffer || !m_vertex_shader || !m_geometry_shader || !m_pixel_shader || !m_binding_layout || !m_input_layout) {
			return;
		}

		auto pipeline_desc = rhi::GraphicsPipelineDesc()
			.setInputLayout(m_input_layout)
			.setVertexShader(m_vertex_shader)
			.setGeometryShader(m_geometry_shader)
			.setPixelShader(m_pixel_shader)
			.addBindingLayout(m_binding_layout)
			.setPrimType(rhi::PrimitiveType::TriangleList);

		rhi::DepthStencilState depth_stencil_state;
		depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(rhi::ComparisonFunc::Less).disableStencil();
		rhi::RasterState raster_state;
		raster_state
			.setCullBack()
			.setDepthBiasClamp(0.0f)
			.setDepthBias(8)
			.setSlopeScaleDepthBias(2.0f);
		rhi::RenderState render_state;
		render_state.setDepthStencilState(depth_stencil_state);
		render_state.setRasterState(raster_state);
		pipeline_desc.setRenderState(render_state);

		m_graphics_pipeline = m_rhi->getDevice()->createGraphicsPipeline(pipeline_desc, m_framebuffer->getFramebufferInfo());
	}

} // dodoe
