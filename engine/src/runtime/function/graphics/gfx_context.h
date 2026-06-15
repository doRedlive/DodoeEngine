#ifndef DODOE_GFX_H
#define DODOE_GFX_H

#include "dopch.h"

#include "backend/vulkan_backend.h"

#include "gfx.h"

struct GLFWwindow;

namespace dodoe {

    enum class RenderBackendApiType;

    struct GfxBackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
        RenderBackendApiType api_type{};
        bool enable_validation{true};
    };

    class GfxContext {
        GfxDeviceHandle device_{};
        GfxCommandListHandle cmd_{};
        std::vector<GfxTextureHandle> swapchain_textures_{};
        Scope<VulkanBackend> vulkan_backend_{nullptr};
        GLFWwindow* window_handle_{nullptr};
        std::vector<VkSemaphore> acquire_semaphores_{};
        std::vector<VkSemaphore> present_semaphores_{};
        std::vector<VkFence> frame_fences_{};
        size_t current_frame_slot_{0};
        size_t active_frame_slot_{(std::numeric_limits<size_t>::max)()};
    public:
        static Scope<GfxContext> Create(const GfxBackendCreateInfo& create_info);
        static void Destroy(Scope<GfxContext>& backend);

        [[nodiscard]] GfxDeviceHandle getDevice() const { return device_; }
        [[nodiscard]] const std::vector<GfxTextureHandle>& getSwapchainTextures() const { return swapchain_textures_; }
        [[nodiscard]] Vector2i getSwapchainExtent2d() const { return vulkan_backend_->getSwapchainExtent2d(); }
        [[nodiscard]] bool acquireNextSwapchainImage(uint32_t& image_index);
        [[nodiscard]] bool presentSwapchainImage(uint32_t image_index);
        [[nodiscard]] bool recreateSwapchain();
        [[nodiscard]] const GfxCommandListHandle& getCommandList() { return cmd_; }

        [[nodiscard]] VulkanBackend* getVulkanBackend() const { return vulkan_backend_.get(); }

    private:
        void initialize(const GfxBackendCreateInfo& create_info);
        void shutdown();

        void createSwapchainTextures();
        void createSwapchainSemaphores();
        void destroySwapchainSemaphores();
    };

} // dodoe

#endif//DODOE_GFX_H
