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

    Scope<RhiContext> RhiContext::create(const RhiBackendCreateInfo& create_info) {
        auto context = create_scope<RhiContext>();
        context->initialize(create_info);
        return context;
    }

    void RhiContext::destroy(Scope<RhiContext>& backend) {
        if (!backend) return;
        backend->shutdown();
        backend.reset();
    }
    
    void RhiContext::initialize(const RhiBackendCreateInfo& create_info) {
        if (create_info.api_type != RenderApiType::Vulkan) {
            return;
        }

        window_handle_ = create_info.window_handle;

        vulkan_backend_ = VulkanBackend::create({create_info.window_handle, true});

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(vulkan_backend_->getInstance()));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device(vulkan_backend_->getDevice()));

        auto* error_callback = new RhiMessageCallback();

        rhi::vulkan::DeviceDesc device_desc;
        device_desc.errorCB = error_callback;
        device_desc.instance = vulkan_backend_->getInstance();
        device_desc.physicalDevice = vulkan_backend_->getPhysicalDevice();
        device_desc.device = vulkan_backend_->getDevice();
        device_desc.graphicsQueue = vulkan_backend_->getGraphicsQueue();
        device_desc.graphicsQueueIndex = vulkan_backend_->getGraphicsQueueIndex();
        device_desc.deviceExtensions = const_cast<const char**>(vulkan_backend_->getDeviceExtensions().data());
        device_desc.numDeviceExtensions = vulkan_backend_->getDeviceExtensions().size();

        device_ = rhi::vulkan::createDevice(device_desc);
        DO_ASSERT(device_ != nullptr, "RhiBackend::initialize: failed to create nvrhi vulkan device.");

        if (create_info.enable_validation) {
            device_ = rhi::validation::createValidationLayer(device_);
            DO_ASSERT(device_ != nullptr, "RhiBackend::initialize: failed to create validation layer.");
        }

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
        device_ = nullptr;
        VulkanBackend::destroy(vulkan_backend_);
    }

    void RhiContext::createSwapchainTextures() {
        if (!device_ || !vulkan_backend_) {
            return;
        }

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
        if (!vulkan_backend_) {
            return false;
        }
        return vulkan_backend_->acquireNextImage(image_index);
    }

    bool RhiContext::presentSwapchainImage(uint32_t image_index) {
        if (!vulkan_backend_) {
            return false;
        }
        return vulkan_backend_->presentImage(image_index);
    }

    bool RhiContext::recreateSwapchain() {
        if (!vulkan_backend_->recreateSwapchain(window_handle_)) {
            return false;
        }

        swapchain_textures_.clear();
        createSwapchainTextures();
        return !swapchain_textures_.empty();
    }

} // dodoe