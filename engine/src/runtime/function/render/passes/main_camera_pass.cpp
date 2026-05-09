// Created by Redlive on 2026/4/6.

#include "main_camera_pass.h"

#include "../render_graph.h"

#include "runtime/core/utils/common.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {
    namespace {
        constexpr ui32 kVolatileConstantBufferVersions = 4096;
		constexpr size_t kGeometryVertexStride = sizeof(Vector3f) + sizeof(ui32) + sizeof(Vector2f);
		constexpr size_t kGeometryPositionOffset = 0;
		constexpr size_t kGeometryNormalOffset = sizeof(Vector3f);
		constexpr size_t kGeometryUvOffset = sizeof(Vector3f) + sizeof(ui32);

        struct MainCameraPassConstants {
            Matrix4f view_projection{1.0f};
            Vector4i draw_data{0};
            Vector4f material_data{0.0f, 1.0f, 1.0f, 0.0f}; // metallic, roughness, ao, reserved
        };

        TextureManager* GetTextureManager() {
            auto& app = Application::Self();
            auto* render_system = app.context().render_system.get();
            return render_system ? render_system->getTextureManager() : nullptr;
        }
    }

	MainCameraPass::MainCameraPass(RhiContext* rhi, DescriptorTableManager* descriptor_manager)
		: m_descriptor_table(descriptor_manager) {
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
	}

	void MainCameraPass::execute(size_t index) {
		(void)index;
		if (!m_graphics_pipeline) { createGraphicsPipeline(); }

		auto& scene = g_RenderResource->getRenderScene();
		const auto& camera = scene.mainCamera();
		const auto& instances = scene.mainCameraInstances();

		m_cmd_list->open();
		m_cmd_list->beginMarker("MainCameraPass");
		m_cmd_list->setTextureState(m_albedo_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->setTextureState(m_normal_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->setTextureState(m_position_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->setTextureState(m_material_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		m_cmd_list->setTextureState(m_depth_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
		m_cmd_list->commitBarriers();
		m_cmd_list->clearTextureFloat(m_albedo_target, rhi::AllSubresources, rhi::Color(0.08f, 0.09f, 0.11f, 1.0f));
		m_cmd_list->clearTextureFloat(m_normal_target, rhi::AllSubresources, rhi::Color(0.0f, 0.0f, 0.0f, 1.0f));
		m_cmd_list->clearTextureFloat(m_position_target, rhi::AllSubresources, rhi::Color(0.0f, 0.0f, 0.0f, 1.0f));
		m_cmd_list->clearTextureFloat(m_material_target, rhi::AllSubresources, rhi::Color(0.0f, 1.0f, 1.0f, 1.0f));
		m_cmd_list->clearDepthStencilTexture(m_depth_target, rhi::AllSubresources, true, 1.0f, false, 0);

		if (!camera || !camera->isValid() || instances.empty()) {
			m_cmd_list->setTextureState(m_albedo_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
			m_cmd_list->setTextureState(m_normal_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
			m_cmd_list->setTextureState(m_position_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
			m_cmd_list->setTextureState(m_material_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
			m_cmd_list->commitBarriers();
			m_cmd_list->endMarker();
			m_cmd_list->close();
			m_rhi->getDevice()->executeCommandList(m_cmd_list);
			return;
		}

		scene.prepareBuffers(m_cmd_list);

		MainCameraPassConstants constants{};
		constants.view_projection = camera->getViewProjectionMatrix();
		auto viewport_state = rhi::ViewportState().addViewportAndScissorRect(
			rhi::Viewport(static_cast<float>(graph().getViewportExtent().x), static_cast<float>(graph().getViewportExtent().y)));

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

				constants.draw_data.x = static_cast<int>(resolveTextureIndex(geometry));
				const auto mr_index = resolveMetallicRoughnessTextureIndex(geometry);
				constants.draw_data.y = static_cast<int>(mr_index);
				constants.draw_data.z = (geometry && geometry->material && geometry->material->metallic_roughness_texture != 0) ? 1 : 0;
				constants.material_data = Vector4f(0.0f, 1.0f, 1.0f, 0.0f);
				if (geometry && geometry->material) {
					constants.material_data.x = glm::clamp(geometry->material->metallic, 0.0f, 1.0f);
					constants.material_data.y = glm::clamp(geometry->material->roughness, 0.04f, 1.0f);
				}
				m_cmd_list->writeBuffer(m_constant_buffer, &constants, sizeof(MainCameraPassConstants));

				auto graphics_state = rhi::GraphicsState()
					.setPipeline(m_graphics_pipeline)
					.setFramebuffer(m_framebuffer)
					.setViewport(viewport_state)
					.addBindingSet(m_binding_set)
					.addBindingSet(m_descriptor_table->getDescriptorTable())
					.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(mesh->buffers->vertex_buffer).setSlot(0).setOffset(0))
					.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(mesh->buffers->instance_buffer).setSlot(1).setOffset(0))
					.setIndexBuffer(rhi::IndexBufferBinding().setBuffer(mesh->buffers->index_buffer).setFormat(rhi::Format::R32_UINT).setOffset(0));
				m_cmd_list->setGraphicsState(graphics_state);

				auto draw_args = rhi::DrawArguments()
					.setVertexCount(geometry->index_count)
					.setInstanceCount(static_cast<ui32>(end - begin))
					.setStartIndexLocation(geometry->index_offset)
					.setStartVertexLocation(geometry->vertex_offset);
				m_cmd_list->drawIndexed(draw_args);
			}

			begin = end;
		}

		m_cmd_list->setTextureState(m_albedo_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->setTextureState(m_normal_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->setTextureState(m_position_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->setTextureState(m_material_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
		m_cmd_list->commitBarriers();

		m_cmd_list->endMarker();
		m_cmd_list->close();
		m_rhi->getDevice()->executeCommandList(m_cmd_list);
	}

	void MainCameraPass::onViewportResize(const Vector2i& viewport_extent) {
		(void)viewport_extent;
		createFramebuffer();
		m_graphics_pipeline = nullptr;
	}

	void MainCameraPass::createShaders() {
		auto vert_source = dodoe::ReadShaderFile("engine/res/shaders/bin/main_camera_pass.vert.spv");
		auto frag_source = dodoe::ReadShaderFile("engine/res/shaders/bin/main_camera_pass.frag.spv");
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
			.setByteSize(sizeof(MainCameraPassConstants))
			.setIsConstantBuffer(true)
			.setIsVolatile(true)
			.setMaxVersions(kVolatileConstantBufferVersions)
			.setDebugName("MainCameraPass Camera Buffer");
		m_constant_buffer = m_rhi->getDevice()->createBuffer(m_constant_bufferdesc);
	}

	void MainCameraPass::createSampler() {
		m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());
	}

	void MainCameraPass::createBindingSet() {
		auto binding_set_desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::ConstantBuffer(0, m_constant_buffer))
			.addItem(rhi::BindingSetItem::Sampler(0, m_sampler));
		m_binding_set = m_rhi->getDevice()->createBindingSet(binding_set_desc, m_binding_layout);
	}
	
	void MainCameraPass::createInputLayout() {
		rhi::VertexAttributeDesc attributes[] = {
			rhi::VertexAttributeDesc()
				.setName("a_Position")
				.setFormat(rhi::Format::RGB32_FLOAT)
				.setOffset(kGeometryPositionOffset)
				.setElementStride(kGeometryVertexStride),
			rhi::VertexAttributeDesc()
				.setName("a_Normal")
				.setFormat(rhi::Format::RGBA8_SNORM)
				.setOffset(kGeometryNormalOffset)
				.setElementStride(kGeometryVertexStride),
			rhi::VertexAttributeDesc()
				.setName("a_UV")
				.setFormat(rhi::Format::RG32_FLOAT)
				.setOffset(kGeometryUvOffset)
				.setElementStride(kGeometryVertexStride),
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
		m_albedo_target = nullptr;
		m_normal_target = nullptr;
		m_position_target = nullptr;
		m_material_target = nullptr;
		m_depth_target = nullptr;
		m_framebuffer = nullptr;

		m_albedo_target = getTextureResource(kSceneAlbedoName);
		m_normal_target = getTextureResource(kSceneNormalName);
		m_position_target = getTextureResource(kScenePositionName);
		m_material_target = getTextureResource(kSceneMaterialName);
		m_depth_target = getTextureResource(kSceneDepthName);
		if (!m_albedo_target || !m_normal_target || !m_position_target || !m_material_target || !m_depth_target) {
			return;
		}

		auto framebuffer_desc = rhi::FramebufferDesc()
			.addColorAttachment(m_albedo_target)
			.addColorAttachment(m_normal_target)
			.addColorAttachment(m_position_target)
			.addColorAttachment(m_material_target)
			.setDepthAttachment(m_depth_target);
		m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
	}

	ui32 MainCameraPass::resolveTextureIndex(const Ref<MeshGeometry>& geometry) const {
		auto* texture_manager = GetTextureManager();

		auto fallback_texture = texture_manager->loadFallbackTexture();
		ui32 texture_index = 0;
		if (fallback_texture && fallback_texture->descriptor_index >= 0) {
			texture_index = static_cast<ui32>(fallback_texture->descriptor_index);
		}

		if (geometry && geometry->material && geometry->material->base_color_texture != 0) {
			auto texture = texture_manager->loadTexture(geometry->material->base_color_texture);
			if (texture && texture->descriptor_index >= 0) {
				texture_index = static_cast<ui32>(texture->descriptor_index);
			}
		}

		return texture_index;
	}

	ui32 MainCameraPass::resolveMetallicRoughnessTextureIndex(const Ref<MeshGeometry>& geometry) const {
		auto* texture_manager = GetTextureManager();
		if (!texture_manager || !geometry || !geometry->material || geometry->material->metallic_roughness_texture == 0) {
			return 0;
		}

		auto texture = texture_manager->loadTexture(geometry->material->metallic_roughness_texture);
		if (texture && texture->descriptor_index >= 0) {
			return static_cast<ui32>(texture->descriptor_index);
		}
		return 0;
	}
} // dodoe
