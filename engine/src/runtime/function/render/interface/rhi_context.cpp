// 
// Created by Redlive on 2026/4/5.
//

#include "rhi_context.h"

#include "../render_api.h"

#include "vulkan/vulkan.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace dodoe {

    namespace {

        rhi::Format to_nvrhi_format(VkFormat format) {
            switch (format) {
                case VK_FORMAT_B8G8R8A8_UNORM: return rhi::Format::BGRA8_UNORM;
                case VK_FORMAT_B8G8R8A8_SRGB: return rhi::Format::SBGRA8_UNORM;
                case VK_FORMAT_R8G8B8A8_UNORM: return rhi::Format::RGBA8_UNORM;
                case VK_FORMAT_R8G8B8A8_SRGB: return rhi::Format::SRGBA8_UNORM;
                default: return rhi::Format::BGRA8_UNORM;
            }
        }

        class RhiMessageCallback : public rhi::IMessageCallback {
        public:
            void message(rhi::MessageSeverity severity, const char* message_text) override {
                if (severity == rhi::MessageSeverity::Info || severity == rhi::MessageSeverity::Warning) return;
                DO_ERROR("RHI::ERROR: {}", message_text);
            }
        };

    }

    Scope<RhiContext> RhiContext::Create(const RhiBackendCreateInfo& create_info) {
        auto context = create_scope<RhiContext>();
        context->initialize(create_info);
        return context;
    }

    void RhiContext::Destroy(Scope<RhiContext>& backend) {
        if (!backend) return;
        backend->shutdown();
        backend.reset();
    }
    
    void RhiContext::initialize(const RhiBackendCreateInfo& create_info) {
        if (create_info.api_type != RenderApiType::Vulkan) {
            return;
        }

        window_handle_ = create_info.window_handle;

        vulkan_backend_ = VulkanBackend::create({create_info.window_handle, create_info.enable_validation});

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(vulkan_backend_->getInstance()));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device(vulkan_backend_->getDevice()));

        auto* error_callback = new RhiMessageCallback();

        rhi::vulkan::DeviceDesc device_desc{};
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

        device_ = rhi::vulkan::createDevice(device_desc);
        DO_ASSERT(device_ != nullptr, "RhiBackend::initialize: failed to create nvrhi vulkan device.");

        if (create_info.enable_validation) {
            device_ = rhi::validation::createValidationLayer(device_);
            DO_ASSERT(device_ != nullptr, "RhiBackend::initialize: failed to create validation layer.");
        }

        createSwapchainSemaphores();
        createSwapchainTextures();
        cmd_ = device_->createCommandList();
    }

    void RhiContext::shutdown() {
		cmd_ = nullptr;
        swapchain_textures_.clear();
		if (device_) {
			device_->waitForIdle();
			device_->runGarbageCollection();
		}
        destroySwapchainSemaphores();
        device_ = nullptr;
        VulkanBackend::destroy(vulkan_backend_);
    }

    void RhiContext::createSwapchainTextures() {
        if (!device_ || !vulkan_backend_) {
            return;
        }

        swapchain_textures_.clear();
        const auto swapchain_format = to_nvrhi_format(vulkan_backend_->getSwapchainImageFormat());

        for (const auto& image : vulkan_backend_->getSwapchainImages()) {
            auto texture_desc = rhi::TextureDesc()
                .setDimension(rhi::TextureDimension::Texture2D)
                .setFormat(swapchain_format)
                .setWidth(vulkan_backend_->getSwapchainExtent2d().x)
                .setHeight(vulkan_backend_->getSwapchainExtent2d().y)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(rhi::ResourceStates::Present)
                .setDebugName("Swapchain Image");

            rhi::TextureHandle swapchain_texture = device_->createHandleForNativeTexture(rhi::ObjectTypes::VK_Image, image, texture_desc);
            swapchain_textures_.push_back(swapchain_texture);
        }
    }

    bool RhiContext::acquireNextSwapchainImage(uint32_t& image_index) {
        if (!vulkan_backend_ || !device_ || acquire_semaphores_.empty() || frame_fences_.empty()) {
            return false;
        }

        auto* vk_device = static_cast<rhi::vulkan::IDevice*>(
            device_->getNativeObject(rhi::ObjectTypes::Nvrhi_VK_Device));
        DO_ASSERT(vk_device != nullptr, "RhiContext::acquireNextSwapchainImage: failed to get native nvrhi vulkan device.");
        const size_t frame_slot = current_frame_slot_ % acquire_semaphores_.size();
        VkDevice vk_device_handle = vulkan_backend_->getDevice();
        DO_ASSERT(vkWaitForFences(vk_device_handle, 1, &frame_fences_[frame_slot], VK_TRUE, UINT64_MAX) == VK_SUCCESS,
            "RhiContext::acquireNextSwapchainImage: failed to wait for frame fence.");

        const VkSemaphore acquire_semaphore = acquire_semaphores_[frame_slot];
        if (!vulkan_backend_->acquireNextImage(image_index, acquire_semaphore)) {
            return false;
        }
        vk_device->queueWaitForSemaphore(rhi::CommandQueue::Graphics, acquire_semaphore, 0);
        active_frame_slot_ = frame_slot;

        return true;
    }

    bool RhiContext::presentSwapchainImage(uint32_t image_index) {
        if (!vulkan_backend_ || !device_) {
            return false;
        }

        if (active_frame_slot_ >= frame_fences_.size() || image_index >= present_semaphores_.size()) {
            DO_ASSERT(false, "RhiContext::presentSwapchainImage: invalid swapchain image index.");
            return false;
        }

        VkSemaphore present_semaphore = present_semaphores_[image_index];
        VkFence frame_fence = frame_fences_[active_frame_slot_];
        VkDevice vk_device_handle = vulkan_backend_->getDevice();
        DO_ASSERT(vkResetFences(vk_device_handle, 1, &frame_fence) == VK_SUCCESS,
            "RhiContext::presentSwapchainImage: failed to reset frame fence.");
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 0;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &present_semaphore;
        DO_ASSERT(vkQueueSubmit(vulkan_backend_->getGraphicsQueue(), 1, &submit_info, frame_fence) == VK_SUCCESS,
            "RhiContext::presentSwapchainImage: failed to submit present signal semaphore.");

        const bool present_result = vulkan_backend_->presentImage(image_index, present_semaphore);
        current_frame_slot_ = (active_frame_slot_ + 1) % acquire_semaphores_.size();
        active_frame_slot_ = (std::numeric_limits<size_t>::max)();
        return present_result;
    }

    bool RhiContext::recreateSwapchain() {
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
        createSwapchainTextures();
        if (device_) {
            device_->runGarbageCollection();
        }
        return !swapchain_textures_.empty();
    }

    void RhiContext::createSwapchainSemaphores() {
        DO_ASSERT(vulkan_backend_ != nullptr, "RhiContext::createSwapchainSemaphores: vulkan backend is null.");

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkDevice vk_device = vulkan_backend_->getDevice();
        acquire_semaphores_.clear();
        present_semaphores_.clear();
        frame_fences_.clear();
        const size_t frame_count = vulkan_backend_->getSwapchainImages().size();
        acquire_semaphores_.reserve(frame_count);
        present_semaphores_.reserve(frame_count);
        frame_fences_.reserve(frame_count);

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < frame_count; ++i) {
            VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &acquire_semaphore) == VK_SUCCESS,
                "RhiContext::createSwapchainSemaphores: failed to create acquire semaphore.");
            acquire_semaphores_.push_back(acquire_semaphore);

            VkSemaphore present_semaphore = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &present_semaphore) == VK_SUCCESS,
                "RhiContext::createSwapchainSemaphores: failed to create present semaphore.");
            present_semaphores_.push_back(present_semaphore);

            VkFence frame_fence = VK_NULL_HANDLE;
            DO_ASSERT(vkCreateFence(vk_device, &fence_info, nullptr, &frame_fence) == VK_SUCCESS,
                "RhiContext::createSwapchainSemaphores: failed to create frame fence.");
            frame_fences_.push_back(frame_fence);
        }

        current_frame_slot_ = 0;
        active_frame_slot_ = (std::numeric_limits<size_t>::max)();
    }

    void RhiContext::destroySwapchainSemaphores() {
        if (!vulkan_backend_) {
            acquire_semaphores_.clear();
            present_semaphores_.clear();
            frame_fences_.clear();
            current_frame_slot_ = 0;
            active_frame_slot_ = (std::numeric_limits<size_t>::max)();
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
        active_frame_slot_ = (std::numeric_limits<size_t>::max)();
    }

} // dodoe
