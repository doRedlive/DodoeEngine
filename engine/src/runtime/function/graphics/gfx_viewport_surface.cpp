// do@Redlive

#include "gfx_viewport_surface.h"

#include "runtime/function/render/render_settings.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

namespace dodoe {

    namespace {

        GfxFormat ToRHIFormatVK(VkFormat format) {
            switch (format) {
                case VK_FORMAT_B8G8R8A8_UNORM: return GfxFormat::BGRA8_UNORM;
                case VK_FORMAT_B8G8R8A8_SRGB: return GfxFormat::SBGRA8_UNORM;
                case VK_FORMAT_R8G8B8A8_UNORM: return GfxFormat::RGBA8_UNORM;
                case VK_FORMAT_R8G8B8A8_SRGB: return GfxFormat::SRGBA8_UNORM;
                default: return GfxFormat::BGRA8_UNORM;
            }
        }

        GfxFormat RHIFormatD3D12(DXGI_FORMAT format) {
            switch (format) {
                case DXGI_FORMAT_R8G8B8A8_UNORM:     return GfxFormat::RGBA8_UNORM;
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return GfxFormat::SRGBA8_UNORM;
                case DXGI_FORMAT_B8G8R8A8_UNORM:     return GfxFormat::BGRA8_UNORM;
                case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return GfxFormat::SBGRA8_UNORM;
                default: return GfxFormat::RGBA8_UNORM;
            }
        }

        UINT GetSwapchainFlags() {
            return (RenderSettings::GetPresentMode() == PresentMode::Immediate) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        }

        struct VulkanSwapchainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities{};
            DynamicArray<VkSurfaceFormatKHR> formats{};
            DynamicArray<VkPresentModeKHR> present_modes{};
        };

        VulkanSwapchainSupportDetails QueryVulkanSwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface) {
            VulkanSwapchainSupportDetails details;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &details.capabilities);

            uint32_t format_count = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, nullptr);
            if (format_count != 0) {
                details.formats.resize(format_count);
                vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, details.formats.data());
            }

            uint32_t presentmode_count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentmode_count, nullptr);
            if (presentmode_count != 0) {
                details.present_modes.resize(presentmode_count);
                vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentmode_count, details.present_modes.data());
            }

            return details;
        }

    } // namespace

    GfxViewportSurface::~GfxViewportSurface() {
        shutdown();
    }

    Bool GfxViewportSurface::initialize(GfxDeviceHandle device, GLFWwindow* window, void* host_handle,
                                        UInt32 w, UInt32 h, RenderBackendApiType api,
                                        GfxBackend* backend, Bool is_primary) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::initialize", "startup");
        m_device = device;
        m_window = window;
        m_host_handle = host_handle;
        m_api_type = api;
        m_backend = backend;

        switch (m_api_type) {
        case RenderBackendApiType::Vulkan:
            return initializeVulkan(device, window, host_handle, w, h, is_primary);
        case RenderBackendApiType::D3D12:
            return initializeD3D12(device, window, host_handle, w, h);
        case RenderBackendApiType::OpenGL:
            return initializeOpenGL(device, window, w, h);
        default:
            DO_ERROR("GfxViewportSurface::initialize: unsupported render backend API ({})", static_cast<int>(m_api_type));
            return false;
        }
    }

    void GfxViewportSurface::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::shutdown", "shutdown");
        if (m_api_type != RenderBackendApiType::None) {
            DO_PROFILE_MARK("GfxViewportSurface::shutdown.releaseResources", "shutdown");
        }
        m_textures.clear();
        m_framebuffers.clear();
        if (m_device) {
            m_device->waitForIdle();
            m_device->runGarbageCollection();
        }

        switch (m_api_type) {
        case RenderBackendApiType::Vulkan:
            destroyVulkanSemaphores();
            destroyVulkanOwnedState();
            break;
        case RenderBackendApiType::D3D12:
            waitD3D12Gpu();
            releaseD3D12Backbuffers();
            m_dx_rtv_heap.Reset();
            if (m_dx_fence_event != nullptr) {
                CloseHandle(m_dx_fence_event);
                m_dx_fence_event = nullptr;
            }
            m_dx_fence.Reset();
            m_dx_swapchain.Reset();
            break;
        case RenderBackendApiType::OpenGL:
        default:
            break;
        }

        m_api_type = RenderBackendApiType::None;
        m_device = nullptr;
        m_window = nullptr;
        m_host_handle = nullptr;
        m_backend = nullptr;
    }

    Vector2i GfxViewportSurface::extent() const {
        switch (m_api_type) {
        case RenderBackendApiType::Vulkan:
            return Vector2i(static_cast<Int32>(m_vk_extent.width), static_cast<Int32>(m_vk_extent.height));
        case RenderBackendApiType::D3D12:
            return Vector2i(static_cast<Int32>(m_dx_width), static_cast<Int32>(m_dx_height));
        case RenderBackendApiType::OpenGL:
            return Vector2i(m_gl_fb_width, m_gl_fb_height);
        default:
            return Vector2i(0, 0);
        }
    }

    Bool GfxViewportSurface::acquire(UInt32& image_index) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::acquire", "swapchain");
        switch (m_api_type) {
        case RenderBackendApiType::OpenGL: {
            image_index = 0;
            updateOpenGLFramebufferSize();
            return m_gl_fb_width > 0 && m_gl_fb_height > 0 && !m_framebuffers.empty();
        }

        case RenderBackendApiType::D3D12: {
            if (!m_dx_swapchain) {
                DO_ERROR("GfxViewportSurface::acquire: D3D12 swapchain is unavailable");
                return false;
            }
            const UINT backbuffer_index = m_dx_swapchain->GetCurrentBackBufferIndex();
            if (m_dx_frame_fence_values[backbuffer_index] > 0 && m_dx_fence) {
                UINT64 completed = m_dx_fence->GetCompletedValue();
                if (completed < m_dx_frame_fence_values[backbuffer_index]) {
                    m_dx_fence->SetEventOnCompletion(m_dx_frame_fence_values[backbuffer_index], m_dx_fence_event);
                    WaitForSingleObject(m_dx_fence_event, INFINITE);
                }
            }
            image_index = static_cast<UInt32>(backbuffer_index);
            return true;
        }

        case RenderBackendApiType::Vulkan: {
            if (!m_device || m_vk_acquire_semaphores.empty() || m_vk_frame_fences.empty() || m_vk_swapchain == VK_NULL_HANDLE) {
                DO_ERROR("GfxViewportSurface::acquire: Vulkan surface is not ready");
                return false;
            }

            auto* vk_device = static_cast<vulkan::IDevice*>(
                m_device->getNativeObject(GfxObjectTypes::Cutie_VK_Device));

            const Size_t frame_slot = m_vk_current_frame_slot % m_vk_acquire_semaphores.size();
            VkDevice vk_device_handle = getVulkanBackend()->getDevice();
            DO_ASSERT(vkWaitForFences(vk_device_handle, 1, &m_vk_frame_fences[frame_slot], VK_TRUE, UINT64_MAX) == VK_SUCCESS,
                "GfxViewportSurface::acquire: failed to wait for frame fence.");

            const VkSemaphore acquire_semaphore = m_vk_acquire_semaphores[frame_slot];
            const VkResult acquire_result = vkAcquireNextImageKHR(vk_device_handle, m_vk_swapchain, UINT64_MAX,
                acquire_semaphore, VK_NULL_HANDLE, &image_index);
            if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
                DO_ERROR("GfxViewportSurface::acquire failed with VkResult={}", static_cast<int>(acquire_result));
                return false;
            }

            if (!vk_device) {
                DO_ERROR("GfxViewportSurface::acquire: vk_device is NULL!");
                return false;
            }
            if (!acquire_semaphore) {
                DO_ERROR("GfxViewportSurface::acquire: acquire_semaphore is NULL!");
                return false;
            }

            vk_device->queueWaitForSemaphore(GfxCommandQueue::Graphics, acquire_semaphore, 0);
            m_vk_active_frame_slot = frame_slot;
            return true;
        }

        default:
            DO_ERROR("GfxViewportSurface::acquire: surface has no active backend");
            return false;
        }
    }

    Bool GfxViewportSurface::present(UInt32 image_index) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::present", "swapchain");
        switch (m_api_type) {
        case RenderBackendApiType::OpenGL: {
            if (!m_window || image_index >= m_framebuffers.size()) {
                DO_ERROR("GfxViewportSurface::present: OpenGL surface is not ready (image={})", image_index);
                return false;
            }
            if (m_gl_fb_width <= 0 || m_gl_fb_height <= 0) return true;
            glfwSwapBuffers(m_window);
            return true;
        }

        case RenderBackendApiType::D3D12: {
            if (!m_dx_swapchain || !getD3D12Backend()->getGraphicsQueue()) {
                DO_ERROR("GfxViewportSurface::present: D3D12 swapchain or queue is unavailable");
                return false;
            }
            if (image_index >= kBackbufferCount || image_index >= m_dx_backbuffers.size()) {
                DO_ERROR("GfxViewportSurface::present: invalid D3D12 backbuffer index {}", image_index);
                return false;
            }

            HRESULT hr = DXGI_ERROR_INVALID_CALL;
            switch (RenderSettings::GetPresentMode()) {
            case PresentMode::VSync:
                hr = m_dx_swapchain->Present(1, 0);
                break;
            case PresentMode::Immediate:
                hr = m_dx_swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
                break;
            case PresentMode::Mailbox:
            default:
                hr = m_dx_swapchain->Present(0, 0);
                break;
            }
            if (hr == DXGI_ERROR_DEVICE_REMOVED) {
                HRESULT device_removed = getD3D12Backend()->getDevice()->GetDeviceRemovedReason();
                DO_ERROR("GfxViewportSurface::present: Device removed! HRESULT={:08X}", static_cast<UINT>(device_removed));
                return false;
            }
            if (FAILED(hr)) {
                DO_ERROR("GfxViewportSurface::present: Present failed with HRESULT={:08X}", static_cast<UINT>(hr));
                return false;
            }

            ++m_dx_fence_value;
            m_dx_frame_fence_values[image_index] = m_dx_fence_value;
            HRESULT signal_hr = getD3D12Backend()->getGraphicsQueue()->Signal(m_dx_fence.Get(), m_dx_fence_value);
            if (FAILED(signal_hr)) {
                DO_ERROR("GfxViewportSurface::present: Signal fence failed with HRESULT={:08X}", static_cast<UINT>(signal_hr));
                return false;
            }

            return true;
        }

        case RenderBackendApiType::Vulkan: {
            if (!m_device || m_vk_swapchain == VK_NULL_HANDLE || m_vk_present_queue == VK_NULL_HANDLE) {
                DO_ERROR("GfxViewportSurface::present: Vulkan surface is not ready");
                return false;
            }

            if (m_vk_active_frame_slot >= m_vk_frame_fences.size() || image_index >= m_vk_present_semaphores.size()) {
                DO_ASSERT(false, "GfxViewportSurface::present: invalid swapchain image index.");
                return false;
            }

            VkSemaphore present_semaphore = m_vk_present_semaphores[image_index];
            VkFence frame_fence = m_vk_frame_fences[m_vk_active_frame_slot];
            VkDevice vk_device_handle = getVulkanBackend()->getDevice();
            DO_ASSERT(vkResetFences(vk_device_handle, 1, &frame_fence) == VK_SUCCESS,
                "GfxViewportSurface::present: failed to reset frame fence.");

            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 0;
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &present_semaphore;
            DO_ASSERT(vkQueueSubmit(getVulkanBackend()->getGraphicsQueue(), 1, &submit_info, frame_fence) == VK_SUCCESS,
                "GfxViewportSurface::present: failed to submit present signal semaphore.");

            VkPresentInfoKHR present_info{};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &present_semaphore;
            present_info.swapchainCount = 1;
            present_info.pSwapchains = &m_vk_swapchain;
            present_info.pImageIndices = &image_index;
            present_info.pResults = nullptr;

            const VkResult present_result = vkQueuePresentKHR(m_vk_present_queue, &present_info);
            m_vk_current_frame_slot = (m_vk_active_frame_slot + 1) % m_vk_acquire_semaphores.size();
            m_vk_active_frame_slot = (std::numeric_limits<Size_t>::max)();
            if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR) {
                DO_ERROR("GfxViewportSurface::present: vkQueuePresentKHR failed with VkResult={}", static_cast<int>(present_result));
                return false;
            }
            if (present_result == VK_SUBOPTIMAL_KHR) {
                DO_WARN("GfxViewportSurface::present: Vulkan swapchain is suboptimal");
            }
            return true;
        }

        default:
            DO_ERROR("GfxViewportSurface::present: surface has no active backend");
            return false;
        }
    }

    Bool GfxViewportSurface::resize(UInt32 w, UInt32 h) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::resize", "swapchain");
        DO_PROFILE_MARK("GfxViewportSurface::resize.releaseResources", "swapchain");
        switch (m_api_type) {
        case RenderBackendApiType::OpenGL: {
            updateOpenGLFramebufferSize();
            m_textures.clear();
            m_framebuffers.clear();
            createSwapchainTexturesOpenGL();
            return !m_framebuffers.empty();
        }

        case RenderBackendApiType::D3D12: {
            if (w == 0 || h == 0) {
                return false;
            }

            if (m_device) {
                m_device->waitForIdle();
            }

            m_textures.clear();
            m_framebuffers.clear();

            if (m_device) {
                m_device->runGarbageCollection();
                m_device->waitForIdle();
            }

            waitD3D12Gpu();
            releaseD3D12Backbuffers();

            m_dx_width = w;
            m_dx_height = h;

            HRESULT hr = m_dx_swapchain->ResizeBuffers(kBackbufferCount, m_dx_width, m_dx_height, m_dx_format, GetSwapchainFlags());
            if (FAILED(hr)) {
                DO_ERROR("GfxViewportSurface::resize: ResizeBuffers failed with HRESULT=0x{:08X}", static_cast<UINT>(hr));
            }
            DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::resize: ResizeBuffers failed");

            createD3D12BackbufferRTVs();
            createSwapchainTexturesD3D12();
            if (m_device) {
                m_device->runGarbageCollection();
            }
            return !m_framebuffers.empty();
        }

        case RenderBackendApiType::Vulkan: {
            if (w == 0 || h == 0) {
                return false;
            }

            if (m_device) {
                m_device->waitForIdle();
            }

            m_textures.clear();
            m_framebuffers.clear();
            destroyVulkanSemaphores();
            if (m_device) {
                m_device->runGarbageCollection();
                m_device->waitForIdle();
            }

            if (m_owns_swapchain) {
                VkDevice vk_device_handle = getVulkanBackend()->getDevice();
                for (auto image_view : m_vk_imageviews) {
                    if (image_view != VK_NULL_HANDLE) {
                        vkDestroyImageView(vk_device_handle, image_view, nullptr);
                    }
                }
                m_vk_imageviews.clear();
                if (m_vk_swapchain != VK_NULL_HANDLE) {
                    vkDestroySwapchainKHR(vk_device_handle, m_vk_swapchain, nullptr);
                    m_vk_swapchain = VK_NULL_HANDLE;
                }
                m_vk_images.clear();

                createVulkanSwapchain(w, h);
                createVulkanImageViews();
            } else {
                if (!getVulkanBackend()->recreateSwapchain(m_window, w, h)) {
                    return false;
                }
                m_vk_swapchain = getVulkanBackend()->getSwapchain();
                const auto& backend_images = getVulkanBackend()->getSwapchainImages();
                m_vk_images.assign(backend_images.begin(), backend_images.end());
                const auto& backend_imageviews = getVulkanBackend()->getSwapchainImageViews();
                m_vk_imageviews.assign(backend_imageviews.begin(), backend_imageviews.end());
                m_vk_format = getVulkanBackend()->getSwapchainImageFormat();
                const auto extent = getVulkanBackend()->getSwapchainExtent2D();
                m_vk_extent = { static_cast<uint32_t>(extent.x), static_cast<uint32_t>(extent.y) };
            }

            createVulkanSemaphores();
            createSwapchainTexturesVulkan();
            if (m_device) {
                m_device->runGarbageCollection();
            }
            return !m_framebuffers.empty();
        }

        default:
            DO_ERROR("GfxViewportSurface::resize: surface has no active backend");
            return false;
        }
    }

    Bool GfxViewportSurface::initializeVulkan(GfxDeviceHandle device, GLFWwindow* window, void* host_handle,
                                              UInt32 w, UInt32 h, Bool is_primary) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::initializeVulkan", "startup");
        if (!device || !getVulkanBackend()) {
            DO_ERROR("GfxViewportSurface::initializeVulkan: device or backend is unavailable");
            return false;
        }

        if (is_primary) {
            m_vk_swapchain = getVulkanBackend()->getSwapchain();
            m_vk_present_queue = getVulkanBackend()->getPresentQueue();
            const auto& backend_images = getVulkanBackend()->getSwapchainImages();
            m_vk_images.assign(backend_images.begin(), backend_images.end());
            const auto& backend_imageviews = getVulkanBackend()->getSwapchainImageViews();
            m_vk_imageviews.assign(backend_imageviews.begin(), backend_imageviews.end());
            m_vk_format = getVulkanBackend()->getSwapchainImageFormat();
            const auto extent = getVulkanBackend()->getSwapchainExtent2D();
            m_vk_extent = { static_cast<uint32_t>(extent.x), static_cast<uint32_t>(extent.y) };
            m_owns_swapchain = false;
            m_vk_owns_surface = false;
        } else {
            if (!createVulkanSurface(window, host_handle)) {
                return false;
            }
            createVulkanSwapchain(w, h);
            createVulkanImageViews();
            m_vk_present_queue = getVulkanBackend()->getPresentQueue();
            m_owns_swapchain = true;
            m_vk_owns_surface = true;
        }

        createVulkanSemaphores();
        createSwapchainTexturesVulkan();
        DO_INFO("GfxViewportSurface: Vulkan {} surface initialized ({} images, {}x{})",
            is_primary ? "primary" : "secondary", m_vk_images.size(), m_vk_extent.width, m_vk_extent.height);
        return !m_textures.empty();
    }

    Bool GfxViewportSurface::initializeD3D12(GfxDeviceHandle device, GLFWwindow* window, void* host_handle, UInt32 w, UInt32 h) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::initializeD3D12", "startup");
        if (!device || !getD3D12Backend()) {
            DO_ERROR("GfxViewportSurface::initializeD3D12: device or backend is unavailable");
            return false;
        }

        m_owns_swapchain = true;
        createD3D12Swapchain(w, h);
        createD3D12RTVHeap();
        createD3D12BackbufferRTVs();
        createD3D12Fence();
        createSwapchainTexturesD3D12();
        DO_INFO("GfxViewportSurface: D3D12 surface initialized ({} images, {}x{})",
            m_dx_backbuffers.size(), m_dx_width, m_dx_height);
        return !m_textures.empty();
    }

    Bool GfxViewportSurface::initializeOpenGL(GfxDeviceHandle device, GLFWwindow* window, UInt32 w, UInt32 h) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::initializeOpenGL", "startup");
        if (!device || !getOpenGLBackend()) {
            DO_ERROR("GfxViewportSurface::initializeOpenGL: device or backend is unavailable");
            return false;
        }

        m_owns_swapchain = false;
        updateOpenGLFramebufferSize();
        createSwapchainTexturesOpenGL();
        DO_INFO("GfxViewportSurface: OpenGL surface initialized ({}x{})", m_gl_fb_width, m_gl_fb_height);
        return !m_framebuffers.empty();
    }

    Bool GfxViewportSurface::createVulkanSurface(GLFWwindow* window, void* host_handle) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createVulkanSurface", "startup");
        if (host_handle != nullptr) {
#if defined(DO_PLATFORM_WINDOWS)
            VkWin32SurfaceCreateInfoKHR surface_info{};
            surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surface_info.hwnd = static_cast<HWND>(host_handle);
            surface_info.hinstance = GetModuleHandle(nullptr);
            const VkResult result = vkCreateWin32SurfaceKHR(getVulkanBackend()->getInstance(), &surface_info, nullptr, &m_vk_surface);
            DO_ASSERT(result == VK_SUCCESS, "GfxViewportSurface::createVulkanSurface(host) failed");
            return result == VK_SUCCESS;
#else
            DO_ASSERT(false, "GfxViewportSurface::createVulkanSurface: host unsupported platform");
            return false;
#endif
        }
        if (window != nullptr) {
            const VkResult result = glfwCreateWindowSurface(getVulkanBackend()->getInstance(), window, nullptr, &m_vk_surface);
            DO_ASSERT(result == VK_SUCCESS, "GfxViewportSurface::createVulkanSurface GLFW failed");
            return result == VK_SUCCESS;
        }
        DO_ASSERT(false, "GfxViewportSurface::createVulkanSurface: no handle");
        return false;
    }

    void GfxViewportSurface::createVulkanSwapchain(UInt32 w, UInt32 h) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createVulkanSwapchain", "swapchain");
        const auto swapchain_details = QueryVulkanSwapchainSupport(getVulkanBackend()->getPhysicalDevice(), m_vk_surface);

        VkSurfaceFormatKHR chosen_surface_format{};
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
            if (w > 0 && h > 0) {
                sel_w = static_cast<int>(w);
                sel_h = static_cast<int>(h);
            } else if (swapchain_details.capabilities.currentExtent.width != UINT32_MAX
                && swapchain_details.capabilities.currentExtent.width > 0
                && swapchain_details.capabilities.currentExtent.height > 0) {
                sel_w = static_cast<int>(swapchain_details.capabilities.currentExtent.width);
                sel_h = static_cast<int>(swapchain_details.capabilities.currentExtent.height);
            } else if (m_host_handle != nullptr) {
                RECT rect;
                GetClientRect(static_cast<HWND>(m_host_handle), &rect);
                sel_w = rect.right - rect.left;
                sel_h = rect.bottom - rect.top;
            } else {
                glfwGetFramebufferSize(m_window, &sel_w, &sel_h);
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
        swapchain_info.surface = m_vk_surface;
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

        const uint32_t graphics_family = getVulkanBackend()->getGraphicsQueueIndex();
        const int present_family = getVulkanBackend()->getPresentQueueIndex();
        if (present_family >= 0 && static_cast<uint32_t>(present_family) != graphics_family) {
            uint32_t queue_family_indices[] = { graphics_family, static_cast<uint32_t>(present_family) };
            swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_info.queueFamilyIndexCount = 2;
            swapchain_info.pQueueFamilyIndices = queue_family_indices;
        } else {
            swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchain_info.queueFamilyIndexCount = 0;
            swapchain_info.pQueueFamilyIndices = nullptr;
        }

        swapchain_info.preTransform = swapchain_details.capabilities.currentTransform;
        swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.presentMode = chosen_present_mode;
        swapchain_info.clipped = VK_TRUE;
        swapchain_info.oldSwapchain = VK_NULL_HANDLE;

        const VkResult result = vkCreateSwapchainKHR(getVulkanBackend()->getDevice(), &swapchain_info, nullptr, &m_vk_swapchain);
        DO_ASSERT(result == VK_SUCCESS, "GfxViewportSurface::createVulkanSwapchain failed with VkResult={}", static_cast<int>(result));

        vkGetSwapchainImagesKHR(getVulkanBackend()->getDevice(), m_vk_swapchain, &image_count, nullptr);
        m_vk_images.resize(image_count);
        vkGetSwapchainImagesKHR(getVulkanBackend()->getDevice(), m_vk_swapchain, &image_count, m_vk_images.data());

        m_vk_format = chosen_surface_format.format;
        m_vk_extent = chosen_extent;
        DO_INFO("GfxViewportSurface: Vulkan swapchain created ({} images, {}x{})",
            m_vk_images.size(), m_vk_extent.width, m_vk_extent.height);
    }

    void GfxViewportSurface::createVulkanImageViews() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createVulkanImageViews", "swapchain");
        m_vk_imageviews.clear();
        m_vk_imageviews.reserve(m_vk_images.size());
        for (const auto swapchain_image : m_vk_images) {
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = swapchain_image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = m_vk_format;
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
            DO_ASSERT(vkCreateImageView(getVulkanBackend()->getDevice(), &view_info, nullptr, &image_view) == VK_SUCCESS,
                "GfxViewportSurface::createVulkanImageViews failed to create swapchain image view.");
            m_vk_imageviews.push_back(image_view);
        }
    }

    void GfxViewportSurface::createVulkanSemaphores() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createVulkanSemaphores", "swapchain");
        if (!getVulkanBackend()) {
            DO_ERROR("GfxViewportSurface::createVulkanSemaphores: backend is unavailable");
            return;
        }

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkDevice vk_device = getVulkanBackend()->getDevice();
        m_vk_acquire_semaphores.clear();
        m_vk_present_semaphores.clear();
        m_vk_frame_fences.clear();
        const Size_t frame_count = m_vk_images.size();
        m_vk_acquire_semaphores.reserve(frame_count);
        m_vk_present_semaphores.reserve(frame_count);
        m_vk_frame_fences.reserve(frame_count);

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (Size_t i = 0; i < frame_count; ++i) {
            VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &acquire_semaphore) == VK_SUCCESS,
                "GfxViewportSurface::createVulkanSemaphores: failed to create acquire semaphore.");
            m_vk_acquire_semaphores.push_back(acquire_semaphore);

            VkSemaphore present_semaphore = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &present_semaphore) == VK_SUCCESS,
                "GfxViewportSurface::createVulkanSemaphores: failed to create present semaphore.");
            m_vk_present_semaphores.push_back(present_semaphore);

            VkFence frame_fence = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateFence(vk_device, &fence_info, nullptr, &frame_fence) == VK_SUCCESS,
                "GfxViewportSurface::createVulkanSemaphores: failed to create frame fence.");
            m_vk_frame_fences.push_back(frame_fence);
        }

        m_vk_current_frame_slot = 0;
        m_vk_active_frame_slot = (std::numeric_limits<Size_t>::max)();
    }

    void GfxViewportSurface::destroyVulkanSemaphores() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::destroyVulkanSemaphores", "shutdown");
        if (!getVulkanBackend()) {
            m_vk_acquire_semaphores.clear();
            m_vk_present_semaphores.clear();
            m_vk_frame_fences.clear();
            m_vk_current_frame_slot = 0;
            m_vk_active_frame_slot = (std::numeric_limits<Size_t>::max)();
            return;
        }

        VkDevice vk_device = getVulkanBackend()->getDevice();
        if (vk_device != VK_NULL_HANDLE) {
            for (auto semaphore : m_vk_acquire_semaphores) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(vk_device, semaphore, nullptr);
                }
            }
            for (auto semaphore : m_vk_present_semaphores) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(vk_device, semaphore, nullptr);
                }
            }
            for (auto fence : m_vk_frame_fences) {
                if (fence != VK_NULL_HANDLE) {
                    vkDestroyFence(vk_device, fence, nullptr);
                }
            }
        }
        m_vk_acquire_semaphores.clear();
        m_vk_present_semaphores.clear();
        m_vk_frame_fences.clear();
        m_vk_current_frame_slot = 0;
        m_vk_active_frame_slot = (std::numeric_limits<Size_t>::max)();
    }

    void GfxViewportSurface::destroyVulkanOwnedState() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::destroyVulkanOwnedState", "shutdown");
        if (!getVulkanBackend()) {
            return;
        }

        VkDevice vk_device = getVulkanBackend()->getDevice();
        if (vk_device == VK_NULL_HANDLE) {
            return;
        }

        if (m_owns_swapchain) {
            for (auto image_view : m_vk_imageviews) {
                if (image_view != VK_NULL_HANDLE) {
                    vkDestroyImageView(vk_device, image_view, nullptr);
                }
            }
            m_vk_imageviews.clear();
            if (m_vk_swapchain != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(vk_device, m_vk_swapchain, nullptr);
                m_vk_swapchain = VK_NULL_HANDLE;
            }
        }
        m_vk_images.clear();

        if (m_vk_owns_surface && m_vk_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(getVulkanBackend()->getInstance(), m_vk_surface, nullptr);
            m_vk_surface = VK_NULL_HANDLE;
        }
    }

    void GfxViewportSurface::createSwapchainTexturesVulkan() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createSwapchainTexturesVulkan", "resource");
        if (!m_device || !getVulkanBackend()) {
            DO_ERROR("GfxViewportSurface::createSwapchainTexturesVulkan: device or backend is unavailable");
            return;
        }

        m_textures.clear();
        m_framebuffers.clear();
        const auto swapchain_format = ToRHIFormatVK(m_vk_format);

        for (const auto& image : m_vk_images) {
            auto texture_desc = GfxTextureDesc()
                .setDimension(GfxTextureDimension::Texture2D)
                .setFormat(swapchain_format)
                .setWidth(m_vk_extent.width)
                .setHeight(m_vk_extent.height)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(GfxResourceStates::Present)
                .setDebugName("Swapchain Image");

            auto tex = create_ref<GfxTexture>(m_device->createHandleForNativeTexture(GfxObjectTypes::VK_Image, image, texture_desc), texture_desc, "Swapchain Image");
            m_textures.push_back(tex);
            GfxFramebufferDesc framebuffer_desc{};
            framebuffer_desc.addColorAttachment(tex);
            auto framebuffer = create_ref<GfxFramebuffer>(framebuffer_desc);
            framebuffer->initializeGpu(m_device);
            m_framebuffers.push_back(framebuffer);
        }
    }

    void GfxViewportSurface::updateOpenGLFramebufferSize() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::updateOpenGLFramebufferSize", "swapchain");
        if (m_window) {
            glfwGetFramebufferSize(m_window, &m_gl_fb_width, &m_gl_fb_height);
        } else {
            m_gl_fb_width = 0;
            m_gl_fb_height = 0;
        }
    }

    void GfxViewportSurface::createSwapchainTexturesOpenGL() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createSwapchainTexturesOpenGL", "resource");
        if (!m_device || !getOpenGLBackend()) {
            DO_ERROR("GfxViewportSurface::createSwapchainTexturesOpenGL: device or backend is unavailable");
            return;
        }

        m_textures.clear();
        m_framebuffers.clear();
        if (m_gl_fb_width <= 0 || m_gl_fb_height <= 0) {
            return;
        }

        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info.addColorFormat(GfxFormat::RGBA8_UNORM);
        const auto framebuffer = opengl::createDefaultFramebuffer(m_device);
        DO_ASSERT(framebuffer != nullptr, "GfxViewportSurface::createSwapchainTexturesOpenGL: failed to create default framebuffer.");
        m_framebuffers.push_back(create_ref<GfxFramebuffer>(framebuffer, framebuffer_info));
    }

    void GfxViewportSurface::createSwapchainTexturesD3D12() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createSwapchainTexturesD3D12", "resource");
        if (!m_device || !getD3D12Backend()) {
            DO_ERROR("GfxViewportSurface::createSwapchainTexturesD3D12: device or backend is unavailable");
            return;
        }

        m_textures.clear();
        m_framebuffers.clear();
        const auto swapchain_format = RHIFormatD3D12(m_dx_format);

        for (auto* backbuffer : m_dx_backbuffers) {
            auto texture_desc = GfxTextureDesc()
                .setDimension(GfxTextureDimension::Texture2D)
                .setFormat(swapchain_format)
                .setWidth(m_dx_width)
                .setHeight(m_dx_height)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(GfxResourceStates::Present)
                .setDebugName("Swapchain Image");

            auto tex = create_ref<GfxTexture>(m_device->createHandleForNativeTexture(GfxObjectTypes::D3D12_Resource, static_cast<cutie::Object>(backbuffer), texture_desc), texture_desc, "Swapchain Image");
            m_textures.push_back(tex);
            GfxFramebufferDesc framebuffer_desc{};
            framebuffer_desc.addColorAttachment(tex);
            auto framebuffer = create_ref<GfxFramebuffer>(framebuffer_desc);
            framebuffer->initializeGpu(m_device);
            m_framebuffers.push_back(framebuffer);
        }
    }

    void GfxViewportSurface::createD3D12Swapchain(UInt32 w, UInt32 h) {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createD3D12Swapchain", "swapchain");
        HWND hwnd = m_host_handle != nullptr ? static_cast<HWND>(m_host_handle) : nullptr;
        if (hwnd == nullptr && m_window != nullptr) {
            hwnd = glfwGetWin32Window(m_window);
        }
        DO_ASSERT(hwnd != nullptr, "GfxViewportSurface::createD3D12Swapchain: no window handle.");

        UInt32 swapchain_w = w;
        UInt32 swapchain_h = h;
        if (m_host_handle != nullptr || swapchain_w == 0 || swapchain_h == 0) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            if (rect.right > rect.left && rect.bottom > rect.top) {
                swapchain_w = static_cast<UInt32>(rect.right - rect.left);
                swapchain_h = static_cast<UInt32>(rect.bottom - rect.top);
            }
        }
        m_dx_width = (swapchain_w == 0) ? 1 : swapchain_w;
        m_dx_height = (swapchain_h == 0) ? 1 : swapchain_h;

        DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
        swapchain_desc.Width = m_dx_width;
        swapchain_desc.Height = m_dx_height;
        swapchain_desc.Format = m_dx_format;
        swapchain_desc.Stereo = FALSE;
        swapchain_desc.SampleDesc.Count = 1;
        swapchain_desc.SampleDesc.Quality = 0;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.BufferCount = kBackbufferCount;
        swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapchain_desc.Flags = GetSwapchainFlags();

        ComPtr<IDXGISwapChain1> swapchain1;
        HRESULT hr = getD3D12Backend()->getFactory()->CreateSwapChainForHwnd(
            getD3D12Backend()->getGraphicsQueue(),
            hwnd,
            &swapchain_desc,
            nullptr,
            nullptr,
            &swapchain1);
        DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12Swapchain: CreateSwapChainForHwnd failed with HRESULT={:08X}", static_cast<UINT>(hr));

        hr = swapchain1.As(&m_dx_swapchain);
        DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12Swapchain: QueryInterface IDXGISwapChain4 failed with HRESULT={:08X}", static_cast<UINT>(hr));

        getD3D12Backend()->getFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        { char buf[128]; snprintf(buf, sizeof(buf), "[D3D12] swapchain created: %ux%u, format=%u\n", m_dx_width, m_dx_height, static_cast<UINT>(m_dx_format)); OutputDebugStringA(buf); }
    }

    void GfxViewportSurface::createD3D12RTVHeap() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createD3D12RTVHeap", "resource");
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = kBackbufferCount;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heap_desc.NodeMask = 0;

        HRESULT hr = getD3D12Backend()->getDevice()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_dx_rtv_heap));
        DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12RTVHeap failed with HRESULT={:08X}", static_cast<UINT>(hr));

        m_dx_rtv_descriptor_size = getD3D12Backend()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    void GfxViewportSurface::createD3D12BackbufferRTVs() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createD3D12BackbufferRTVs", "resource");
        releaseD3D12Backbuffers();

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_dx_rtv_heap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < kBackbufferCount; ++i) {
            ID3D12Resource* backbuffer = nullptr;
            HRESULT hr = m_dx_swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffer));
            DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12BackbufferRTVs: GetBuffer({}) failed with HRESULT={:08X}", i, static_cast<UINT>(hr));

            getD3D12Backend()->getDevice()->CreateRenderTargetView(backbuffer, nullptr, rtv_handle);
            m_dx_backbuffers.push_back(backbuffer);

            if (getD3D12Backend()->isValidationEnabled()) {
                wchar_t name[64];
                swprintf(name, 64, L"Backbuffer %u", i);
                backbuffer->SetName(name);
            }

            rtv_handle.ptr += m_dx_rtv_descriptor_size;
        }
    }

    void GfxViewportSurface::createD3D12Fence() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::createD3D12Fence", "synchronization");
        HRESULT hr = getD3D12Backend()->getDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_dx_fence));
        DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12Fence failed with HRESULT={:08X}", static_cast<UINT>(hr));

        m_dx_fence_value = 0;
        m_dx_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        DO_ASSERT(m_dx_fence_event != nullptr, "GfxViewportSurface::createD3D12Fence: CreateEventW failed.");

        for (UINT i = 0; i < kBackbufferCount; ++i) {
            m_dx_frame_fence_values[i] = 0;
        }
    }

    void GfxViewportSurface::releaseD3D12Backbuffers() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::releaseD3D12Backbuffers", "shutdown");
        for (auto& bb : m_dx_backbuffers) {
            if (bb) {
                bb->Release();
            }
        }
        m_dx_backbuffers.clear();
    }

    void GfxViewportSurface::waitD3D12Gpu() {
        DO_PROFILE_SCOPE_CATEGORY("GfxViewportSurface::waitD3D12Gpu", "synchronization");
        if (getD3D12Backend()->getGraphicsQueue() && m_dx_fence) {
            ++m_dx_fence_value;
            HRESULT hr = getD3D12Backend()->getGraphicsQueue()->Signal(m_dx_fence.Get(), m_dx_fence_value);
            if (SUCCEEDED(hr)) {
                m_dx_fence->SetEventOnCompletion(m_dx_fence_value, m_dx_fence_event);
                WaitForSingleObject(m_dx_fence_event, INFINITE);
            }
        }
    }

} // dodoe
