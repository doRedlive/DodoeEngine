// do@Redlive

#pragma once

#include "dopch.h"

#include "backend/gfx_backend.h"
#include "backend/vulkan_backend.h"
#include "backend/opengl_backend.h"
#include "backend/d3d12_backend.h"

#include "gfx.h"
#include "draw_command_list.h"
#include "runtime/function/render/render_settings.h"

struct GLFWwindow;

namespace dodoe {

    class GfxViewportSurface {
        GfxDeviceHandle device_{};
        GLFWwindow* window_{nullptr};
        void* host_handle_{nullptr};
        RenderBackendApiType api_type_{RenderBackendApiType::None};

        GfxBackend* backend_{nullptr};
        Bool m_owns_swapchain{false};

        DynamicArray<GfxTextureHandle> textures_{};
        DynamicArray<GfxFramebufferHandle> framebuffers_{};

        // Vulkan owned state
        VkSurfaceKHR vk_surface_{VK_NULL_HANDLE};
        VkSwapchainKHR vk_swapchain_{VK_NULL_HANDLE};
        VkQueue vk_present_queue_{VK_NULL_HANDLE};
        Bool vk_owns_surface_{false};
        std::vector<VkImage> vk_images_{};
        std::vector<VkImageView> vk_imageviews_{};
        VkFormat vk_format_{VK_FORMAT_UNDEFINED};
        VkExtent2D vk_extent_{};
        DynamicArray<VkSemaphore> vk_acquire_semaphores_{};
        DynamicArray<VkSemaphore> vk_present_semaphores_{};
        DynamicArray<VkFence> vk_frame_fences_{};
        Size_t vk_current_frame_slot_{0};
        Size_t vk_active_frame_slot_{(std::numeric_limits<Size_t>::max)()};

        // D3D12 owned state
        ComPtr<IDXGISwapChain4> dx_swapchain_{};
        std::vector<ID3D12Resource*> dx_backbuffers_{};
        ComPtr<ID3D12DescriptorHeap> dx_rtv_heap_{};
        UINT dx_rtv_descriptor_size_{0};
        DXGI_FORMAT dx_format_{DXGI_FORMAT_R8G8B8A8_UNORM};
        UINT dx_width_{0};
        UINT dx_height_{0};
        Int32 m_gl_fb_width{0};
        Int32 m_gl_fb_height{0};
        static constexpr UINT kBackbufferCount = 3;

        ComPtr<ID3D12Fence> dx_fence_{};
        UINT64 dx_fence_value_{0};
        HANDLE dx_fence_event_{nullptr};
        UINT64 dx_frame_fence_values_[kBackbufferCount]{};

    public:

        ~GfxViewportSurface();

        Bool initialize(GfxDeviceHandle device, GLFWwindow* window, void* host_handle,
                        UInt32 w, UInt32 h, RenderBackendApiType api,
                        GfxBackend* backend, Bool is_primary);
        void shutdown();
        Bool resize(UInt32 w, UInt32 h);
        Bool acquire(UInt32& image_index);
        Bool present(UInt32 image_index);
        [[nodiscard]] Vector2i extent() const;
        [[nodiscard]] const DynamicArray<GfxTextureHandle>& getTextures() const { return textures_; }
        [[nodiscard]] GfxFramebufferHandle getFramebuffer(UInt32 image_index) const {
            return image_index < framebuffers_.size() ? framebuffers_[image_index] : nullptr;
        }
        [[nodiscard]] GLFWwindow* getNativeWindow() const { return window_; }
        [[nodiscard]] RenderBackendApiType getApiType() const { return api_type_; }
        [[nodiscard]] Bool isOpenGL() const { return api_type_ == RenderBackendApiType::OpenGL; }

    private:
        [[nodiscard]] VulkanBackend* getVulkanBackend() const { return static_cast<VulkanBackend*>(backend_); }
        [[nodiscard]] OpenGLBackend* getOpenGLBackend() const { return static_cast<OpenGLBackend*>(backend_); }
        [[nodiscard]] D3D12Backend* getD3D12Backend() const { return static_cast<D3D12Backend*>(backend_); }

        Bool initializeVulkan(GfxDeviceHandle device, GLFWwindow* window, void* host_handle,
                              UInt32 w, UInt32 h, Bool is_primary);
        Bool initializeD3D12(GfxDeviceHandle device, GLFWwindow* window, void* host_handle, UInt32 w, UInt32 h);
        Bool initializeOpenGL(GfxDeviceHandle device, GLFWwindow* window, UInt32 w, UInt32 h);

        void createSwapchainTexturesVulkan();
        void createSwapchainTexturesD3D12();
        void createSwapchainTexturesOpenGL();

        Bool createVulkanSurface(GLFWwindow* window, void* host_handle);
        void createVulkanSwapchain(UInt32 w, UInt32 h);
        void createVulkanImageViews();
        void createVulkanSemaphores();
        void destroyVulkanSemaphores();
        void destroyVulkanOwnedState();

        void createD3D12Swapchain(UInt32 w, UInt32 h);
        void createD3D12RTVHeap();
        void createD3D12BackbufferRTVs();
        void createD3D12Fence();
        void releaseD3D12Backbuffers();
        void waitD3D12Gpu();
        void updateOpenGLFramebufferSize();
    };

} // dodoe
