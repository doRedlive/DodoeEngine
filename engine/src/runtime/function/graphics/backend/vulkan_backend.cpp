// do@Redlive

#include "vulkan_backend.h"

#include "runtime/function/render/render_settings.h"

#include <set>

namespace dodoe {

	namespace {
		static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT,
                                                        VkDebugUtilsMessageTypeFlagsEXT,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void*) {
			DO_ERROR("Validation layer: {}.", pCallbackData->pMessage);
			return VK_FALSE;
		}
	}

	bool VulkanBackend::initialize(const VulkanBackendCreateInfo& info) {
		OutputDebugStringA("[VK] initialize begin\n");
		enable_validation_layers_ = info.enable_validation && checkValidationLayerSupport();
		host_handle_ = info.host_handle;
        instance_extensions_ = getRequiredExtensions();
		OutputDebugStringA("[VK] creating instance...\n");
		createInstance(instance_extensions_.data(), static_cast<int>(instance_extensions_.size()));
		OutputDebugStringA("[VK] instance ok\n");
		if (enable_validation_layers_) {
			OutputDebugStringA("[VK] init debug messenger...\n");
			initializeDebugMessenger();
			OutputDebugStringA("[VK] debug messenger ok\n");
		}
		OutputDebugStringA("[VK] createSurface...\n");
		createSurface(info.window_handle, info.host_handle);
		OutputDebugStringA("[VK] surface ok\n");
		OutputDebugStringA("[VK] pickPhysicalDevice...\n");
		pickPhysicalDevice();
		OutputDebugStringA("[VK] physicalDevice ok\n");
		OutputDebugStringA("[VK] createLogicalDevice...\n");
		createLogicalDevice();
		OutputDebugStringA("[VK] logicalDevice ok\n");
		OutputDebugStringA("[VK] createSwapchain...\n");
		createSwapchain(info.window_handle, info.width, info.height);
		OutputDebugStringA("[VK] swapchain ok\n");
		createCommandPool();
		OutputDebugStringA("[VK] commandPool ok\n");
		createSwapchainImageViews();
		OutputDebugStringA("[VK] imageViews ok, initialize done\n");
		return true;
	}

	void VulkanBackend::shutdown() {
		if (device_ != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(device_);

			for (auto image_view : swapchain_imageviews_) {
				if (image_view != VK_NULL_HANDLE) {
					vkDestroyImageView(device_, image_view, nullptr);
				}
			}
			swapchain_imageviews_.clear();

			if (command_pool_ != VK_NULL_HANDLE) {
				vkDestroyCommandPool(device_, command_pool_, nullptr);
				command_pool_ = VK_NULL_HANDLE;
			}

			if (swapchain_ != VK_NULL_HANDLE) {
				vkDestroySwapchainKHR(device_, swapchain_, nullptr);
				swapchain_ = VK_NULL_HANDLE;
			}

			vkDestroyDevice(device_, nullptr);
			device_ = VK_NULL_HANDLE;
		}

		if (surface_ != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(m_instance, surface_, nullptr);
			surface_ = VK_NULL_HANDLE;
		}

		if (debug_messenger_ != VK_NULL_HANDLE) {
			auto destroy_debug = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
				vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
			if (destroy_debug) {
				destroy_debug(m_instance, debug_messenger_, nullptr);
			}
			debug_messenger_ = VK_NULL_HANDLE;
		}

		if (m_instance != VK_NULL_HANDLE) {
			vkDestroyInstance(m_instance, nullptr);
			m_instance = VK_NULL_HANDLE;
		}
	}

	bool VulkanBackend::checkValidationLayerSupport() {
		uint32_t count;
		vkEnumerateInstanceLayerProperties(&count, nullptr);

		std::vector<VkLayerProperties> available_layers(count);
		vkEnumerateInstanceLayerProperties(&count, available_layers.data());

		for (auto& valid : validation_layers_) {
			bool found{false};
			for (const auto& avail : available_layers) {
				if (strcmp(valid, avail.layerName) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				return false;
			}
		}
		return true;
	}

	void VulkanBackend::createInstance(const char** extensions, int extension_count) {
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = nullptr;
		app_info.pApplicationName = "Dodoe";
		app_info.applicationVersion = VK_API_VERSION_1_3;
		app_info.pEngineName = "Dodoe Renderer";
		app_info.engineVersion = VK_API_VERSION_1_3;
		app_info.apiVersion = VK_API_VERSION_1_3;

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledExtensionCount = extension_count;
		create_info.ppEnabledExtensionNames = extensions;

		VkDebugUtilsMessengerCreateInfoEXT debug_info{};
		debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debug_info.pfnUserCallback = VulkanDebugCallback;

		if (enable_validation_layers_) {
			create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
			create_info.ppEnabledLayerNames = validation_layers_.data();
			create_info.pNext = &debug_info;
		}
		else {
			create_info.enabledLayerCount = 0;
			create_info.ppEnabledLayerNames = nullptr;
			create_info.pNext = nullptr;
		}

		VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
		DO_ASSERT(result == VK_SUCCESS, "VulkanBackend::createInstance failed with VkResult={}", static_cast<int>(result));
	}

	void VulkanBackend::createSurface(GLFWwindow* window_handle, void* host_handle) {
		if (host_handle != nullptr) {
#if defined(DO_PLATFORM_WINDOWS)
			VkWin32SurfaceCreateInfoKHR surfaceInfo{};
			surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceInfo.hwnd = static_cast<HWND>(host_handle);
			surfaceInfo.hinstance = GetModuleHandle(nullptr);
			VkResult result = vkCreateWin32SurfaceKHR(m_instance, &surfaceInfo, nullptr, &surface_);
			DO_ASSERT(result == VK_SUCCESS, "VulkanBackend::createSurface(host) failed");
#else
			DO_ASSERT(false, "createSurface: host unsupported platform");
#endif
		} else if (window_handle != nullptr) {
			VkResult result = glfwCreateWindowSurface(m_instance, window_handle, nullptr, &surface_);
			DO_ASSERT(result == VK_SUCCESS, "VulkanBackend::createSurface GLFW failed");
		} else {
			DO_ASSERT(false, "createSurface: no handle");
		}

	}
	void VulkanBackend::pickPhysicalDevice() {
		OutputDebugStringA("[VK] pickPhysicalDevice: enumerate...\n");
		uint32_t gpu_count;
		vkEnumeratePhysicalDevices(m_instance, &gpu_count, nullptr);
		{ char _b[64]; snprintf(_b, sizeof(_b), "[VK] gpu_count=%u\n", gpu_count); OutputDebugStringA(_b); }
		DO_ASSERT(gpu_count > 0, "No available GPU found!");

		std::vector<VkPhysicalDevice> gpus(gpu_count);
		vkEnumeratePhysicalDevices(m_instance, &gpu_count, gpus.data());

		int use_gpu = 0;
		for (int i = 0; i < gpu_count; i++) {
			VkPhysicalDeviceProperties prop;
			vkGetPhysicalDeviceProperties(gpus[i], &prop);
			if (prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				use_gpu = i;
				break;
			}
		}
		{ char _b[64]; snprintf(_b, sizeof(_b), "[VK] use_gpu=%d, checking suitable...\n", use_gpu); OutputDebugStringA(_b); }
		OutputDebugStringA("[VK] calling isDeviceSuitable...\n");
		DO_ASSERT(isDeviceSuitable(gpus[use_gpu]), "Suitable gpu not found!");

		physical_device_ = gpus[use_gpu];
		OutputDebugStringA("[VK] pickPhysicalDevice done\n");
	}

	void VulkanBackend::createLogicalDevice() {
		queue_family_indices_ = findQueueFamilies(physical_device_);

		std::vector<VkDeviceQueueCreateInfo> queue_infos;
		std::set<uint32_t> queue_families = {queue_family_indices_.graphics_family.value(), 
			queue_family_indices_.present_family.value(), queue_family_indices_.compute_family.value() };

		float queue_priority{1.0f};
		for (uint32_t queue_family : queue_families) {
			VkDeviceQueueCreateInfo queue_info{};
			queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_info.queueFamilyIndex = queue_family;
			queue_info.queueCount = 1;
			queue_info.pQueuePriorities = &queue_priority;
			queue_infos.push_back(queue_info);
		}

		VkPhysicalDeviceFeatures features{};
		features.samplerAnisotropy = VK_TRUE;
		features.fragmentStoresAndAtomics = VK_TRUE;
		features.independentBlend = VK_TRUE;

		VkPhysicalDeviceVulkan13Features supported_vulkan13_features{};
		supported_vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		supported_vulkan13_features.pNext = nullptr;

		VkPhysicalDeviceVulkan12Features supported_vulkan12_features{};
		supported_vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		supported_vulkan12_features.pNext = &supported_vulkan13_features;

		VkPhysicalDeviceFeatures2 supported_features2{};
		supported_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		supported_features2.pNext = &supported_vulkan12_features;
		vkGetPhysicalDeviceFeatures2(physical_device_, &supported_features2);
		DO_ASSERT(supported_vulkan13_features.dynamicRendering == VK_TRUE,
			"VulkanBackend::createLogicalDevice requires dynamicRendering support.");
		DO_ASSERT(supported_vulkan12_features.descriptorBindingPartiallyBound == VK_TRUE,
			"VulkanBackend::createLogicalDevice requires descriptorBindingPartiallyBound support.");
		DO_ASSERT(supported_vulkan12_features.runtimeDescriptorArray == VK_TRUE,
			"VulkanBackend::createLogicalDevice requires runtimeDescriptorArray support.");
		DO_ASSERT(supported_vulkan12_features.timelineSemaphore == VK_TRUE,
			"VulkanBackend::createLogicalDevice requires timelineSemaphore support.");
		DO_ASSERT(supported_vulkan12_features.bufferDeviceAddress == VK_TRUE,
			"VulkanBackend::createLogicalDevice requires bufferDeviceAddress support.");

		VkPhysicalDeviceVulkan13Features vulkan13_features{};
		vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vulkan13_features.pNext = nullptr;
		vulkan13_features.dynamicRendering = VK_TRUE;
		vulkan13_features.synchronization2 = VK_TRUE;

		VkPhysicalDeviceVulkan12Features vulkan12_features{};
		vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vulkan12_features.pNext = &vulkan13_features;
		vulkan12_features.descriptorIndexing = VK_TRUE;
		vulkan12_features.runtimeDescriptorArray = VK_TRUE;
		vulkan12_features.descriptorBindingPartiallyBound = VK_TRUE;
		vulkan12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		vulkan12_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		vulkan12_features.timelineSemaphore = VK_TRUE;
		vulkan12_features.bufferDeviceAddress = VK_TRUE;
		features.geometryShader = VK_TRUE;

		VkPhysicalDeviceFeatures2 enabled_features2{};
		enabled_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		enabled_features2.pNext = &vulkan12_features;
		enabled_features2.features = features;

		VkDeviceCreateInfo device_info{};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = &enabled_features2;
		device_info.flags = 0;
		device_info.pQueueCreateInfos = queue_infos.data();
		device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
		device_info.pEnabledFeatures = nullptr;
		device_info.enabledLayerCount = 0;
		device_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions_.size());
		device_info.ppEnabledExtensionNames = device_extensions_.data();

		VkResult result = vkCreateDevice(physical_device_, &device_info, nullptr, &device_);
		DO_ASSERT(result == VK_SUCCESS, "VulkanBackend::createLogicalDevice failed with VkResult={}", static_cast<int>(result));

		vkGetDeviceQueue(device_, queue_family_indices_.graphics_family.value(), 0, &graphics_queue_);
		vkGetDeviceQueue(device_, queue_family_indices_.present_family.value(), 0, &present_queue_);
		vkGetDeviceQueue(device_, queue_family_indices_.compute_family.value(), 0, &compute_queue_);
	}

	void VulkanBackend::createSwapchain(::GLFWwindow* window_handle, uint32_t width, uint32_t height) {
		VulkanBackend::SwapchainSupportDetails swapchain_details = querySwapchainSupport(physical_device_);
		VkSurfaceFormatKHR chosen_surface_format;
		{
			bool chosen{false};

			for (const auto& surface_format : swapchain_details.formats) {
				if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					chosen_surface_format = surface_format;
					chosen = true;
				}
			}
			if (!chosen) {
				chosen_surface_format = swapchain_details.formats[0];
			}
		}
		VkPresentModeKHR preferred_present_mode;
		switch (RenderSettings::GetPresentMode()) {
		case PresentMode::VSync:
			preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR;
			break;
		case PresentMode::Immediate:
			preferred_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			break;
		case PresentMode::Mailbox:
		default:
			preferred_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}

		VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& present_mode : swapchain_details.present_modes) {
			if (present_mode == preferred_present_mode) {
				chosen_present_mode = preferred_present_mode;
				break;
			}
		}
		VkExtent2D chosen_extent;
		{
			int sel_w = 0, sel_h = 0;
			if (width > 0 && height > 0) {
				sel_w = static_cast<int>(width);
				sel_h = static_cast<int>(height);
			} else if (swapchain_details.capabilities.currentExtent.width != UINT32_MAX
				&& swapchain_details.capabilities.currentExtent.width > 0
				&& swapchain_details.capabilities.currentExtent.height > 0) {
				sel_w = static_cast<int>(swapchain_details.capabilities.currentExtent.width);
				sel_h = static_cast<int>(swapchain_details.capabilities.currentExtent.height);
			} else if (host_handle_ != nullptr) {
				RECT rect;
				GetClientRect(static_cast<HWND>(host_handle_), &rect);
				sel_w = rect.right - rect.left;
				sel_h = rect.bottom - rect.top;
			} else {
				glfwGetFramebufferSize(window_handle, &sel_w, &sel_h);
			}
			chosen_extent.width = std::clamp(static_cast<uint32_t>(sel_w),
				swapchain_details.capabilities.minImageExtent.width,
				swapchain_details.capabilities.maxImageExtent.width);
			chosen_extent.height = std::clamp(static_cast<uint32_t>(sel_h),
				swapchain_details.capabilities.minImageExtent.height,
				swapchain_details.capabilities.maxImageExtent.height);
		}
        uint32_t image_count = swapchain_details.capabilities.minImageCount + 1;
        if (swapchain_details.capabilities.maxImageCount > 0 &&
            image_count > swapchain_details.capabilities.maxImageCount) {
            image_count = swapchain_details.capabilities.maxImageCount;
        }

		VkSwapchainCreateInfoKHR swapchain_info{};
		swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_info.pNext = nullptr;
		swapchain_info.flags = 0;
		swapchain_info.surface = surface_;
		swapchain_info.minImageCount = image_count;
		swapchain_info.imageFormat = chosen_surface_format.format;
		swapchain_info.imageColorSpace = chosen_surface_format.colorSpace;
		swapchain_info.imageExtent = chosen_extent;
		swapchain_info.imageArrayLayers = 1;
		VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if ((swapchain_details.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0) {
			image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		swapchain_info.imageUsage = image_usage;

		uint32_t queue_family_indices[] = {queue_family_indices_.graphics_family.value(), queue_family_indices_.present_family.value()};	
		if (queue_family_indices_.graphics_family != queue_family_indices_.present_family) {
			swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchain_info.queueFamilyIndexCount = 2;
			swapchain_info.pQueueFamilyIndices = queue_family_indices;
		}
		else {
			swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchain_info.queueFamilyIndexCount = 0;
			swapchain_info.pQueueFamilyIndices = nullptr;
		}

		swapchain_info.preTransform = swapchain_details.capabilities.currentTransform;
		swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchain_info.presentMode = chosen_present_mode;
		swapchain_info.clipped = VK_TRUE;
		swapchain_info.oldSwapchain = VK_NULL_HANDLE;

		VkResult result = vkCreateSwapchainKHR(device_, &swapchain_info, nullptr, &swapchain_);
		DO_ASSERT(result == VK_SUCCESS, "VulkanBackend::createSwapchain failed with VkResult={}", static_cast<int>(result));

		vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
		swapchain_images_.resize(image_count);
		vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());

		swapchain_image_format_ = chosen_surface_format.format;
		swapchain_extent_.width = chosen_extent.width;
		swapchain_extent_.height = chosen_extent.height;
		scissor_ = {{0, 0}, {swapchain_extent_.width, swapchain_extent_.height}};
	}

	void VulkanBackend::createSwapchainImageViews() {
		swapchain_imageviews_.clear();
		swapchain_imageviews_.reserve(swapchain_images_.size());
		for (const auto swapchain_image : swapchain_images_) {
			VkImageViewCreateInfo view_info{};
			view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view_info.image = swapchain_image;
			view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view_info.format = swapchain_image_format_;
			view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view_info.subresourceRange.baseMipLevel = 0;
			view_info.subresourceRange.levelCount = 1;
			view_info.subresourceRange.baseArrayLayer = 0;
			view_info.subresourceRange.layerCount = 1;

			VkImageView image_view = VK_NULL_HANDLE;
			DO_ASSERT(vkCreateImageView(device_, &view_info, nullptr, &image_view) == VK_SUCCESS,
				"VulkanBackend::createSwapchainImageViews failed to create swapchain image view.");
			swapchain_imageviews_.push_back(image_view);
		}
	}

	void VulkanBackend::createCommandPool() {
		VkCommandPoolCreateInfo cmd_pool_info{};
		cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmd_pool_info.pNext = nullptr;
		cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		cmd_pool_info.queueFamilyIndex = queue_family_indices_.graphics_family.value();

		vkCreateCommandPool(device_, &cmd_pool_info, nullptr, &command_pool_);
	}

	bool VulkanBackend::acquireNextImage(uint32_t& image_index, VkSemaphore signal_semaphore) {
		if (device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
			return false;
		}

		VkResult acquire_result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, signal_semaphore, VK_NULL_HANDLE, &image_index);
		if (acquire_result == VK_SUBOPTIMAL_KHR || acquire_result == VK_SUCCESS) {
			return true;
		}

		DO_ERROR("VulkanBackend::acquireNextImage failed with VkResult={}", static_cast<int>(acquire_result));
		return false;
	}

	bool VulkanBackend::presentImage(uint32_t image_index, VkSemaphore wait_semaphore) {
		if (swapchain_ == VK_NULL_HANDLE || present_queue_ == VK_NULL_HANDLE) {
			return false;
		}

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		if (wait_semaphore != VK_NULL_HANDLE) {
			present_info.waitSemaphoreCount = 1;
			present_info.pWaitSemaphores = &wait_semaphore;
		} else {
			present_info.waitSemaphoreCount = 0;
			present_info.pWaitSemaphores = nullptr;
		}
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &swapchain_;
		present_info.pImageIndices = &image_index;
		present_info.pResults = nullptr;

		VkResult present_result = vkQueuePresentKHR(present_queue_, &present_info);
		if (present_result == VK_SUBOPTIMAL_KHR || present_result == VK_SUCCESS) {
			return true;
		}

		DO_ERROR("VulkanBackend::presentImage failed with VkResult={}", static_cast<int>(present_result));
		return false;
	}

	bool VulkanBackend::recreateSwapchain(GLFWwindow* window_handle, uint32_t width, uint32_t height) {
		if (width == 0 || height == 0) {
			return false;
		}

		vkDeviceWaitIdle(device_);

		for (auto image_view : swapchain_imageviews_) {
			if (image_view != VK_NULL_HANDLE) {
				vkDestroyImageView(device_, image_view, nullptr);
			}
		}
		swapchain_imageviews_.clear();

		if (swapchain_ != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(device_, swapchain_, nullptr);
			swapchain_ = VK_NULL_HANDLE;
		}

		swapchain_images_.clear();

		createSwapchain(window_handle, width, height);
		createSwapchainImageViews();
		return true;
	}

    std::vector<const char*> VulkanBackend::getRequiredExtensions() {
        uint32_t     glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enable_validation_layers_) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

#if defined(DO_PLATFORM_WINDOWS)
        if (host_handle_ != nullptr) {
            extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        }
#endif

        return extensions;
    }

	void VulkanBackend::initializeDebugMessenger() {
		VkDebugUtilsMessengerCreateInfoEXT create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create_info.pfnUserCallback = VulkanDebugCallback;

		auto fun = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
		if (fun) {
			fun(m_instance, &create_info, nullptr, &debug_messenger_);
		}
		else {
			DO_ERROR("Create debug messenger failed!");
		}
	}

 	bool VulkanBackend::isDeviceSuitable(VkPhysicalDevice gpu) {
		OutputDebugStringA("[VK] isDeviceSuitable enter\n");
        auto queue_indices           = findQueueFamilies(gpu);
        OutputDebugStringA("[VK] findQueueFamilies ok\n");
        bool is_extensions_supported = checkDeviceExtensionSupport(gpu);
        bool is_swapchain_adequate   = false;
        if (is_extensions_supported) {
            SwapchainSupportDetails swapchain_support_details = querySwapchainSupport(gpu);
            is_swapchain_adequate =
                !swapchain_support_details.formats.empty() && !swapchain_support_details.present_modes.empty();
        }

        VkPhysicalDeviceFeatures gpu_features;
        vkGetPhysicalDeviceFeatures(gpu, &gpu_features);

        if (!queue_indices.isComplete() || !is_swapchain_adequate || !gpu_features.samplerAnisotropy) {
            return false;
        }

        return true;
    }

	VulkanBackend::QueueFamilyIndices VulkanBackend::findQueueFamilies(VkPhysicalDevice gpu)
    {
        QueueFamilyIndices indices;
        uint32_t           queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, queue_families.data());

        int i = 0;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics_family = i;
            }
            if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                indices.compute_family = i;
            }

            VkBool32 is_present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface_, &is_present_support);
            if (is_present_support) {
                indices.present_family = i;
            }

            if (indices.isComplete()) {
                break;
            }
            i++;
        }
        return indices;
    }

	bool VulkanBackend::checkDeviceExtensionSupport(VkPhysicalDevice gpu) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extension_count, available_extensions.data());

        std::set<String> required_extensions(device_extensions_.begin(), device_extensions_.end());
        for (const auto& extension : available_extensions) {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

	VulkanBackend::SwapchainSupportDetails VulkanBackend::querySwapchainSupport(VkPhysicalDevice gpu) {
        VulkanBackend::SwapchainSupportDetails details_result;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface_, &details_result.capabilities);

        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface_, &format_count, nullptr);
        if (format_count != 0) {
            details_result.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                gpu, surface_, &format_count, details_result.formats.data());
        }

        uint32_t presentmode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface_, &presentmode_count, nullptr);
        if (presentmode_count != 0) {
            details_result.present_modes.resize(presentmode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                gpu, surface_, &presentmode_count, details_result.present_modes.data());
        }

        return details_result;
    }

} // dodoe
