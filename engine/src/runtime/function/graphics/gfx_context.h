// do@Redlive
#pragma once

#include "dopch.h"

#include "backend/vulkan_backend.h"
#include "backend/opengl_backend.h"
#include "backend/d3d12_backend.h"

#include "gfx.h"
#include "draw_command_list.h"
#include "runtime/function/render/render_settings.h"

struct GLFWwindow;

namespace dodoe {

    enum class RenderBackendApiType;

    struct GfxBackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
        RenderBackendApiType api_type{};
        Bool enable_validation{true};
        RenderFeatureSettings feature_settings{};
        void* host_handle{nullptr};
        UInt32 width{0};
        UInt32 height{0};
    };

    class GfxContext {
        GfxDeviceHandle device_{};
        GfxCommandListHandle cmd_{};
        DynamicArray<GfxTextureHandle> swapchain_textures_{};
        DynamicArray<GfxFramebufferHandle> swapchain_framebuffers_{};

        Scope<VulkanBackend> vulkan_backend_{nullptr};
        Scope<OpenGLBackend> opengl_backend_{nullptr};
        Scope<D3D12Backend> d3d12_backend_{nullptr};
        GLFWwindow* window_handle_{nullptr};
        Bool m_gpu_driven_supported{false};

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
        [[nodiscard]] GfxFramebufferHandle getSwapchainFramebuffer(UInt32 image_index) const {
            return image_index < swapchain_framebuffers_.size() ? swapchain_framebuffers_[image_index] : nullptr;
        }
        [[nodiscard]] Vector2i getSwapchainExtent2d() const;
        [[nodiscard]] Bool acquireNextSwapchainImage(UInt32& image_index);
        [[nodiscard]] Bool presentSwapchainImage(UInt32 image_index);
        [[nodiscard]] Bool recreateSwapchain(UInt32 width, UInt32 height);
        [[nodiscard]] Bool acquireOpenGLContext();
        void releaseOpenGLContext();
        [[nodiscard]] const GfxCommandListHandle& getCommandList() { return cmd_; }

        [[nodiscard]] Bool isGpuDrivenSupported() const { return m_gpu_driven_supported; }
        [[nodiscard]] VulkanBackend* getVulkanBackend() const { return vulkan_backend_.get(); }
        [[nodiscard]] OpenGLBackend* getOpenGLBackend() const { return opengl_backend_.get(); }
        [[nodiscard]] D3D12Backend* getD3D12Backend() const { return d3d12_backend_.get(); }

        void waitForIdle();
        void clearGarbage();

    private:
        friend class DrawCommandList;

        void initialize(const GfxBackendCreateInfo& create_info);
        void shutdown();

        void initializeVulkan(const GfxBackendCreateInfo& create_info);
        void initializeOpenGL(const GfxBackendCreateInfo& create_info);
        void initializeD3D12(const GfxBackendCreateInfo& create_info);

        void createSwapchainTexturesVulkan();
        void createSwapchainTexturesOpenGL();
        void createSwapchainTexturesD3D12();
        void createSwapchainSemaphores();
        void destroySwapchainSemaphores();
    };

} // dodoe
