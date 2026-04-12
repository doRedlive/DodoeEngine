#ifndef DODOE_RHI_H
#define DODOE_RHI_H

#include "dopch.h"

#include "vulkan_backend.h"

#include "rhi.h"

struct GLFWwindow;

namespace dodoe {

    enum class RenderApiType;

    struct RhiBackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
        RenderApiType api_type{};
        bool enable_validation{true};
    };

    class RhiContext {
        Scope<VulkanBackend> vulkan_backend_{nullptr};
        rhi::DeviceHandle device_{};
        std::vector<rhi::TextureHandle> swapchain_textures_{};
    public:
        static Scope<RhiContext> create(const RhiBackendCreateInfo& create_info);
        static void destroy(Scope<RhiContext>& backend);

        [[nodiscard]] rhi::DeviceHandle getDevice() const { return device_; }
        [[nodiscard]] const std::vector<rhi::TextureHandle>& getSwapchainTextures() const { return swapchain_textures_; }
        [[nodiscard]] Vector2i getSwapchainExtent2d() const { return vulkan_backend_->getSwapchainExtent2d(); }
        [[nodiscard]] bool acquireNextSwapchainImage(uint32_t& image_index);
        [[nodiscard]] bool presentSwapchainImage(uint32_t image_index);

        [[nodiscard]] VulkanBackend* getVulkanBackend() const { return vulkan_backend_.get(); }

    private:
        void initialize(const RhiBackendCreateInfo& create_info);
        void shutdown();

        void createSwapchainTextures();
    };

} // dodoe

#endif//DODOE_RHI_H
