//
// Created by Redlive on 2026/3/17.
//

#include "vk_render_context.h"

namespace dodoe {

	Scope<VkRenderContext> VkRenderContext::create(VkRenderContextCreateInfo create_info) {
		Scope<VkRenderContext> vk_render_context = create_scope<VkRenderContext>();
		vk_render_context->initialize(create_info);
		return vk_render_context;
	}

	void VkRenderContext::destroy(Scope<VkRenderContext>& vk_render_context) {
		if (!vk_render_context) {
			return;
		}

		vk_render_context->shutdown();
		vk_render_context.reset();
	}

	void VkRenderContext::initialize(VkRenderContextCreateInfo create_info) {
		DoAssert(create_info.window, "VkRenderContextCreateInfo::window must not be null.");

		shutdown();

		validation_layers_enabled_ = create_info.enable_validation_layers;

		create_instance(create_info);
		setup_debug_messenger();
		create_surface(create_info.window);
		pick_physical_device(create_info);
		create_logical_device(create_info);

		DoInfo("VkRenderContext initialize success.");
	}

	void VkRenderContext::shutdown() {
		if (device_ != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(device_);
			vkDestroyDevice(device_, nullptr);
			device_ = VK_NULL_HANDLE;
		}

		graphics_queue_ = VK_NULL_HANDLE;
		present_queue_ = VK_NULL_HANDLE;
		graphics_queue_family_index_ = VK_QUEUE_FAMILY_IGNORED;
		present_queue_family_index_ = VK_QUEUE_FAMILY_IGNORED;
		physical_device_ = VK_NULL_HANDLE;

		if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(instance_, surface_, nullptr);
			surface_ = VK_NULL_HANDLE;
		}

		destroy_debug_messenger();

		if (instance_ != VK_NULL_HANDLE) {
			vkDestroyInstance(instance_, nullptr);
			instance_ = VK_NULL_HANDLE;
		}

		validation_layers_enabled_ = false;
	}

	void VkRenderContext::create_instance(const VkRenderContextCreateInfo& create_info) {
		if (validation_layers_enabled_) {
			DoAssert(check_validation_layer_support(create_info.validation_layers),
				"VkRenderContext::create_instance: Requested validation layers are not available.");
		}

		uint32_t glfw_extension_count = 0;
		const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
		DoAssert(glfw_extensions, "VkRenderContext::create_instance: Failed to get GLFW required instance extensions.");

		std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
		if (validation_layers_enabled_) {
			extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

#ifdef DO_PLATFORM_MACOS
		extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

		VkApplicationInfo application_info {};
		application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		application_info.pApplicationName = "Dodoe";
		application_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
		application_info.pEngineName = "DodoeRenderer";
		application_info.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
		application_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo instance_create_info {};
		instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_create_info.pApplicationInfo = &application_info;
#ifdef DO_PLATFORM_MACOS
		instance_create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
		instance_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instance_create_info.ppEnabledExtensionNames = extensions.data();

		if (validation_layers_enabled_) {
			instance_create_info.enabledLayerCount = static_cast<uint32_t>(create_info.validation_layers.size());
			instance_create_info.ppEnabledLayerNames = create_info.validation_layers.empty() ? nullptr : create_info.validation_layers.data();
		}

		const VkResult result = vkCreateInstance(&instance_create_info, nullptr, &instance_);
		DoAssert(result == VK_SUCCESS, "VkRenderContext::create_instance: vkCreateInstance failed.");
	}

	void VkRenderContext::create_surface(GLFWwindow* window) {
		DoAssert(instance_ != VK_NULL_HANDLE, "VkRenderContext::create_surface: Instance must be valid.");
		DoAssert(window, "VkRenderContext::create_surface: GLFWwindow must not be null.");

		const VkResult result = glfwCreateWindowSurface(instance_, window, nullptr, &surface_);
		DoAssert(result == VK_SUCCESS, "VkRenderContext::create_surface: glfwCreateWindowSurface failed.");
	}

	void VkRenderContext::pick_physical_device(const VkRenderContextCreateInfo& create_info) {
		DoAssert(instance_ != VK_NULL_HANDLE, "VkRenderContext::pick_physical_device: Instance must be valid.");

		uint32_t physical_device_count = 0;
		vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr);
		DoAssert(physical_device_count > 0, "VkRenderContext::pick_physical_device: No Vulkan physical device was found.");

		std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
		vkEnumeratePhysicalDevices(instance_, &physical_device_count, physical_devices.data());

		for (const VkPhysicalDevice physical_device : physical_devices) {
			if (!is_device_suitable(physical_device, create_info)) {
				continue;
			}

			const QueueFamilyIndices queue_family_indices = find_queue_families(physical_device);
			physical_device_ = physical_device;
			graphics_queue_family_index_ = queue_family_indices.graphics_family.value();
			present_queue_family_index_ = queue_family_indices.present_family.value();
			return;
		}

		DoAssert(false, "VkRenderContext::pick_physical_device: No suitable Vulkan physical device was found.");
	}

	void VkRenderContext::create_logical_device(const VkRenderContextCreateInfo& create_info) {
		DoAssert(physical_device_ != VK_NULL_HANDLE, "VkRenderContext::create_logical_device: Physical device must be valid.");

		std::vector<uint32_t> queue_family_indices {graphics_queue_family_index_};
		if (present_queue_family_index_ != graphics_queue_family_index_) {
			queue_family_indices.push_back(present_queue_family_index_);
		}

		float queue_priority = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queue_create_infos {};
		queue_create_infos.reserve(queue_family_indices.size());

		for (const uint32_t queue_family_index : queue_family_indices) {
			VkDeviceQueueCreateInfo queue_create_info {};
			queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info.queueFamilyIndex = queue_family_index;
			queue_create_info.queueCount = 1;
			queue_create_info.pQueuePriorities = &queue_priority;
			queue_create_infos.push_back(queue_create_info);
		}

		VkPhysicalDeviceFeatures device_features {};

		VkDeviceCreateInfo device_create_info {};
		device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
		device_create_info.pQueueCreateInfos = queue_create_infos.data();
		device_create_info.pEnabledFeatures = &device_features;
		device_create_info.enabledExtensionCount = static_cast<uint32_t>(create_info.device_extensions.size());
		device_create_info.ppEnabledExtensionNames = create_info.device_extensions.empty() ? nullptr : create_info.device_extensions.data();

		if (validation_layers_enabled_) {
			device_create_info.enabledLayerCount = static_cast<uint32_t>(create_info.validation_layers.size());
			device_create_info.ppEnabledLayerNames = create_info.validation_layers.empty() ? nullptr : create_info.validation_layers.data();
		}

		const VkResult result = vkCreateDevice(physical_device_, &device_create_info, nullptr, &device_);
		DoAssert(result == VK_SUCCESS, "VkRenderContext::create_logical_device: vkCreateDevice failed.");

		vkGetDeviceQueue(device_, graphics_queue_family_index_, 0, &graphics_queue_);
		vkGetDeviceQueue(device_, present_queue_family_index_, 0, &present_queue_);
	}

	bool VkRenderContext::check_validation_layer_support(const std::vector<const char*>& validation_layers) const {
		uint32_t layer_count = 0;
		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

		std::vector<VkLayerProperties> available_layers(layer_count);
		vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

		for (const char* requested_layer : validation_layers) {
			bool layer_found = false;
			for (const VkLayerProperties& available_layer : available_layers) {
				if (std::string(available_layer.layerName) == requested_layer) {
					layer_found = true;
					break;
				}
			}

			if (!layer_found) {
				return false;
			}
		}

		return true;
	}

	bool VkRenderContext::check_device_extension_support(VkPhysicalDevice physical_device, const std::vector<const char*>& device_extensions) const {
		if (device_extensions.empty()) {
			return true;
		}

		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

		std::vector<VkExtensionProperties> available_extensions(extension_count);
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

		for (const char* requested_extension : device_extensions) {
			bool extension_found = false;
			for (const VkExtensionProperties& available_extension : available_extensions) {
				if (std::string(available_extension.extensionName) == requested_extension) {
					extension_found = true;
					break;
				}
			}

			if (!extension_found) {
				return false;
			}
		}

		return true;
	}

	VkRenderContext::QueueFamilyIndices VkRenderContext::find_queue_families(VkPhysicalDevice physical_device) const {
		QueueFamilyIndices queue_family_indices {};

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

		for (uint32_t queue_family_index = 0; queue_family_index < queue_family_count; ++queue_family_index) {
			const VkQueueFamilyProperties& queue_family = queue_families[queue_family_index];

			if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				queue_family_indices.graphics_family = queue_family_index;
			}

			VkBool32 present_supported = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_index, surface_, &present_supported);
			if (present_supported == VK_TRUE) {
				queue_family_indices.present_family = queue_family_index;
			}

			if (queue_family_indices.is_complete()) {
				break;
			}
		}

		return queue_family_indices;
	}

	bool VkRenderContext::is_device_suitable(VkPhysicalDevice physical_device, const VkRenderContextCreateInfo& create_info) const {
		const QueueFamilyIndices queue_family_indices = find_queue_families(physical_device);
		if (!queue_family_indices.is_complete()) {
			return false;
		}

		if (!check_device_extension_support(physical_device, create_info.device_extensions)) {
			return false;
		}

		return true;
	}
	
	void VkRenderContext::setup_debug_messenger() {
		if (!validation_layers_enabled_) {
			return;
		}

		DoAssert(instance_ != VK_NULL_HANDLE, "VkRenderContext::setup_debug_messenger: Instance must be valid.");

		const auto vk_create_debug_utils_messenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
		DoAssert(vk_create_debug_utils_messenger != nullptr,
			"VkRenderContext::setup_debug_messenger: vkCreateDebugUtilsMessengerEXT is not available.");

		const VkDebugUtilsMessengerCreateInfoEXT create_info = make_debug_messenger_create_info();
		const VkResult result = vk_create_debug_utils_messenger(instance_, &create_info, nullptr, &debug_messenger_);
		DoAssert(result == VK_SUCCESS, "VkRenderContext::setup_debug_messenger: Failed to create debug messenger.");
	}

	void VkRenderContext::destroy_debug_messenger() {
		if (debug_messenger_ == VK_NULL_HANDLE || instance_ == VK_NULL_HANDLE) {
			return;
		}

		const auto vk_destroy_debug_utils_messenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
		DoAssert(vk_destroy_debug_utils_messenger != nullptr,
			"VkRenderContext::destroy_debug_messenger: vkDestroyDebugUtilsMessengerEXT is not available.");

		vk_destroy_debug_utils_messenger(instance_, debug_messenger_, nullptr);
		debug_messenger_ = VK_NULL_HANDLE;
	}

	VkDebugUtilsMessengerCreateInfoEXT VkRenderContext::make_debug_messenger_create_info() const {
		VkDebugUtilsMessengerCreateInfoEXT create_info {};
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create_info.pfnUserCallback = debug_messenger_callback;
		create_info.pUserData = nullptr;
		return create_info;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL VkRenderContext::debug_messenger_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		VkDebugUtilsMessageTypeFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data) {
		(void)message_severity;
		(void)message_type;
		(void)user_data;

		DoError("Vulkan validation error: {}", callback_data != nullptr ? callback_data->pMessage : "Unknown error");
		return VK_FALSE;
	}

} // dodoe