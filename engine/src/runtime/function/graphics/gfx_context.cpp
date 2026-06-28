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
        }
    }

    void GfxContext::initializeVulkan(const GfxBackendCreateInfo& create_info) {
        OutputDebugStringA("[GFX] Vulkan initialize begin\n");

        vulkan_backend_ = VulkanBackend::Create({create_info.window_handle, create_info.host_handle, create_info.enable_validation});

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

        opengl_backend_ = OpenGLBackend::Create({create_info.window_handle});
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

    void GfxContext::shutdown() {
        cmd_ = nullptr;
        swapchain_textures_.clear();
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
    }

    void GfxContext::waitForIdle() {
        if (device_) device_->waitForIdle();
    }

    void GfxContext::clearGarbage() {
        if (device_) device_->runGarbageCollection();
    }

    Vector2i GfxContext::getSwapchainExtent2d() const {
        if (vulkan_backend_) {
            return vulkan_backend_->getSwapchainExtent2d();
        }
        if (opengl_backend_) {
            return opengl_backend_->getSwapchainExtent2d();
        }
        return Vector2i(0, 0);
    }

    void GfxContext::createSwapchainTexturesVulkan() {
        if (!device_ || !vulkan_backend_) {
            return;
        }

        swapchain_textures_.clear();
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

            GfxTextureHandle swapchain_texture = device_->createHandleForNativeTexture(GfxObjectTypes::VK_Image, image, texture_desc);
            swapchain_textures_.push_back(swapchain_texture);
        }
    }

    void GfxContext::createSwapchainTexturesOpenGL() {
        if (!device_ || !opengl_backend_) {
            return;
        }

        swapchain_textures_.clear();
        const auto extent = opengl_backend_->getSwapchainExtent2d();

        auto texture_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::Texture2D)
            .setFormat(GfxFormat::RGBA8_UNORM)
            .setWidth(extent.x)
            .setHeight(extent.y)
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(GfxResourceStates::Present)
            .setDebugName("GL Backbuffer");

        GfxTextureHandle backbuffer = device_->createTexture(texture_desc);
        swapchain_textures_.push_back(backbuffer);
    }

    Bool GfxContext::acquireNextSwapchainImage(UInt32& image_index) {
        if (opengl_backend_) {
            image_index = 0;
            opengl_backend_->updateFramebufferSize();
            return true;
        }

        if (!vulkan_backend_ || !device_ || acquire_semaphores_.empty() || frame_fences_.empty()) {
            DO_DEBUG("GfxContext::acquireNextSwapchainImage: early return false (missing objects)");
            return false;
        }

        auto* vk_device = static_cast<vulkan::IDevice*>(
            device_->getNativeObject(GfxObjectTypes::VK_Device));
        DO_ASSERT(vk_device != nullptr, "GfxContext::acquireNextSwapchainImage: failed to get native cutie vulkan device.");

        try {
            auto test_ptr = reinterpret_cast<void**>(vk_device);
            volatile void* vtable = test_ptr[0];
            (void)vtable;
            DO_DEBUG("GfxContext::acquireNextSwapchainImage: vk_device vtable={}", vtable);
        } catch (...) {
            DO_ERROR("GfxContext::acquireNextSwapchainImage: vk_device pointer is invalid!");
            return false;
        }

        const Size_t frame_slot = current_frame_slot_ % acquire_semaphores_.size();
        VkDevice vk_device_handle = vulkan_backend_->getDevice();
        DO_DEBUG("GfxContext::acquireNextSwapchainImage: waiting for fence, frame_slot={}", frame_slot);
        DO_ASSERT(vkWaitForFences(vk_device_handle, 1, &frame_fences_[frame_slot], VK_TRUE, UINT64_MAX) == VK_SUCCESS,
            "GfxContext::acquireNextSwapchainImage: failed to wait for frame fence.");
        DO_DEBUG("GfxContext::acquireNextSwapchainImage: fence wait done");

        const VkSemaphore acquire_semaphore = acquire_semaphores_[frame_slot];
        DO_DEBUG("GfxContext::acquireNextSwapchainImage: calling vulkan_backend_->acquireNextImage...");
        if (!vulkan_backend_->acquireNextImage(image_index, acquire_semaphore)) {
            DO_DEBUG("GfxContext::acquireNextSwapchainImage: acquireNextImage returned false");
            return false;
        }
        DO_DEBUG("GfxContext::acquireNextSwapchainImage: acquireNextImage succeeded, image_index={}", image_index);
        DO_DEBUG("GfxContext::acquireNextSwapchainImage: vk_device={}, acquire_semaphore={}",
                 (void*)vk_device, (void*)acquire_semaphore);

        if (!vk_device) {
            DO_ERROR("GfxContext::acquireNextSwapchainImage: vk_device is NULL!");
            return false;
        }
        if (!acquire_semaphore) {
            DO_ERROR("GfxContext::acquireNextSwapchainImage: acquire_semaphore is NULL!");
            return false;
        }

        DO_DEBUG("GfxContext::acquireNextSwapchainImage: about to call queueWaitForSemaphore with queue=Graphics(0)");
        vk_device->queueWaitForSemaphore(GfxCommandQueue::Graphics, acquire_semaphore, 0);
        DO_DEBUG("GfxContext::acquireNextSwapchainImage: queueWaitForSemaphore completed successfully");
        active_frame_slot_ = frame_slot;

        DO_DEBUG("GfxContext::acquireNextSwapchainImage: returning true");
        return true;
    }

    Bool GfxContext::presentSwapchainImage(UInt32 image_index) {
        if (opengl_backend_) {
            if (opengl_backend_->getWindow()) {
                glfwSwapBuffers(opengl_backend_->getWindow());
            }
            return true;
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

    Bool GfxContext::recreateSwapchain() {
        if (opengl_backend_) {
            opengl_backend_->updateFramebufferSize();
            swapchain_textures_.clear();
            createSwapchainTexturesOpenGL();
            return !swapchain_textures_.empty();
        }

        if (device_) {
            device_->waitForIdle();
        }

        swapchain_textures_.clear();
        destroySwapchainSemaphores();
        if (device_) {
            device_->runGarbageCollection();
            device_->waitForIdle();
        }

        if (!vulkan_backend_->recreateSwapchain(window_handle_)) {
            return false;
        }

        createSwapchainSemaphores();
        createSwapchainTexturesVulkan();
        if (device_) {
            device_->runGarbageCollection();
        }
        return !swapchain_textures_.empty();
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
