// do@Redlive

#include "gfx_context.h"

#include "../render/render_settings.h"

#include "vulkan/vulkan.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

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

        class RhiMessageCallback : public GfxMessageCallback {
        public:
            void message(GfxMessageSeverity severity, const char* message_text) override {
                if (severity == GfxMessageSeverity::Info || severity == GfxMessageSeverity::Warning) return;
                DO_ERROR("RHI::ERROR: {}", message_text);
            }
        };

    }

    Scope<GfxContext> GfxContext::Create(const GfxBackendCreateInfo& create_info) {
        auto context = create_scope<GfxContext>();
        context->initialize(create_info);
        return context;
    }

    void GfxContext::Destroy(Scope<GfxContext>& backend) {
        if (!backend) return;
        backend->shutdown();
        backend.reset();
    }

    void GfxContext::initialize(const GfxBackendCreateInfo& create_info) {
        window_handle_ = create_info.window_handle;

        if (create_info.api_type == RenderBackendApiType::Vulkan) {
            initializeVulkan(create_info);
        } else if (create_info.api_type == RenderBackendApiType::OpenGL) {
            initializeOpenGL(create_info);
        } else if (create_info.api_type == RenderBackendApiType::D3D12) {
            initializeD3D12(create_info);
        }

        if (device_) {
            DeviceCapabilities caps{};
            caps.bindless_supported = device_->queryFeatureSupport(cutie::Feature::HeapDirectlyIndexed);
            caps.compute_queue_supported = device_->queryFeatureSupport(cutie::Feature::ComputeQueue);
            RenderSettings::SetDeviceCapabilities(caps);
        }
        RenderSettings::ResolveFeatures(create_info.feature_settings);
        m_gpu_driven_supported = RenderSettings::GetResolvedFeatures().gpu_driven_active;
    }

    void GfxContext::initializeVulkan(const GfxBackendCreateInfo& create_info) {
        OutputDebugStringA("[GFX] Vulkan initialize begin\n");

        vulkan_backend_ = VulkanBackend::Create({create_info.window_handle, create_info.host_handle, create_info.enable_validation, create_info.width, create_info.height});

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(vulkan_backend_->getInstance()));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device(vulkan_backend_->getDevice()));

        auto* error_callback = new RhiMessageCallback();

        vulkan::DeviceDesc device_desc{};
        device_desc.errorCB = error_callback;
        device_desc.instance = vulkan_backend_->getInstance();
        device_desc.physicalDevice = vulkan_backend_->getPhysicalDevice();
        device_desc.device = vulkan_backend_->getDevice();
        device_desc.graphicsQueue = vulkan_backend_->getGraphicsQueue();
        device_desc.graphicsQueueIndex = vulkan_backend_->getGraphicsQueueIndex();
        device_desc.computeQueue = vulkan_backend_->getComputeQueue();
        device_desc.computeQueueIndex = vulkan_backend_->getComputeQueueIndex();
        device_desc.transferQueue = vulkan_backend_->getGraphicsQueue();
        device_desc.transferQueueIndex = vulkan_backend_->getGraphicsQueueIndex();
        device_desc.instanceExtensions = const_cast<const char**>(vulkan_backend_->getInstanceExtensions().data());
        device_desc.numInstanceExtensions = vulkan_backend_->getInstanceExtensions().size();
        device_desc.deviceExtensions = const_cast<const char**>(vulkan_backend_->getDeviceExtensions().data());
        device_desc.numDeviceExtensions = vulkan_backend_->getDeviceExtensions().size();
        device_desc.bufferDeviceAddressSupported = true;

        device_ = vulkan::createDevice(device_desc);
        DO_ASSERT(device_ != nullptr, "GfxBackend::initializeVulkan: failed to create cutie vulkan device.");

        if (create_info.enable_validation) {
            device_ = validation::createValidationLayer(device_);
            DO_ASSERT(device_ != nullptr, "GfxBackend::initializeVulkan: failed to create validation layer.");
        }

        createSwapchainSemaphores();
        createSwapchainTexturesVulkan();
        cmd_ = device_->createCommandList();

        OutputDebugStringA("[GFX] Vulkan initialize done\n");
    }

    void GfxContext::initializeOpenGL(const GfxBackendCreateInfo& create_info) {
        OutputDebugStringA("[GFX] OpenGL initialize begin\n");

        opengl_backend_ = OpenGLBackend::Create({create_info.window_handle, create_info.width, create_info.height});
        DO_ASSERT(opengl_backend_ != nullptr, "GfxBackend::initializeOpenGL: failed to create OpenGL backend.");

        auto* error_callback = new RhiMessageCallback();

        opengl::DeviceDesc device_desc{};
        device_desc.messageCallback = error_callback;
        device_desc.glLoaderFunc = reinterpret_cast<opengl::GLloaderFunc>(glfwGetProcAddress);

        device_ = opengl::createDevice(device_desc);
        DO_ASSERT(device_ != nullptr, "GfxBackend::initializeOpenGL: failed to create cutie opengl device.");

        createSwapchainTexturesOpenGL();
        cmd_ = device_->createCommandList();

        OutputDebugStringA("[GFX] OpenGL initialize done\n");
    }

    void GfxContext::initializeD3D12(const GfxBackendCreateInfo& create_info) {
        OutputDebugStringA("[GFX] D3D12 initialize begin\n");

        d3d12_backend_ = D3D12Backend::Create({create_info.window_handle, create_info.host_handle, create_info.enable_validation, create_info.width, create_info.height});
        DO_ASSERT(d3d12_backend_ != nullptr, "GfxBackend::initializeD3D12: failed to create D3D12 backend.");

        auto* error_callback = new RhiMessageCallback();

        d3d12::DeviceDesc device_desc{};
        device_desc.errorCB = error_callback;
        device_desc.pDevice = d3d12_backend_->getDevice();
        device_desc.pGraphicsCommandQueue = d3d12_backend_->getGraphicsQueue();
        device_desc.pComputeCommandQueue = d3d12_backend_->getComputeQueue();
        device_desc.pCopyCommandQueue = d3d12_backend_->getCopyQueue();

        device_ = d3d12::createDevice(device_desc);
        DO_ASSERT(device_ != nullptr, "GfxBackend::initializeD3D12: failed to create cutie d3d12 device.");

        if (create_info.enable_validation) {
            device_ = validation::createValidationLayer(device_);
            DO_ASSERT(device_ != nullptr, "GfxBackend::initializeD3D12: failed to create validation layer.");
        }

        createSwapchainTexturesD3D12();
        cmd_ = device_->createCommandList();

        OutputDebugStringA("[GFX] D3D12 initialize done\n");
    }

    void GfxContext::shutdown() {
        cmd_ = nullptr;
        swapchain_textures_.clear();
        swapchain_framebuffers_.clear();
        if (device_) {
            device_->waitForIdle();
            device_->runGarbageCollection();
        }
        destroySwapchainSemaphores();
        device_ = nullptr;

        if (vulkan_backend_) {
            VulkanBackend::Destroy(vulkan_backend_);
        }
        if (opengl_backend_) {
            OpenGLBackend::Destroy(opengl_backend_);
        }
        if (d3d12_backend_) {
            D3D12Backend::Destroy(d3d12_backend_);
        }
    }

    void GfxContext::waitForIdle() {
        if (device_) device_->waitForIdle();
    }

    void GfxContext::clearGarbage() {
        if (device_) device_->runGarbageCollection();
    }

    Bool GfxContext::acquireOpenGLContext() {
        return !opengl_backend_ || opengl_backend_->acquireContext();
    }

    void GfxContext::releaseOpenGLContext() {
        if (opengl_backend_) opengl_backend_->releaseContext();
    }

    Vector2i GfxContext::getSwapchainExtent2d() const {
        if (vulkan_backend_) {
            return vulkan_backend_->getSwapchainExtent2d();
        }
        if (opengl_backend_) {
            return opengl_backend_->getSwapchainExtent2d();
        }
        if (d3d12_backend_) {
            return d3d12_backend_->getSwapchainExtent2d();
        }
        return Vector2i(0, 0);
    }

    void GfxContext::createSwapchainTexturesVulkan() {
        if (!device_ || !vulkan_backend_) {
            return;
        }

        swapchain_textures_.clear();
        swapchain_framebuffers_.clear();
        const auto swapchain_format = ToRHIFormatVK(vulkan_backend_->getSwapchainImageFormat());

        for (const auto& image : vulkan_backend_->getSwapchainImages()) {
            auto texture_desc = GfxTextureDesc()
                .setDimension(GfxTextureDimension::Texture2D)
                .setFormat(swapchain_format)
                .setWidth(vulkan_backend_->getSwapchainExtent2d().x)
                .setHeight(vulkan_backend_->getSwapchainExtent2d().y)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(GfxResourceStates::Present)
                .setDebugName("Swapchain Image");

            auto tex = create_ref<GfxTexture>(device_->createHandleForNativeTexture(GfxObjectTypes::VK_Image, image, texture_desc), texture_desc, "Swapchain Image");
            swapchain_textures_.push_back(tex);
            GfxFramebufferDesc framebuffer_desc{};
            framebuffer_desc.addColorAttachment(tex);
            auto framebuffer = create_ref<GfxFramebuffer>(framebuffer_desc);
            framebuffer->initializeRHI(device_);
            swapchain_framebuffers_.push_back(framebuffer);
        }
    }

    void GfxContext::createSwapchainTexturesOpenGL() {
        if (!device_ || !opengl_backend_) {
            return;
        }

        swapchain_textures_.clear();
        swapchain_framebuffers_.clear();
        const auto extent = opengl_backend_->getSwapchainExtent2d();
        if (extent.x <= 0 || extent.y <= 0) {
            return;
        }

        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info.addColorFormat(GfxFormat::RGBA8_UNORM);
        const auto framebuffer = opengl::createDefaultFramebuffer(device_);
        DO_ASSERT(framebuffer != nullptr, "GfxContext::createSwapchainTexturesOpenGL: failed to create default framebuffer.");
        swapchain_framebuffers_.push_back(create_ref<GfxFramebuffer>(framebuffer, framebuffer_info));
    }

    namespace {
        GfxFormat RHIFormatD3D12(DXGI_FORMAT format) {
            switch (format) {
                case DXGI_FORMAT_R8G8B8A8_UNORM:     return GfxFormat::RGBA8_UNORM;
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return GfxFormat::SRGBA8_UNORM;
                case DXGI_FORMAT_B8G8R8A8_UNORM:     return GfxFormat::BGRA8_UNORM;
                case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return GfxFormat::SBGRA8_UNORM;
                default: return GfxFormat::RGBA8_UNORM;
            }
        }
    }

    void GfxContext::createSwapchainTexturesD3D12() {
        if (!device_ || !d3d12_backend_) {
            return;
        }

        swapchain_textures_.clear();
        swapchain_framebuffers_.clear();
        const auto swapchain_format = RHIFormatD3D12(d3d12_backend_->getBackbufferFormat());
        const auto extent = d3d12_backend_->getSwapchainExtent2d();

        for (auto* backbuffer : d3d12_backend_->getBackbuffers()) {
            auto texture_desc = GfxTextureDesc()
                .setDimension(GfxTextureDimension::Texture2D)
                .setFormat(swapchain_format)
                .setWidth(extent.x)
                .setHeight(extent.y)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(GfxResourceStates::Present)
                .setDebugName("Swapchain Image");

            auto tex = create_ref<GfxTexture>(device_->createHandleForNativeTexture(GfxObjectTypes::D3D12_Resource, static_cast<cutie::Object>(backbuffer), texture_desc), texture_desc, "Swapchain Image");
            swapchain_textures_.push_back(tex);
            GfxFramebufferDesc framebuffer_desc{};
            framebuffer_desc.addColorAttachment(tex);
            auto framebuffer = create_ref<GfxFramebuffer>(framebuffer_desc);
            framebuffer->initializeRHI(device_);
            swapchain_framebuffers_.push_back(framebuffer);
        }
    }

    Bool GfxContext::acquireNextSwapchainImage(UInt32& image_index) {
        if (opengl_backend_) {
            image_index = 0;
            opengl_backend_->updateFramebufferSize();
            const auto extent = opengl_backend_->getSwapchainExtent2d();
            return extent.x > 0 && extent.y > 0 && !swapchain_framebuffers_.empty();
        }

        if (d3d12_backend_) {
            UINT backbuffer_index = 0;
            if (!d3d12_backend_->acquireNextImage(backbuffer_index)) {
                return false;
            }
            image_index = static_cast<UInt32>(backbuffer_index);
            return true;
        }

        if (!vulkan_backend_ || !device_ || acquire_semaphores_.empty() || frame_fences_.empty()) {
            return false;
        }

        auto* vk_device = static_cast<vulkan::IDevice*>(
            device_->getNativeObject(GfxObjectTypes::VK_Device));
        DO_ASSERT(vk_device != nullptr, "GfxContext::acquireNextSwapchainImage: failed to get native cutie vulkan device.");

        try {
            auto test_ptr = reinterpret_cast<void**>(vk_device);
            volatile void* vtable = test_ptr[0];
            (void)vtable;
        } catch (...) {
            DO_ERROR("GfxContext::acquireNextSwapchainImage: vk_device pointer is invalid!");
            return false;
        }

        const Size_t frame_slot = current_frame_slot_ % acquire_semaphores_.size();
        VkDevice vk_device_handle = vulkan_backend_->getDevice();
        DO_ASSERT(vkWaitForFences(vk_device_handle, 1, &frame_fences_[frame_slot], VK_TRUE, UINT64_MAX) == VK_SUCCESS,
            "GfxContext::acquireNextSwapchainImage: failed to wait for frame fence.");

        const VkSemaphore acquire_semaphore = acquire_semaphores_[frame_slot];
        if (!vulkan_backend_->acquireNextImage(image_index, acquire_semaphore)) {
            return false;
        }

        if (!vk_device) {
            DO_ERROR("GfxContext::acquireNextSwapchainImage: vk_device is NULL!");
            return false;
        }
        if (!acquire_semaphore) {
            DO_ERROR("GfxContext::acquireNextSwapchainImage: acquire_semaphore is NULL!");
            return false;
        }

        vk_device->queueWaitForSemaphore(GfxCommandQueue::Graphics, acquire_semaphore, 0);
        active_frame_slot_ = frame_slot;

        return true;
    }

    Bool GfxContext::presentSwapchainImage(UInt32 image_index) {
        if (opengl_backend_) {
            if (!opengl_backend_->getWindow() || image_index >= swapchain_framebuffers_.size()) return false;
            const auto extent = opengl_backend_->getSwapchainExtent2d();
            if (extent.x <= 0 || extent.y <= 0) return true;
            glfwSwapBuffers(opengl_backend_->getWindow());
            return true;
        }

        if (d3d12_backend_) {
            return d3d12_backend_->presentImage(static_cast<UINT>(image_index));
        }

        if (!vulkan_backend_ || !device_) {
            return false;
        }

        if (active_frame_slot_ >= frame_fences_.size() || image_index >= present_semaphores_.size()) {
            DO_ASSERT(false, "GfxContext::presentSwapchainImage: invalid swapchain image index.");
            return false;
        }

        VkSemaphore present_semaphore = present_semaphores_[image_index];
        VkFence frame_fence = frame_fences_[active_frame_slot_];
        VkDevice vk_device_handle = vulkan_backend_->getDevice();
        DO_ASSERT(vkResetFences(vk_device_handle, 1, &frame_fence) == VK_SUCCESS,
            "GfxContext::presentSwapchainImage: failed to reset frame fence.");

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 0;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &present_semaphore;
        DO_ASSERT(vkQueueSubmit(vulkan_backend_->getGraphicsQueue(), 1, &submit_info, frame_fence) == VK_SUCCESS,
            "GfxContext::presentSwapchainImage: failed to submit present signal semaphore.");

        const Bool present_result = vulkan_backend_->presentImage(image_index, present_semaphore);
        current_frame_slot_ = (active_frame_slot_ + 1) % acquire_semaphores_.size();
        active_frame_slot_ = (std::numeric_limits<Size_t>::max)();
        return present_result;
    }

    Bool GfxContext::recreateSwapchain(UInt32 width, UInt32 height) {
        if (opengl_backend_) {
            opengl_backend_->updateFramebufferSize();
            swapchain_textures_.clear();
            swapchain_framebuffers_.clear();
            createSwapchainTexturesOpenGL();
            return !swapchain_framebuffers_.empty();
        }

        if (d3d12_backend_) {
            if (device_) {
                device_->waitForIdle();
            }

            swapchain_textures_.clear();
            swapchain_framebuffers_.clear();

            if (device_) {
                device_->runGarbageCollection();
                device_->waitForIdle();
            }

            if (!d3d12_backend_->recreateSwapchain(window_handle_, width, height)) {
                return false;
            }

            createSwapchainTexturesD3D12();
            if (device_) {
                device_->runGarbageCollection();
            }
            return !swapchain_framebuffers_.empty();
        }

        if (device_) {
            device_->waitForIdle();
        }

        swapchain_textures_.clear();
        swapchain_framebuffers_.clear();
        destroySwapchainSemaphores();
        if (device_) {
            device_->runGarbageCollection();
            device_->waitForIdle();
        }

        if (!vulkan_backend_->recreateSwapchain(window_handle_, width, height)) {
            return false;
        }

        createSwapchainSemaphores();
        createSwapchainTexturesVulkan();
        if (device_) {
            device_->runGarbageCollection();
        }
        return !swapchain_framebuffers_.empty();
    }

    void GfxContext::createSwapchainSemaphores() {
        if (!vulkan_backend_) {
            return;
        }

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkDevice vk_device = vulkan_backend_->getDevice();
        acquire_semaphores_.clear();
        present_semaphores_.clear();
        frame_fences_.clear();
        const Size_t frame_count = vulkan_backend_->getSwapchainImages().size();
        acquire_semaphores_.reserve(frame_count);
        present_semaphores_.reserve(frame_count);
        frame_fences_.reserve(frame_count);

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (Size_t i = 0; i < frame_count; ++i) {
            VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &acquire_semaphore) == VK_SUCCESS,
                "GfxContext::createSwapchainSemaphores: failed to create acquire semaphore.");
            acquire_semaphores_.push_back(acquire_semaphore);

            VkSemaphore present_semaphore = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &present_semaphore) == VK_SUCCESS,
                "GfxContext::createSwapchainSemaphores: failed to create present semaphore.");
            present_semaphores_.push_back(present_semaphore);

            VkFence frame_fence = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateFence(vk_device, &fence_info, nullptr, &frame_fence) == VK_SUCCESS,
                "GfxContext::createSwapchainSemaphores: failed to create frame fence.");
            frame_fences_.push_back(frame_fence);
        }

        current_frame_slot_ = 0;
        active_frame_slot_ = (std::numeric_limits<Size_t>::max)();
    }

    void GfxContext::destroySwapchainSemaphores() {
        if (!vulkan_backend_) {
            acquire_semaphores_.clear();
            present_semaphores_.clear();
            frame_fences_.clear();
            current_frame_slot_ = 0;
            active_frame_slot_ = (std::numeric_limits<Size_t>::max)();
            return;
        }

        VkDevice vk_device = vulkan_backend_->getDevice();
        if (vk_device != VK_NULL_HANDLE) {
            for (auto semaphore : acquire_semaphores_) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(vk_device, semaphore, nullptr);
                }
            }
            for (auto semaphore : present_semaphores_) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(vk_device, semaphore, nullptr);
                }
            }
            for (auto fence : frame_fences_) {
                if (fence != VK_NULL_HANDLE) {
                    vkDestroyFence(vk_device, fence, nullptr);
                }
            }
        }
        acquire_semaphores_.clear();
        present_semaphores_.clear();
        frame_fences_.clear();
        current_frame_slot_ = 0;
        active_frame_slot_ = (std::numeric_limits<Size_t>::max)();
    }

} // dodoe
