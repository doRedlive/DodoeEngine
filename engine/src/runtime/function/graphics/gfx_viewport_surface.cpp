// do@Redlive

#include "gfx_viewport_surface.h"

#include "../render/render_settings.h"

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
            std::vector<VkSurfaceFormatKHR> formats{};
            std::vector<VkPresentModeKHR> present_modes{};
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

    }

    GfxViewportSurface::~GfxViewportSurface() {
        shutdown();
    }

    Bool GfxViewportSurface::initialize(GfxDeviceHandle device, GLFWwindow* window, void* host_handle,
                                        UInt32 w, UInt32 h, RenderBackendApiType api,
                                        GfxBackend* backend, Bool is_primary) {
        device_ = device;
        window_ = window;
        host_handle_ = host_handle;
        api_type_ = api;
        backend_ = backend;

        switch (api_type_) {
        case RenderBackendApiType::Vulkan:
            return initializeVulkan(device, window, host_handle, w, h, is_primary);
        case RenderBackendApiType::D3D12:
            return initializeD3D12(device, window, host_handle, w, h);
        case RenderBackendApiType::OpenGL:
            return initializeOpenGL(device, window, w, h);
        default:
            return false;
        }
    }

    void GfxViewportSurface::shutdown() {
        textures_.clear();
        framebuffers_.clear();
        if (device_) {
            device_->waitForIdle();
            device_->runGarbageCollection();
        }

        switch (api_type_) {
        case RenderBackendApiType::Vulkan:
            destroyVulkanSemaphores();
            destroyVulkanOwnedState();
            break;
        case RenderBackendApiType::D3D12:
            waitD3D12Gpu();
            releaseD3D12Backbuffers();
            dx_rtv_heap_.Reset();
            if (dx_fence_event_ != nullptr) {
                CloseHandle(dx_fence_event_);
                dx_fence_event_ = nullptr;
            }
            dx_fence_.Reset();
            dx_swapchain_.Reset();
            break;
        case RenderBackendApiType::OpenGL:
        default:
            break;
        }

        api_type_ = RenderBackendApiType::None;
        device_ = nullptr;
        window_ = nullptr;
        host_handle_ = nullptr;
        backend_ = nullptr;
    }

    Vector2i GfxViewportSurface::extent() const {
        switch (api_type_) {
        case RenderBackendApiType::Vulkan:
            return Vector2i(static_cast<Int32>(vk_extent_.width), static_cast<Int32>(vk_extent_.height));
        case RenderBackendApiType::D3D12:
            return Vector2i(static_cast<Int32>(dx_width_), static_cast<Int32>(dx_height_));
        case RenderBackendApiType::OpenGL:
            return Vector2i(m_gl_fb_width, m_gl_fb_height);
        default:
            return Vector2i(0, 0);
        }
    }

    Bool GfxViewportSurface::acquire(UInt32& image_index) {
        switch (api_type_) {
        case RenderBackendApiType::OpenGL: {
            image_index = 0;
            updateOpenGLFramebufferSize();
            return m_gl_fb_width > 0 && m_gl_fb_height > 0 && !framebuffers_.empty();
        }

        case RenderBackendApiType::D3D12: {
            if (!dx_swapchain_) {
                return false;
            }
            const UINT backbuffer_index = dx_swapchain_->GetCurrentBackBufferIndex();
            if (dx_frame_fence_values_[backbuffer_index] > 0 && dx_fence_) {
                UINT64 completed = dx_fence_->GetCompletedValue();
                if (completed < dx_frame_fence_values_[backbuffer_index]) {
                    dx_fence_->SetEventOnCompletion(dx_frame_fence_values_[backbuffer_index], dx_fence_event_);
                    WaitForSingleObject(dx_fence_event_, INFINITE);
                }
            }
            image_index = static_cast<UInt32>(backbuffer_index);
            return true;
        }

        case RenderBackendApiType::Vulkan: {
            if (!device_ || vk_acquire_semaphores_.empty() || vk_frame_fences_.empty() || vk_swapchain_ == VK_NULL_HANDLE) {
                return false;
            }

            auto* vk_device = static_cast<vulkan::IDevice*>(
                device_->getNativeObject(GfxObjectTypes::VK_Device));
            DO_ASSERT(vk_device != nullptr, "GfxViewportSurface::acquire: failed to get native cutie vulkan device.");

            try {
                auto test_ptr = reinterpret_cast<void**>(vk_device);
                volatile void* vtable = test_ptr[0];
                (void)vtable;
            } catch (...) {
                DO_ERROR("GfxViewportSurface::acquire: vk_device pointer is invalid!");
                return false;
            }

            const Size_t frame_slot = vk_current_frame_slot_ % vk_acquire_semaphores_.size();
            VkDevice vk_device_handle = getVulkanBackend()->getDevice();
            DO_ASSERT(vkWaitForFences(vk_device_handle, 1, &vk_frame_fences_[frame_slot], VK_TRUE, UINT64_MAX) == VK_SUCCESS,
                "GfxViewportSurface::acquire: failed to wait for frame fence.");

            const VkSemaphore acquire_semaphore = vk_acquire_semaphores_[frame_slot];
            const VkResult acquire_result = vkAcquireNextImageKHR(vk_device_handle, vk_swapchain_, UINT64_MAX,
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
            vk_active_frame_slot_ = frame_slot;
            return true;
        }

        default:
            return false;
        }
    }

    Bool GfxViewportSurface::present(UInt32 image_index) {
        switch (api_type_) {
        case RenderBackendApiType::OpenGL: {
            if (!window_ || image_index >= framebuffers_.size()) return false;
            if (m_gl_fb_width <= 0 || m_gl_fb_height <= 0) return true;
            glfwSwapBuffers(window_);
            return true;
        }

        case RenderBackendApiType::D3D12: {
            if (!dx_swapchain_ || !getD3D12Backend()->getGraphicsQueue()) {
                return false;
            }

            HRESULT hr = DXGI_ERROR_INVALID_CALL;
            switch (RenderSettings::GetPresentMode()) {
            case PresentMode::VSync:
                hr = dx_swapchain_->Present(1, 0);
                break;
            case PresentMode::Immediate:
                hr = dx_swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING);
                break;
            case PresentMode::Mailbox:
            default:
                hr = dx_swapchain_->Present(0, 0);
                break;
            }
            if (hr == DXGI_ERROR_DEVICE_REMOVED) {
                HRESULT device_removed = getD3D12Backend()->getDevice()->GetDeviceRemovedReason();
                DO_ERROR("GfxViewportSurface::present: Device removed! HRESULT={:08X}", static_cast<UINT>(device_removed));
                return false;
            }

            ++dx_fence_value_;
            dx_frame_fence_values_[image_index] = dx_fence_value_;
            HRESULT signal_hr = getD3D12Backend()->getGraphicsQueue()->Signal(dx_fence_.Get(), dx_fence_value_);
            if (FAILED(signal_hr)) {
                DO_ERROR("GfxViewportSurface::present: Signal fence failed with HRESULT={:08X}", static_cast<UINT>(signal_hr));
                return false;
            }

            return SUCCEEDED(hr);
        }

        case RenderBackendApiType::Vulkan: {
            if (!device_ || vk_swapchain_ == VK_NULL_HANDLE || vk_present_queue_ == VK_NULL_HANDLE) {
                return false;
            }

            if (vk_active_frame_slot_ >= vk_frame_fences_.size() || image_index >= vk_present_semaphores_.size()) {
                DO_ASSERT(false, "GfxViewportSurface::present: invalid swapchain image index.");
                return false;
            }

            VkSemaphore present_semaphore = vk_present_semaphores_[image_index];
            VkFence frame_fence = vk_frame_fences_[vk_active_frame_slot_];
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
            present_info.pSwapchains = &vk_swapchain_;
            present_info.pImageIndices = &image_index;
            present_info.pResults = nullptr;

            const VkResult present_result = vkQueuePresentKHR(vk_present_queue_, &present_info);
            vk_current_frame_slot_ = (vk_active_frame_slot_ + 1) % vk_acquire_semaphores_.size();
            vk_active_frame_slot_ = (std::numeric_limits<Size_t>::max)();
            return present_result == VK_SUCCESS || present_result == VK_SUBOPTIMAL_KHR;
        }

        default:
            return false;
        }
    }

    Bool GfxViewportSurface::resize(UInt32 w, UInt32 h) {
        switch (api_type_) {
        case RenderBackendApiType::OpenGL: {
            updateOpenGLFramebufferSize();
            textures_.clear();
            framebuffers_.clear();
            createSwapchainTexturesOpenGL();
            return !framebuffers_.empty();
        }

        case RenderBackendApiType::D3D12: {
            if (w == 0 || h == 0) {
                return false;
            }

            if (device_) {
                device_->waitForIdle();
            }

            textures_.clear();
            framebuffers_.clear();

            if (device_) {
                device_->runGarbageCollection();
                device_->waitForIdle();
            }

            waitD3D12Gpu();
            releaseD3D12Backbuffers();

            dx_width_ = w;
            dx_height_ = h;

            HRESULT hr = dx_swapchain_->ResizeBuffers(kBackbufferCount, dx_width_, dx_height_, dx_format_, GetSwapchainFlags());
            if (FAILED(hr)) {
                DO_ERROR("GfxViewportSurface::resize: ResizeBuffers failed with HRESULT=0x{:08X}", static_cast<UINT>(hr));
            }
            DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::resize: ResizeBuffers failed");

            createD3D12BackbufferRTVs();
            createSwapchainTexturesD3D12();
            if (device_) {
                device_->runGarbageCollection();
            }
            return !framebuffers_.empty();
        }

        case RenderBackendApiType::Vulkan: {
            if (w == 0 || h == 0) {
                return false;
            }

            if (device_) {
                device_->waitForIdle();
            }

            textures_.clear();
            framebuffers_.clear();
            destroyVulkanSemaphores();
            if (device_) {
                device_->runGarbageCollection();
                device_->waitForIdle();
            }

            if (m_owns_swapchain) {
                VkDevice vk_device_handle = getVulkanBackend()->getDevice();
                for (auto image_view : vk_imageviews_) {
                    if (image_view != VK_NULL_HANDLE) {
                        vkDestroyImageView(vk_device_handle, image_view, nullptr);
                    }
                }
                vk_imageviews_.clear();
                if (vk_swapchain_ != VK_NULL_HANDLE) {
                    vkDestroySwapchainKHR(vk_device_handle, vk_swapchain_, nullptr);
                    vk_swapchain_ = VK_NULL_HANDLE;
                }
                vk_images_.clear();

                createVulkanSwapchain(w, h);
                createVulkanImageViews();
            } else {
                if (!getVulkanBackend()->recreateSwapchain(window_, w, h)) {
                    return false;
                }
                vk_swapchain_ = getVulkanBackend()->getSwapchain();
                vk_images_ = getVulkanBackend()->getSwapchainImages();
                vk_imageviews_ = getVulkanBackend()->getSwapchainImageViews();
                vk_format_ = getVulkanBackend()->getSwapchainImageFormat();
                const auto extent = getVulkanBackend()->getSwapchainExtent2d();
                vk_extent_ = { static_cast<uint32_t>(extent.x), static_cast<uint32_t>(extent.y) };
            }

            createVulkanSemaphores();
            createSwapchainTexturesVulkan();
            if (device_) {
                device_->runGarbageCollection();
            }
            return !framebuffers_.empty();
        }

        default:
            return false;
        }
    }

    Bool GfxViewportSurface::initializeVulkan(GfxDeviceHandle device, GLFWwindow* window, void* host_handle,
                                              UInt32 w, UInt32 h, Bool is_primary) {
        if (!device || !getVulkanBackend()) {
            return false;
        }

        if (is_primary) {
            vk_swapchain_ = getVulkanBackend()->getSwapchain();
            vk_present_queue_ = getVulkanBackend()->getPresentQueue();
            vk_images_ = getVulkanBackend()->getSwapchainImages();
            vk_imageviews_ = getVulkanBackend()->getSwapchainImageViews();
            vk_format_ = getVulkanBackend()->getSwapchainImageFormat();
            const auto extent = getVulkanBackend()->getSwapchainExtent2d();
            vk_extent_ = { static_cast<uint32_t>(extent.x), static_cast<uint32_t>(extent.y) };
            m_owns_swapchain = false;
            vk_owns_surface_ = false;
        } else {
            if (!createVulkanSurface(window, host_handle)) {
                return false;
            }
            createVulkanSwapchain(w, h);
            createVulkanImageViews();
            vk_present_queue_ = getVulkanBackend()->getPresentQueue();
            m_owns_swapchain = true;
            vk_owns_surface_ = true;
        }

        createVulkanSemaphores();
        createSwapchainTexturesVulkan();
        return !textures_.empty();
    }

    Bool GfxViewportSurface::initializeD3D12(GfxDeviceHandle device, GLFWwindow* window, void* host_handle, UInt32 w, UInt32 h) {
        if (!device || !getD3D12Backend()) {
            return false;
        }

        m_owns_swapchain = true;
        createD3D12Swapchain(w, h);
        createD3D12RTVHeap();
        createD3D12BackbufferRTVs();
        createD3D12Fence();
        createSwapchainTexturesD3D12();
        return !textures_.empty();
    }

    Bool GfxViewportSurface::initializeOpenGL(GfxDeviceHandle device, GLFWwindow* window, UInt32 w, UInt32 h) {
        if (!device || !getOpenGLBackend()) {
            return false;
        }

        m_owns_swapchain = false;
        updateOpenGLFramebufferSize();
        createSwapchainTexturesOpenGL();
        return !framebuffers_.empty();
    }

    Bool GfxViewportSurface::createVulkanSurface(GLFWwindow* window, void* host_handle) {
        if (host_handle != nullptr) {
#if defined(DO_PLATFORM_WINDOWS)
            VkWin32SurfaceCreateInfoKHR surface_info{};
            surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surface_info.hwnd = static_cast<HWND>(host_handle);
            surface_info.hinstance = GetModuleHandle(nullptr);
            const VkResult result = vkCreateWin32SurfaceKHR(getVulkanBackend()->getInstance(), &surface_info, nullptr, &vk_surface_);
            DO_ASSERT(result == VK_SUCCESS, "GfxViewportSurface::createVulkanSurface(host) failed");
            return result == VK_SUCCESS;
#else
            DO_ASSERT(false, "GfxViewportSurface::createVulkanSurface: host unsupported platform");
            return false;
#endif
        }
        if (window != nullptr) {
            const VkResult result = glfwCreateWindowSurface(getVulkanBackend()->getInstance(), window, nullptr, &vk_surface_);
            DO_ASSERT(result == VK_SUCCESS, "GfxViewportSurface::createVulkanSurface GLFW failed");
            return result == VK_SUCCESS;
        }
        DO_ASSERT(false, "GfxViewportSurface::createVulkanSurface: no handle");
        return false;
    }

    void GfxViewportSurface::createVulkanSwapchain(UInt32 w, UInt32 h) {
        const auto swapchain_details = QueryVulkanSwapchainSupport(getVulkanBackend()->getPhysicalDevice(), vk_surface_);

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
            } else if (host_handle_ != nullptr) {
                RECT rect;
                GetClientRect(static_cast<HWND>(host_handle_), &rect);
                sel_w = rect.right - rect.left;
                sel_h = rect.bottom - rect.top;
            } else {
                glfwGetFramebufferSize(window_, &sel_w, &sel_h);
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
        swapchain_info.surface = vk_surface_;
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

        const VkResult result = vkCreateSwapchainKHR(getVulkanBackend()->getDevice(), &swapchain_info, nullptr, &vk_swapchain_);
        DO_ASSERT(result == VK_SUCCESS, "GfxViewportSurface::createVulkanSwapchain failed with VkResult={}", static_cast<int>(result));

        vkGetSwapchainImagesKHR(getVulkanBackend()->getDevice(), vk_swapchain_, &image_count, nullptr);
        vk_images_.resize(image_count);
        vkGetSwapchainImagesKHR(getVulkanBackend()->getDevice(), vk_swapchain_, &image_count, vk_images_.data());

        vk_format_ = chosen_surface_format.format;
        vk_extent_ = chosen_extent;
    }

    void GfxViewportSurface::createVulkanImageViews() {
        vk_imageviews_.clear();
        vk_imageviews_.reserve(vk_images_.size());
        for (const auto swapchain_image : vk_images_) {
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = swapchain_image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = vk_format_;
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
            vk_imageviews_.push_back(image_view);
        }
    }

    void GfxViewportSurface::createVulkanSemaphores() {
        if (!getVulkanBackend()) {
            return;
        }

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkDevice vk_device = getVulkanBackend()->getDevice();
        vk_acquire_semaphores_.clear();
        vk_present_semaphores_.clear();
        vk_frame_fences_.clear();
        const Size_t frame_count = vk_images_.size();
        vk_acquire_semaphores_.reserve(frame_count);
        vk_present_semaphores_.reserve(frame_count);
        vk_frame_fences_.reserve(frame_count);

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (Size_t i = 0; i < frame_count; ++i) {
            VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &acquire_semaphore) == VK_SUCCESS,
                "GfxViewportSurface::createVulkanSemaphores: failed to create acquire semaphore.");
            vk_acquire_semaphores_.push_back(acquire_semaphore);

            VkSemaphore present_semaphore = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &present_semaphore) == VK_SUCCESS,
                "GfxViewportSurface::createVulkanSemaphores: failed to create present semaphore.");
            vk_present_semaphores_.push_back(present_semaphore);

            VkFence frame_fence = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateFence(vk_device, &fence_info, nullptr, &frame_fence) == VK_SUCCESS,
                "GfxViewportSurface::createVulkanSemaphores: failed to create frame fence.");
            vk_frame_fences_.push_back(frame_fence);
        }

        vk_current_frame_slot_ = 0;
        vk_active_frame_slot_ = (std::numeric_limits<Size_t>::max)();
    }

    void GfxViewportSurface::destroyVulkanSemaphores() {
        if (!getVulkanBackend()) {
            vk_acquire_semaphores_.clear();
            vk_present_semaphores_.clear();
            vk_frame_fences_.clear();
            vk_current_frame_slot_ = 0;
            vk_active_frame_slot_ = (std::numeric_limits<Size_t>::max)();
            return;
        }

        VkDevice vk_device = getVulkanBackend()->getDevice();
        if (vk_device != VK_NULL_HANDLE) {
            for (auto semaphore : vk_acquire_semaphores_) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(vk_device, semaphore, nullptr);
                }
            }
            for (auto semaphore : vk_present_semaphores_) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(vk_device, semaphore, nullptr);
                }
            }
            for (auto fence : vk_frame_fences_) {
                if (fence != VK_NULL_HANDLE) {
                    vkDestroyFence(vk_device, fence, nullptr);
                }
            }
        }
        vk_acquire_semaphores_.clear();
        vk_present_semaphores_.clear();
        vk_frame_fences_.clear();
        vk_current_frame_slot_ = 0;
        vk_active_frame_slot_ = (std::numeric_limits<Size_t>::max)();
    }

    void GfxViewportSurface::destroyVulkanOwnedState() {
        if (!getVulkanBackend()) {
            return;
        }

        VkDevice vk_device = getVulkanBackend()->getDevice();
        if (vk_device == VK_NULL_HANDLE) {
            return;
        }

        if (m_owns_swapchain) {
            for (auto image_view : vk_imageviews_) {
                if (image_view != VK_NULL_HANDLE) {
                    vkDestroyImageView(vk_device, image_view, nullptr);
                }
            }
            vk_imageviews_.clear();
            if (vk_swapchain_ != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(vk_device, vk_swapchain_, nullptr);
                vk_swapchain_ = VK_NULL_HANDLE;
            }
        }
        vk_images_.clear();

        if (vk_owns_surface_ && vk_surface_ != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(getVulkanBackend()->getInstance(), vk_surface_, nullptr);
            vk_surface_ = VK_NULL_HANDLE;
        }
    }

    void GfxViewportSurface::createSwapchainTexturesVulkan() {
        if (!device_ || !getVulkanBackend()) {
            return;
        }

        textures_.clear();
        framebuffers_.clear();
        const auto swapchain_format = ToRHIFormatVK(vk_format_);

        for (const auto& image : vk_images_) {
            auto texture_desc = GfxTextureDesc()
                .setDimension(GfxTextureDimension::Texture2D)
                .setFormat(swapchain_format)
                .setWidth(vk_extent_.width)
                .setHeight(vk_extent_.height)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(GfxResourceStates::Present)
                .setDebugName("Swapchain Image");

            auto tex = create_ref<GfxTexture>(device_->createHandleForNativeTexture(GfxObjectTypes::VK_Image, image, texture_desc), texture_desc, "Swapchain Image");
            textures_.push_back(tex);
            GfxFramebufferDesc framebuffer_desc{};
            framebuffer_desc.addColorAttachment(tex);
            auto framebuffer = create_ref<GfxFramebuffer>(framebuffer_desc);
            framebuffer->initializeRHI(device_);
            framebuffers_.push_back(framebuffer);
        }
    }

    void GfxViewportSurface::updateOpenGLFramebufferSize() {
        if (window_) {
            glfwGetFramebufferSize(window_, &m_gl_fb_width, &m_gl_fb_height);
        } else {
            m_gl_fb_width = 0;
            m_gl_fb_height = 0;
        }
    }

    void GfxViewportSurface::createSwapchainTexturesOpenGL() {
        if (!device_ || !getOpenGLBackend()) {
            return;
        }

        textures_.clear();
        framebuffers_.clear();
        if (m_gl_fb_width <= 0 || m_gl_fb_height <= 0) {
            return;
        }

        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info.addColorFormat(GfxFormat::RGBA8_UNORM);
        const auto framebuffer = opengl::createDefaultFramebuffer(device_);
        DO_ASSERT(framebuffer != nullptr, "GfxViewportSurface::createSwapchainTexturesOpenGL: failed to create default framebuffer.");
        framebuffers_.push_back(create_ref<GfxFramebuffer>(framebuffer, framebuffer_info));
    }

    void GfxViewportSurface::createSwapchainTexturesD3D12() {
        if (!device_ || !getD3D12Backend()) {
            return;
        }

        textures_.clear();
        framebuffers_.clear();
        const auto swapchain_format = RHIFormatD3D12(dx_format_);

        for (auto* backbuffer : dx_backbuffers_) {
            auto texture_desc = GfxTextureDesc()
                .setDimension(GfxTextureDimension::Texture2D)
                .setFormat(swapchain_format)
                .setWidth(dx_width_)
                .setHeight(dx_height_)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(GfxResourceStates::Present)
                .setDebugName("Swapchain Image");

            auto tex = create_ref<GfxTexture>(device_->createHandleForNativeTexture(GfxObjectTypes::D3D12_Resource, static_cast<cutie::Object>(backbuffer), texture_desc), texture_desc, "Swapchain Image");
            textures_.push_back(tex);
            GfxFramebufferDesc framebuffer_desc{};
            framebuffer_desc.addColorAttachment(tex);
            auto framebuffer = create_ref<GfxFramebuffer>(framebuffer_desc);
            framebuffer->initializeRHI(device_);
            framebuffers_.push_back(framebuffer);
        }
    }

    void GfxViewportSurface::createD3D12Swapchain(UInt32 w, UInt32 h) {
        HWND hwnd = host_handle_ != nullptr ? static_cast<HWND>(host_handle_) : nullptr;
        if (hwnd == nullptr && window_ != nullptr) {
            hwnd = glfwGetWin32Window(window_);
        }
        DO_ASSERT(hwnd != nullptr, "GfxViewportSurface::createD3D12Swapchain: no window handle.");

        UInt32 swapchain_w = w;
        UInt32 swapchain_h = h;
        if (host_handle_ != nullptr || swapchain_w == 0 || swapchain_h == 0) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            if (rect.right > rect.left && rect.bottom > rect.top) {
                swapchain_w = static_cast<UInt32>(rect.right - rect.left);
                swapchain_h = static_cast<UInt32>(rect.bottom - rect.top);
            }
        }
        dx_width_ = (swapchain_w == 0) ? 1 : swapchain_w;
        dx_height_ = (swapchain_h == 0) ? 1 : swapchain_h;

        DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
        swapchain_desc.Width = dx_width_;
        swapchain_desc.Height = dx_height_;
        swapchain_desc.Format = dx_format_;
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

        hr = swapchain1.As(&dx_swapchain_);
        DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12Swapchain: QueryInterface IDXGISwapChain4 failed with HRESULT={:08X}", static_cast<UINT>(hr));

        getD3D12Backend()->getFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        { char buf[128]; snprintf(buf, sizeof(buf), "[D3D12] swapchain created: %ux%u, format=%u\n", dx_width_, dx_height_, static_cast<UINT>(dx_format_)); OutputDebugStringA(buf); }
    }

    void GfxViewportSurface::createD3D12RTVHeap() {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = kBackbufferCount;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heap_desc.NodeMask = 0;

        HRESULT hr = getD3D12Backend()->getDevice()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&dx_rtv_heap_));
        DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12RTVHeap failed with HRESULT={:08X}", static_cast<UINT>(hr));

        dx_rtv_descriptor_size_ = getD3D12Backend()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    void GfxViewportSurface::createD3D12BackbufferRTVs() {
        releaseD3D12Backbuffers();

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = dx_rtv_heap_->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < kBackbufferCount; ++i) {
            ID3D12Resource* backbuffer = nullptr;
            HRESULT hr = dx_swapchain_->GetBuffer(i, IID_PPV_ARGS(&backbuffer));
            DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12BackbufferRTVs: GetBuffer({}) failed with HRESULT={:08X}", i, static_cast<UINT>(hr));

            getD3D12Backend()->getDevice()->CreateRenderTargetView(backbuffer, nullptr, rtv_handle);
            dx_backbuffers_.push_back(backbuffer);

            if (getD3D12Backend()->isValidationEnabled()) {
                wchar_t name[64];
                swprintf(name, 64, L"Backbuffer %u", i);
                backbuffer->SetName(name);
            }

            rtv_handle.ptr += dx_rtv_descriptor_size_;
        }
    }

    void GfxViewportSurface::createD3D12Fence() {
        HRESULT hr = getD3D12Backend()->getDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dx_fence_));
        DO_ASSERT(SUCCEEDED(hr), "GfxViewportSurface::createD3D12Fence failed with HRESULT={:08X}", static_cast<UINT>(hr));

        dx_fence_value_ = 0;
        dx_fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        DO_ASSERT(dx_fence_event_ != nullptr, "GfxViewportSurface::createD3D12Fence: CreateEventW failed.");

        for (UINT i = 0; i < kBackbufferCount; ++i) {
            dx_frame_fence_values_[i] = 0;
        }
    }

    void GfxViewportSurface::releaseD3D12Backbuffers() {
        for (auto& bb : dx_backbuffers_) {
            if (bb) {
                bb->Release();
            }
        }
        dx_backbuffers_.clear();
    }

    void GfxViewportSurface::waitD3D12Gpu() {
        if (getD3D12Backend()->getGraphicsQueue() && dx_fence_) {
            ++dx_fence_value_;
            HRESULT hr = getD3D12Backend()->getGraphicsQueue()->Signal(dx_fence_.Get(), dx_fence_value_);
            if (SUCCEEDED(hr)) {
                dx_fence_->SetEventOnCompletion(dx_fence_value_, dx_fence_event_);
                WaitForSingleObject(dx_fence_event_, INFINITE);
            }
        }
    }

} // dodoe
