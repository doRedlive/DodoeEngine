//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_VK_RENDER_CONTEXT_H
#define DODOE_VK_RENDER_CONTEXT_H

#include "dopch.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace dodoe {

    struct VkRenderContextCreateInfo {
        GLFWwindow* window {nullptr};
        bool enable_validation_layers {false};
        std::vector<const char*> validation_layers {};
        std::vector<const char*> device_extensions {};
    };

    class VkRenderContext {
    public:
        static Scope<VkRenderContext> create(VkRenderContextCreateInfo create_info);
        static void destroy(Scope<VkRenderContext>& vk_render_context);

        [[nodiscard]] VkInstance instance() const { return instance_; }
        [[nodiscard]] VkSurfaceKHR surface() const { return surface_; }
        [[nodiscard]] VkPhysicalDevice physical_device() const { return physical_device_; }
        [[nodiscard]] VkDevice device() const { return device_; }
        [[nodiscard]] VkQueue graphics_queue() const { return graphics_queue_; }
        [[nodiscard]] VkQueue present_queue() const { return present_queue_; }
        [[nodiscard]] uint32_t graphics_queue_family_index() const { return graphics_queue_family_index_; }
        [[nodiscard]] uint32_t present_queue_family_index() const { return present_queue_family_index_; }
        [[nodiscard]] bool is_valid() const { return instance_ != VK_NULL_HANDLE && physical_device_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE; }

    private:
        struct QueueFamilyIndices {
            std::optional<uint32_t> graphics_family {};
            std::optional<uint32_t> present_family {};

            [[nodiscard]] bool is_complete() const {
                return graphics_family.has_value() && present_family.has_value();
            }
        };

        void initialize(VkRenderContextCreateInfo create_info);
        void shutdown();

        void create_instance(const VkRenderContextCreateInfo& create_info);
        void setup_debug_messenger();
        void destroy_debug_messenger();
        void create_surface(GLFWwindow* window);
        void pick_physical_device(const VkRenderContextCreateInfo& create_info);
        void create_logical_device(const VkRenderContextCreateInfo& create_info);
        
        [[nodiscard]] bool check_validation_layer_support(const std::vector<const char*>& validation_layers) const;
        [[nodiscard]] bool check_device_extension_support(VkPhysicalDevice physical_device, const std::vector<const char*>& device_extensions) const;
        [[nodiscard]] QueueFamilyIndices find_queue_families(VkPhysicalDevice physical_device) const;
        [[nodiscard]] bool is_device_suitable(VkPhysicalDevice physical_device, const VkRenderContextCreateInfo& create_info) const;
        [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT make_debug_messenger_create_info() const;

        static VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
            VkDebugUtilsMessageTypeFlagsEXT message_type,
            const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
            void* user_data);

    private:
        VkInstance instance_ {VK_NULL_HANDLE};
        VkDebugUtilsMessengerEXT debug_messenger_ {VK_NULL_HANDLE};
        VkSurfaceKHR surface_ {VK_NULL_HANDLE};
        VkPhysicalDevice physical_device_ {VK_NULL_HANDLE};
        VkDevice device_ {VK_NULL_HANDLE};
        VkQueue graphics_queue_ {VK_NULL_HANDLE};
        VkQueue present_queue_ {VK_NULL_HANDLE};
        uint32_t graphics_queue_family_index_ {VK_QUEUE_FAMILY_IGNORED};
        uint32_t present_queue_family_index_ {VK_QUEUE_FAMILY_IGNORED};
        bool validation_layers_enabled_ {false};
    };

} // dodoe

#endif//DODOE_VK_RENDER_CONTEXT_H