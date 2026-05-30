#ifndef DODOE_RHI_H
#define DODOE_RHI_H

#include "dopch.h"

#include "vulkan_backend.h"

#include "rhi.h"

struct GLFWwindow;

namespace dodoe {

    enum class RenderBackendApiType;

    struct RhiBackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
        RenderBackendApiType api_type{};
        bool enable_validation{true};
    };

    class RhiContext {
        rhi::DeviceHandle device_{};
        rhi::CommandListHandle cmd_{};
        std::vector<rhi::TextureHandle> swapchain_textures_{};
        Scope<VulkanBackend> vulkan_backend_{nullptr};
        GLFWwindow* window_handle_{nullptr};
        std::vector<VkSemaphore> acquire_semaphores_{};
        std::vector<VkSemaphore> present_semaphores_{};
        std::vector<VkFence> frame_fences_{};
        size_t current_frame_slot_{0};
        size_t active_frame_slot_{(std::numeric_limits<size_t>::max)()};
    public:
        static Scope<RhiContext> Create(const RhiBackendCreateInfo& create_info);
        static void Destroy(Scope<RhiContext>& backend);

        [[nodiscard]] rhi::DeviceHandle getDevice() const { return device_; }
        [[nodiscard]] const std::vector<rhi::TextureHandle>& getSwapchainTextures() const { return swapchain_textures_; }
        [[nodiscard]] Vector2i getSwapchainExtent2d() const { return vulkan_backend_->getSwapchainExtent2d(); }
        [[nodiscard]] bool acquireNextSwapchainImage(uint32_t& image_index);
        [[nodiscard]] bool presentSwapchainImage(uint32_t image_index);
        [[nodiscard]] bool recreateSwapchain();
        [[nodiscard]] const rhi::CommandListHandle& getCommandList() { return cmd_; }

        [[nodiscard]] VulkanBackend* getVulkanBackend() const { return vulkan_backend_.get(); }

    private:
        void initialize(const RhiBackendCreateInfo& create_info);
        void shutdown();

        void createSwapchainTextures();
        void createSwapchainSemaphores();
        void destroySwapchainSemaphores();
    };

} // dodoe

#endif//DODOE_RHI_H
