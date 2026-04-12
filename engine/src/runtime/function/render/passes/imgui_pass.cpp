// do: GreenMuffin

#include "imgui_pass.h"

#include "../render_api.h"
#include "../interface/rhi_backend.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_vulkan.h"

namespace dodoe {

	ImGuiPass::ImGuiPass(const RenderPassCreateInfo& info, RhiContext* rhi_backend, const std::vector<rhi::TextureHandle>& swapchain_targets, const Vector2i& target_extent)
		: RenderPass(info), rhi_backend_(rhi_backend), swapchain_targets_(swapchain_targets), target_extent_(target_extent) {
		setName("ImGuiPass");
	}

	void ImGuiPass::setup() {
		if (RenderApi::apiType() != RenderApiType::Vulkan) return;
		cmd_list_ = device_->createCommandList();

		auto* vulkan_backend = rhi_backend_->getVulkanBackend();
		DoAssert(vulkan_backend, "ImGuiPass::setup vulkan backend is null.");
		DoAssert(!vulkan_backend->getSwapchainImageViews().empty(), "ImGuiPass::setup swapchain image views are empty.");

		DoAssert(initializeVulkanBackend(), "ImGuiPass::initialize vulkan backend failed!");
	}

	void ImGuiPass::execute(size_t index) {
		current_framebuffer_index_ = index;
		ImGui::Render();


		ImDrawData* draw_data = ImGui::GetDrawData();
		if (!draw_data) return;
		auto* vulkan_backend = rhi_backend_->getVulkanBackend();
		const auto& swapchain_image_views = vulkan_backend->getSwapchainImageViews();

		rhi::TextureHandle target_texture = swapchain_targets_[current_framebuffer_index_ % swapchain_targets_.size()];

		cmd_list_->open();
		cmd_list_->setTextureState(target_texture, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
		cmd_list_->commitBarriers();

		auto vk_command_buffer = static_cast<VkCommandBuffer>(cmd_list_->getNativeObject(rhi::ObjectTypes::VK_CommandBuffer));
		auto vk_target_image_view = swapchain_image_views[current_framebuffer_index_ % swapchain_image_views.size()];
		if (!vk_command_buffer || !vk_target_image_view) {
			cmd_list_->close();
			device_->executeCommandList(cmd_list_);
			return;
		}

		VkRenderingAttachmentInfo color_attachment{};
		color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		color_attachment.imageView = vk_target_image_view;
		color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

		VkRenderingInfo rendering_info{};
		rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		rendering_info.renderArea.offset = {0, 0};
		rendering_info.renderArea.extent = {
			static_cast<uint32_t>(target_extent_.x),
			static_cast<uint32_t>(target_extent_.y)
		};
		rendering_info.layerCount = 1;
		rendering_info.colorAttachmentCount = 1;
		rendering_info.pColorAttachments = &color_attachment;

		vkCmdBeginRendering(vk_command_buffer, &rendering_info);
		if (draw_data->CmdListsCount > 0) {
			ImGui_ImplVulkan_RenderDrawData(draw_data, vk_command_buffer);
		}
		vkCmdEndRendering(vk_command_buffer);

		cmd_list_->setTextureState(target_texture, rhi::AllSubresources, rhi::ResourceStates::Present);
		cmd_list_->commitBarriers();

		cmd_list_->close();
		device_->executeCommandList(cmd_list_);
	}

	void ImGuiPass::cleanup() {
		if (RenderApi::apiType() != RenderApiType::Vulkan) return;

		ImGui_ImplVulkan_Shutdown();
		if (descriptor_pool_ != VK_NULL_HANDLE && rhi_backend_ && rhi_backend_->getVulkanBackend()) {
			vkDestroyDescriptorPool(rhi_backend_->getVulkanBackend()->getDevice(), descriptor_pool_, nullptr);
			descriptor_pool_ = VK_NULL_HANDLE;
		}
		cmd_list_ = nullptr;
	}

	bool ImGuiPass::initializeVulkanBackend() {
		if (!rhi_backend_ || !rhi_backend_->getVulkanBackend()) {
			return false;
		}

		VulkanBackend* vulkan_backend = rhi_backend_->getVulkanBackend();

		VkDescriptorPoolSize pool_sizes[] = {
			{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
			{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
			{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
			{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
			{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
		};

		VkDescriptorPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000 * static_cast<uint32_t>(std::size(pool_sizes));
		pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
		pool_info.pPoolSizes = pool_sizes;
		if (vkCreateDescriptorPool(vulkan_backend->getDevice(), &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
			return false;
		}

		ImGui_ImplVulkan_InitInfo init_info{};
		init_info.ApiVersion = VK_API_VERSION_1_3;
		init_info.Instance = vulkan_backend->getInstance();
		init_info.PhysicalDevice = vulkan_backend->getPhysicalDevice();
		init_info.Device = vulkan_backend->getDevice();
		init_info.QueueFamily = vulkan_backend->getGraphicsQueueIndex();
		init_info.Queue = vulkan_backend->getGraphicsQueue();
		init_info.DescriptorPool = descriptor_pool_;
		init_info.MinImageCount = static_cast<uint32_t>((std::max)(size_t(2), swapchain_targets_.size()));
		init_info.ImageCount = static_cast<uint32_t>((std::max)(size_t(2), swapchain_targets_.size()));
		init_info.UseDynamicRendering = true;
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		VkFormat color_attachment_format = vulkan_backend->getSwapchainImageFormat();
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_attachment_format;
		if (!ImGui_ImplVulkan_Init(&init_info)) {
			return false;
		}

		return true;
	}

} // dodoe
