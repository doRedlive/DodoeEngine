// Created by Redlive on 2026/4/6.

#include "main_camera_pass.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/resource_manager.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

namespace dodoe {

	namespace {

		constexpr uint32_t kSpirvMagic = 0x07230203u;

		bool IsSpirvBinary(const std::vector<char>& bytes) {
			if (bytes.size() < sizeof(uint32_t) || (bytes.size() % sizeof(uint32_t)) != 0) {
				return false;
			}
			uint32_t magic = 0;
			std::memcpy(&magic, bytes.data(), sizeof(uint32_t));
			return magic == kSpirvMagic;
		}

		std::vector<char> ReadShaderFile(const std::string& path) {
			std::ifstream in(path, std::ios::binary | std::ios::ate);
			if (!in.is_open()) {
				DoError("MainCameraPass: open shader file {} failed.", path);
				return {};
			}

			const std::streamsize size = in.tellg();
			in.seekg(0, std::ios::beg);

			std::vector<char> content(static_cast<size_t>(size));
			in.read(content.data(), size);
			return content;
		}

	}

	MainCameraPass::MainCameraPass(const RenderPassCreateInfo& info, const std::vector<rhi::TextureHandle>& swapchain_targets, const Vector2i& target_extent)
		: RenderPass(info), swapchain_targets_(swapchain_targets), target_extent_(target_extent) {
		setName("MainCameraPass");
	}

	void MainCameraPass::setup() {
		if (initialized_) {
			return;
		}

		if (!device_ || swapchain_targets_.empty()) {
			return;
		}

		createFramebuffers();
		createGraphicsPipeline();
		createBuffers();
		fillResources();
		loadMeshFromResourceManager();

		cmd_list_ = device_->createCommandList();
		initialized_ = true;
	}

	void MainCameraPass::cleanup() {
	}

	void MainCameraPass::execute() {
		if (!initialized_ || !cmd_list_ || !graphics_pipeline_ || framebuffers_.empty() || !vertex_buffer_ || !binding_set_ || !constant_buffer_) {
			return;
		}

		if (draw_vertices_.empty()) {
			return;
		}

		const size_t framebuffer_index = static_cast<size_t>(1 % framebuffers_.size());
		auto framebuffer = framebuffers_[framebuffer_index];
		if (!framebuffer) {
			return;
		}

		cmd_list_->open();

		if (target_extent_.x <= 0 || target_extent_.y <= 0) {
			cmd_list_->close();
			return;
		}

		const float aspect = static_cast<float>(target_extent_.x) / static_cast<float>(target_extent_.y);
		const Matrix4f projection = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
		const Matrix4f view = glm::lookAt(Vector3f(0.0f, 1.0f, 3.5f), Vector3f(0.0f, 0.5f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));
		const Matrix4f model = glm::mat4(1.0f);
		const Matrix4f mvp = projection * view * model;
		cmd_list_->writeBuffer(constant_buffer_, &mvp, sizeof(Matrix4f));
		cmd_list_->writeBuffer(vertex_buffer_, draw_vertices_.data(), static_cast<ui32>(draw_vertices_.size() * sizeof(MainCameraVertex)));

		auto graphics_state = rhi::GraphicsState()
			.setPipeline(graphics_pipeline_)
			.setFramebuffer(framebuffer)
			.setViewport(rhi::ViewportState().addViewportAndScissorRect(
				rhi::Viewport(static_cast<float>(target_extent_.x), static_cast<float>(target_extent_.y))))
			.addBindingSet(binding_set_)
			.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(vertex_buffer_).setSlot(0).setOffset(0));
		cmd_list_->setGraphicsState(graphics_state);

		auto draw_args = rhi::DrawArguments()
			.setVertexCount(static_cast<ui32>(draw_vertices_.size()));
		cmd_list_->draw(draw_args);

		cmd_list_->close();
		device_->executeCommandList(cmd_list_);
	}

	void MainCameraPass::createFramebuffers() {
		framebuffers_.clear();
		framebuffers_.reserve(swapchain_targets_.size());

		for (const auto& target : swapchain_targets_) {
			auto framebuffer_desc = rhi::FramebufferDesc().addColorAttachment(target);
			auto framebuffer = device_->createFramebuffer(framebuffer_desc);
			if (framebuffer) {
				framebuffers_.push_back(framebuffer);
			}
		}
	}

	void MainCameraPass::createGraphicsPipeline() {
		auto vert_source = ReadShaderFile("engine/res/shaders/main_camera_pass.vert.spv");
		auto frag_source = ReadShaderFile("engine/res/shaders/main_camera_pass.frag.spv");
		if (vert_source.empty() || frag_source.empty()) {
			DoError("MainCameraPass: missing SPIR-V shader files (expected main_camera_pass.vert.spv / main_camera_pass.frag.spv).");
			return;
		}

		if (!IsSpirvBinary(vert_source) || !IsSpirvBinary(frag_source)) {
			DoError("MainCameraPass: shader files are not valid SPIR-V binaries.");
			return;
		}

		auto vertex_shader = device_->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex).setEntryName("main").setDebugName("MainCameraPass VS"),
			vert_source.data(), vert_source.size());
		auto pixel_shader = device_->createShader(
			rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel).setEntryName("main").setDebugName("MainCameraPass PS"),
			frag_source.data(), frag_source.size());
		if (!vertex_shader || !pixel_shader) {
			DoError("MainCameraPass: createShader failed.");
			return;
		}

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
		};

		auto input_layout = device_->createInputLayout(
			attributes,
			static_cast<ui32>(std::size(attributes)),
			vertex_shader);
		if (!input_layout) {
			DoError("MainCameraPass: createInputLayout failed.");
			return;
		}

		if (framebuffers_.empty()) {
			return;
		}

		auto framebuffer_info = framebuffers_.front()->getFramebufferInfo();

		auto layout_desc = rhi::BindingLayoutDesc()
			.setVisibility(rhi::ShaderType::All)
			.addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0));
		binding_layout_ = device_->createBindingLayout(layout_desc);
		if (!binding_layout_) {
			DoError("MainCameraPass: createBindingLayout failed.");
			return;
		}

		auto pipeline_desc = rhi::GraphicsPipelineDesc()
			.setInputLayout(input_layout)
			.setVertexShader(vertex_shader)
			.setPixelShader(pixel_shader)
			.addBindingLayout(binding_layout_);

		rhi::DepthStencilState depth_stencil_state;
		depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();
		rhi::RenderState render_state;
		render_state.setDepthStencilState(depth_stencil_state);
		pipeline_desc.setRenderState(render_state);

		graphics_pipeline_ = device_->createGraphicsPipeline(pipeline_desc, framebuffer_info);
		if (!graphics_pipeline_) {
			DoError("MainCameraPass: createGraphicsPipeline failed.");
		}
	}

	void MainCameraPass::createBuffers() {
		auto constant_buffer_desc = rhi::BufferDesc()
			.setByteSize(sizeof(Matrix4f))
			.setIsConstantBuffer(true)
			.setIsVolatile(true)
			.setMaxVersions(16);
		constant_buffer_ = device_->createBuffer(constant_buffer_desc);

		auto vertex_buffer_desc = rhi::BufferDesc()
			.setByteSize(sizeof(MainCameraVertex) * 1024 * 1024)
			.setIsVertexBuffer(true)
			.enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
			.setDebugName("MainCameraPass Vertex Buffer");
		vertex_buffer_ = device_->createBuffer(vertex_buffer_desc);
	}

	void MainCameraPass::fillResources() {
		if (!binding_layout_ || !constant_buffer_) {
			return;
		}

		auto binding_set_desc = rhi::BindingSetDesc()
			.addItem(rhi::BindingSetItem::ConstantBuffer(0, constant_buffer_));
		binding_set_ = device_->createBindingSet(binding_set_desc, binding_layout_);
	}

	void MainCameraPass::loadMeshFromResourceManager() {
		if (mesh_loaded_) {
			return;
		}

		const std::filesystem::path model_path = std::filesystem::path(DODOE_ROOT) / "engine" / "res" / "models" / "marry" / "Marry.obj";
		const std::string model_name = "main_camera_model";

		auto model = ResourceManager::self().get_model(model_name, model_path.string());
		if (!model.data) {
			DoError("MainCameraPass: failed to load model from {}", model_path.string());
			return;
		}

		model_id_ = model.id;
		draw_vertices_.clear();

		for (const identifier mesh_id : model.data->meshes) {
			auto mesh = ResourceManager::self().get_mesh(mesh_id);
			if (!mesh.data) {
				continue;
			}

			for (const auto index : mesh.data->indices) {
				if (index >= mesh.data->vertices.size()) {
					continue;
				}

				const auto& source_vertex = mesh.data->vertices[index];
				draw_vertices_.push_back({source_vertex.position, source_vertex.normal});
			}
		}

		mesh_loaded_ = !draw_vertices_.empty();
		if (!mesh_loaded_) {
			DoError("MainCameraPass: model loaded but no drawable vertices.");
		}
	}

} // dodoe