//
// Created by Redlive on 2026/4/5.
//

#ifndef DODOE_VULKAN_BACKEND_H
#define DODOE_VULKAN_BACKEND_H

#include "dopch.h"

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include "GLFW/glfw3.h"
#include "vulkan/vulkan.h"

namespace dodoe {

	struct VulkanBackendCreateInfo {
        GLFWwindow* window_handle;
		bool enable_validation{true};
	};

	class VulkanBackend {
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

		VkInstance instance_;
		VkSurfaceKHR surface_;
		VkPhysicalDevice physical_device_;
		VkDevice device_;
		VkQueue present_queue_;
		VkQueue graphics_queue_;
		VkQueue compute_queue_;
		VkSwapchainKHR swapchain_;
		std::vector<VkImage> swapchain_images_;
		// std::vector<VkImageView> swapchain_imageviews_;
		std::vector<VkFence> swapchain_fences_;
		VkFormat swapchain_image_format_;
		VkExtent2D swapchain_extent_;
		VkCommandPool command_pool_;
		VkViewport viewport_;
		VkRect2D scissor_;	
		VkDebugUtilsMessengerEXT debug_messenger_;
		QueueFamilyIndices queue_family_indices_;
		bool enable_validation_layers_{false};

		const std::vector<const char*> validation_layers_{"VK_LAYER_KHRONOS_validation"};
		std::vector<const char*> device_extensions_ = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	public:
		static Scope<VulkanBackend> create(const VulkanBackendCreateInfo& info);
		static void destroy(Scope<VulkanBackend>& context);

		[[nodiscard]] VkInstance getInstance() { return instance_; }
		[[nodiscard]] VkPhysicalDevice getPhysicalDevice() { return physical_device_; }
		[[nodiscard]] VkDevice getDevice() { return device_; }
		[[nodiscard]] VkQueue getGraphicsQueue() { return graphics_queue_; }
		[[nodiscard]] uint32_t getGraphicsQueueIndex() { return queue_family_indices_.graphics_family.value(); }
		[[nodiscard]] const std::vector<VkImage>& getSwapchainImages() { return swapchain_images_; }
		[[nodiscard]] VkFormat getSwapchainImageFormat() { return swapchain_image_format_; }
		[[nodiscard]] const std::vector<const char*>& getDeviceExtensions() { return device_extensions_; }
		[[nodiscard]] Vector2i getSwapchainExtent2d() { return Vector2i(swapchain_extent_.width, swapchain_extent_.height); }

	private:
		void initialize(const VulkanBackendCreateInfo& info);
		void shutdown();

		bool checkValidationLayerSupport();
		void createInstance(const char** extension, int extension_count);
		void pickPhysicalDevice();
		void createLogicalDevice();
		void createSurface(GLFWwindow* window_handle);
		void createSwapchain(GLFWwindow* window_handle);
		// void createSwapchainImageViews();
		void createSwapchainFences();
		void createCommandPool();

        std::vector<const char*> getRequiredExtensions();
		void initializeDebugMessenger();
		bool isDeviceSuitable(VkPhysicalDevice gpu);
		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice gpu);
		bool checkDeviceExtensionSupport(VkPhysicalDevice gpu);
		SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice gpu);
	};

} // dodoe

#endif//DODOE_VULKAN_BACKEND_H