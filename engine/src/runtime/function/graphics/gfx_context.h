// do@Redlive
#pragma once

#include "dopch.h"

#include "backend/vulkan_backend.h"
#include "backend/opengl_backend.h"
#include "backend/dx12_backend.h"

#include "gfx.h"

struct GLFWwindow;

namespace dodoe {

    enum class RenderBackendApiType;

    struct GfxBackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
        RenderBackendApiType api_type{};
        Bool enable_validation{true};
        void* host_handle{nullptr};
    };

    class GfxContext {
        GfxDeviceHandle device_{};
        GfxCommandListHandle cmd_{};
        DynamicArray<GfxTextureHandle> swapchain_textures_{};

        Scope<VulkanBackend> vulkan_backend_{nullptr};
        Scope<OpenGLBackend> opengl_backend_{nullptr};
        Scope<Dx12Backend> dx12_backend_{nullptr};

        GLFWwindow* window_handle_{nullptr};

        DynamicArray<VkSemaphore> acquire_semaphores_{};
        DynamicArray<VkSemaphore> present_semaphores_{};
        DynamicArray<VkFence> frame_fences_{};
        Size_t current_frame_slot_{0};
        Size_t active_frame_slot_{(std::numeric_limits<Size_t>::max)()};

    public:
        static Scope<GfxContext> Create(const GfxBackendCreateInfo& create_info);
        static void Destroy(Scope<GfxContext>& backend);

        [[nodiscard]] GfxDeviceHandle getDevice() const { return device_; }
        [[nodiscard]] const DynamicArray<GfxTextureHandle>& getSwapchainTextures() const { return swapchain_textures_; }
        [[nodiscard]] Vector2i getSwapchainExtent2d() const;
        [[nodiscard]] Bool acquireNextSwapchainImage(UInt32& image_index);
        [[nodiscard]] Bool presentSwapchainImage(UInt32 image_index);
        [[nodiscard]] Bool recreateSwapchain();
        [[nodiscard]] const GfxCommandListHandle& getCommandList() { return cmd_; }

        [[nodiscard]] VulkanBackend* getVulkanBackend() const { return vulkan_backend_.get(); }
        [[nodiscard]] OpenGLBackend* getOpenGLBackend() const { return opengl_backend_.get(); }
        [[nodiscard]] Dx12Backend* getDx12Backend() const { return dx12_backend_.get(); }

        void waitForIdle();
        void clearGarbage();

    private:
        void initialize(const GfxBackendCreateInfo& create_info);
        void shutdown();

        void initializeVulkan(const GfxBackendCreateInfo& create_info);
        void initializeOpenGL(const GfxBackendCreateInfo& create_info);
        void initializeDx12(const GfxBackendCreateInfo& create_info);

        void createSwapchainTexturesVulkan();
        void createSwapchainTexturesOpenGL();
        void createSwapchainTexturesDx12();
        void createSwapchainSemaphores();
        void destroySwapchainSemaphores();
    };

} // dodoe
