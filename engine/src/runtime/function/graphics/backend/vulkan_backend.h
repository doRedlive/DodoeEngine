// do@Redlive
#pragma once

#include "dopch.h"

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include "GLFW/glfw3.h"
#include "vulkan/vulkan.h"

namespace dodoe {

	struct VulkanBackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
        void*       host_handle{nullptr};
		bool        enable_validation{true};
	};

	class VulkanBackend : public Managed<VulkanBackend, VulkanBackendCreateInfo> {
        friend class Managed<VulkanBackend, VulkanBackendCreateInfo>;
		struct QueueFamilyIndices {
			std::optional<uint32_t> graphics_family;
			std::optional<uint32_t> present_family;
			std::optional<uint32_t> compute_family;

			bool isComplete() { return graphics_family.has_value() && present_family.has_value() && compute_family.has_value();; }
    	};

		struct SwapchainSupportDetails {
			VkSurfaceCapabilitiesKHR        capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR>   present_modes;
		};

		VkInstance m_instance;
		VkSurfaceKHR surface_;
		VkPhysicalDevice physical_device_;
		VkDevice device_;
		VkQueue present_queue_;
		VkQueue graphics_queue_;
		VkQueue compute_queue_;
		VkSwapchainKHR swapchain_;
		std::vector<VkImage> swapchain_images_;
		std::vector<VkImageView> swapchain_imageviews_;
		VkFormat swapchain_image_format_;
		VkExtent2D swapchain_extent_;
		VkCommandPool command_pool_;
		VkViewport viewport_;
		VkRect2D scissor_;	
		VkDebugUtilsMessengerEXT debug_messenger_;
		QueueFamilyIndices queue_family_indices_;
		bool enable_validation_layers_{false};
		void* host_handle_{nullptr};

		const std::vector<const char*> validation_layers_{"VK_LAYER_KHRONOS_validation"};
		std::vector<const char*> instance_extensions_{};
		std::vector<const char*> device_extensions_ = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	public:

		[[nodiscard]] VkInstance getInstance() { return m_instance; }
		[[nodiscard]] VkPhysicalDevice getPhysicalDevice() { return physical_device_; }
		[[nodiscard]] VkDevice getDevice() { return device_; }
		[[nodiscard]] VkQueue getGraphicsQueue() { return graphics_queue_; }
		[[nodiscard]] VkQueue getComputeQueue() { return compute_queue_; }
		[[nodiscard]] VkCommandPool getCommandPool() { return command_pool_; }
		[[nodiscard]] uint32_t getGraphicsQueueIndex() { return queue_family_indices_.graphics_family.value(); }
		[[nodiscard]] int getComputeQueueIndex() const { return queue_family_indices_.compute_family.has_value() ? static_cast<int>(queue_family_indices_.compute_family.value()) : -1; }
		[[nodiscard]] int getPresentQueueIndex() const { return queue_family_indices_.present_family.has_value() ? static_cast<int>(queue_family_indices_.present_family.value()) : -1; }
		[[nodiscard]] const std::vector<const char*>& getInstanceExtensions() { return instance_extensions_; }
		[[nodiscard]] const std::vector<VkImage>& getSwapchainImages() { return swapchain_images_; }
		[[nodiscard]] const std::vector<VkImageView>& getSwapchainImageViews() { return swapchain_imageviews_; }
		[[nodiscard]] VkFormat getSwapchainImageFormat() { return swapchain_image_format_; }
		[[nodiscard]] const std::vector<const char*>& getDeviceExtensions() { return device_extensions_; }
		[[nodiscard]] Vector2i getSwapchainExtent2d() { return Vector2i(swapchain_extent_.width, swapchain_extent_.height); }
		[[nodiscard]] bool acquireNextImage(uint32_t& image_index, VkSemaphore signal_semaphore);
		[[nodiscard]] bool presentImage(uint32_t image_index, VkSemaphore wait_semaphore);
		[[nodiscard]] bool recreateSwapchain(GLFWwindow* window_handle);

	private:
		bool initialize(const VulkanBackendCreateInfo& info);
		void shutdown();

		bool checkValidationLayerSupport();
		void createInstance(const char** extension, int extension_count);
		void pickPhysicalDevice();
		void createLogicalDevice();
		void createSurface(GLFWwindow* window_handle, void* host_handle);
		void createSwapchain(GLFWwindow* window_handle);
		void createSwapchainImageViews();
		void createCommandPool();

        std::vector<const char*> getRequiredExtensions();
		void initializeDebugMessenger();
		bool isDeviceSuitable(VkPhysicalDevice gpu);
		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice gpu);
		bool checkDeviceExtensionSupport(VkPhysicalDevice gpu);
		SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice gpu);
	};

} // dodoe
