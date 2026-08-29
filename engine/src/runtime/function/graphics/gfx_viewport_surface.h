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
        GfxDeviceHandle m_device{};
        GLFWwindow* m_window{nullptr};
        void* m_host_handle{nullptr};
        RenderBackendApiType m_api_type{RenderBackendApiType::None};

        GfxBackend* m_backend{nullptr};
        Bool m_owns_swapchain{false};

        DynamicArray<GfxTextureHandle> m_textures{};
        DynamicArray<GfxFramebufferHandle> m_framebuffers{};

        // Vulkan owned state
        VkSurfaceKHR m_vk_surface{VK_NULL_HANDLE};
        VkSwapchainKHR m_vk_swapchain{VK_NULL_HANDLE};
        VkQueue m_vk_present_queue{VK_NULL_HANDLE};
        Bool m_vk_owns_surface{false};
        DynamicArray<VkImage> m_vk_images{};
        DynamicArray<VkImageView> m_vk_imageviews{};
        VkFormat m_vk_format{VK_FORMAT_UNDEFINED};
        VkExtent2D m_vk_extent{};
        DynamicArray<VkSemaphore> m_vk_acquire_semaphores{};
        DynamicArray<VkSemaphore> m_vk_present_semaphores{};
        DynamicArray<VkFence> m_vk_frame_fences{};
        Size_t m_vk_current_frame_slot{0};
        Size_t m_vk_active_frame_slot{(std::numeric_limits<Size_t>::max)()};

        // D3D12 owned state
        ComPtr<IDXGISwapChain4> m_dx_swapchain{};
        DynamicArray<ID3D12Resource*> m_dx_backbuffers{};
        ComPtr<ID3D12DescriptorHeap> m_dx_rtv_heap{};
        UINT m_dx_rtv_descriptor_size{0};
        DXGI_FORMAT m_dx_format{DXGI_FORMAT_R8G8B8A8_UNORM};
        UINT m_dx_width{0};
        UINT m_dx_height{0};
        static constexpr UINT kBackbufferCount = 3;

        ComPtr<ID3D12Fence> m_dx_fence{};
        UINT64 m_dx_fence_value{0};
        HANDLE m_dx_fence_event{nullptr};
        UINT64 m_dx_frame_fence_values[kBackbufferCount]{};

        // OpenGL owned state
        Int32 m_gl_fb_width{0};
        Int32 m_gl_fb_height{0};

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
        [[nodiscard]] const DynamicArray<GfxTextureHandle>& getTextures() const { return m_textures; }
        [[nodiscard]] GfxFramebufferHandle getFramebuffer(UInt32 image_index) const {
            return image_index < m_framebuffers.size() ? m_framebuffers[image_index] : nullptr;
        }
        [[nodiscard]] GLFWwindow* getNativeWindow() const { return m_window; }
        [[nodiscard]] RenderBackendApiType getApiType() const { return m_api_type; }
        [[nodiscard]] Bool isOpenGL() const { return m_api_type == RenderBackendApiType::OpenGL; }

    private:
        [[nodiscard]] VulkanBackend* getVulkanBackend() const { return static_cast<VulkanBackend*>(m_backend); }
        [[nodiscard]] OpenGLBackend* getOpenGLBackend() const { return static_cast<OpenGLBackend*>(m_backend); }
        [[nodiscard]] D3D12Backend* getD3D12Backend() const { return static_cast<D3D12Backend*>(m_backend); }

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
