// do@Redlive

#include "gfx_context.h"

#include "../render/render_settings.h"

#include "vulkan/vulkan.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace dodoe {

    namespace {

        class RhiMessageCallback : public GfxMessageCallback {
        public:
            void message(GfxMessageSeverity severity, const char* message_text) override {
                if (severity == GfxMessageSeverity::Info || severity == GfxMessageSeverity::Warning) return;
                DO_ERROR("RHI::ERROR: {}", message_text);
            }
        };

    }

    Scope<GfxContext> GfxContext::Create(const GfxContextCreateInfo& create_info) {
        auto context = create_scope<GfxContext>();
        context->initialize(create_info);
        return context;
    }

    void GfxContext::Destroy(Scope<GfxContext>& backend) {
        if (!backend) return;
        backend->shutdown();
        backend.reset();
    }

    void GfxContext::initialize(const GfxContextCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::initialize", "startup");
        window_handle_ = create_info.window_handle;
        host_handle_ = create_info.host_handle;
        m_api_type_ = create_info.api_type;

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

    void GfxContext::initializeVulkan(const GfxContextCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::initializeVulkan", "startup");
        OutputDebugStringA("[GFX] Vulkan initialize begin\n");

        backend_ = GfxBackend::Create({create_info.window_handle, create_info.host_handle, RenderBackendApiType::Vulkan, create_info.enable_validation, create_info.width, create_info.height});
        DO_ASSERT(backend_ != nullptr, "GfxContext::initializeVulkan: failed to create Vulkan backend.");
        auto* vulkan_backend = static_cast<VulkanBackend*>(backend_.get());

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(vulkan_backend->getInstance()));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device(vulkan_backend->getDevice()));

        auto* error_callback = new RhiMessageCallback();

        vulkan::DeviceDesc device_desc{};
        device_desc.errorCB = error_callback;
        device_desc.instance = vulkan_backend->getInstance();
        device_desc.physicalDevice = vulkan_backend->getPhysicalDevice();
        device_desc.device = vulkan_backend->getDevice();
        device_desc.graphicsQueue = vulkan_backend->getGraphicsQueue();
        device_desc.graphicsQueueIndex = vulkan_backend->getGraphicsQueueIndex();
        device_desc.computeQueue = vulkan_backend->getComputeQueue();
        device_desc.computeQueueIndex = vulkan_backend->getComputeQueueIndex();
        device_desc.transferQueue = vulkan_backend->getGraphicsQueue();
        device_desc.transferQueueIndex = vulkan_backend->getGraphicsQueueIndex();
        device_desc.instanceExtensions = const_cast<const char**>(vulkan_backend->getInstanceExtensions().data());
        device_desc.numInstanceExtensions = vulkan_backend->getInstanceExtensions().size();
        device_desc.deviceExtensions = const_cast<const char**>(vulkan_backend->getDeviceExtensions().data());
        device_desc.numDeviceExtensions = vulkan_backend->getDeviceExtensions().size();
        device_desc.bufferDeviceAddressSupported = true;

        device_ = vulkan::createDevice(device_desc);
        DO_ASSERT(device_ != nullptr, "GfxBackend::initializeVulkan: failed to create cutie vulkan device.");

        if (create_info.enable_validation) {
            device_ = validation::createValidationLayer(device_);
            DO_ASSERT(device_ != nullptr, "GfxBackend::initializeVulkan: failed to create validation layer.");
        }

        m_main_surface_ = create_scope<GfxViewportSurface>();
        DO_ASSERT(m_main_surface_->initialize(device_, window_handle_, host_handle_,
            create_info.width, create_info.height, RenderBackendApiType::Vulkan,
            backend_.get(), true),
            "GfxContext::initializeVulkan: failed to initialize main viewport surface.");
        cmd_ = device_->createCommandList();

        OutputDebugStringA("[GFX] Vulkan initialize done\n");
    }

    void GfxContext::initializeOpenGL(const GfxContextCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::initializeOpenGL", "startup");
        OutputDebugStringA("[GFX] OpenGL initialize begin\n");

        backend_ = GfxBackend::Create({create_info.window_handle, nullptr, RenderBackendApiType::OpenGL, create_info.enable_validation, create_info.width, create_info.height});
        DO_ASSERT(backend_ != nullptr, "GfxContext::initializeOpenGL: failed to create OpenGL backend.");
        auto* opengl_backend = static_cast<OpenGLBackend*>(backend_.get());

        auto* error_callback = new RhiMessageCallback();

        opengl::DeviceDesc device_desc{};
        device_desc.messageCallback = error_callback;
        device_desc.glLoaderFunc = reinterpret_cast<opengl::GLloaderFunc>(glfwGetProcAddress);

        device_ = opengl::createDevice(device_desc);
        DO_ASSERT(device_ != nullptr, "GfxBackend::initializeOpenGL: failed to create cutie opengl device.");

        m_main_surface_ = create_scope<GfxViewportSurface>();
        DO_ASSERT(m_main_surface_->initialize(device_, window_handle_, host_handle_,
            create_info.width, create_info.height, RenderBackendApiType::OpenGL,
            backend_.get(), true),
            "GfxContext::initializeOpenGL: failed to initialize main viewport surface.");
        cmd_ = device_->createCommandList();

        OutputDebugStringA("[GFX] OpenGL initialize done\n");
    }

    void GfxContext::initializeD3D12(const GfxContextCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::initializeD3D12", "startup");
        OutputDebugStringA("[GFX] D3D12 initialize begin\n");

        backend_ = GfxBackend::Create({create_info.window_handle, create_info.host_handle, RenderBackendApiType::D3D12, create_info.enable_validation, create_info.width, create_info.height});
        DO_ASSERT(backend_ != nullptr, "GfxContext::initializeD3D12: failed to create D3D12 backend.");
        auto* d3d12_backend = static_cast<D3D12Backend*>(backend_.get());

        auto* error_callback = new RhiMessageCallback();

        d3d12::DeviceDesc device_desc{};
        device_desc.errorCB = error_callback;
        device_desc.pDevice = d3d12_backend->getDevice();
        device_desc.pGraphicsCommandQueue = d3d12_backend->getGraphicsQueue();
        device_desc.pComputeCommandQueue = d3d12_backend->getComputeQueue();
        device_desc.pCopyCommandQueue = d3d12_backend->getCopyQueue();

        device_ = d3d12::createDevice(device_desc);
        DO_ASSERT(device_ != nullptr, "GfxBackend::initializeD3D12: failed to create cutie d3d12 device.");

        if (create_info.enable_validation) {
            device_ = validation::createValidationLayer(device_);
            DO_ASSERT(device_ != nullptr, "GfxBackend::initializeD3D12: failed to create validation layer.");
        }

        m_main_surface_ = create_scope<GfxViewportSurface>();
        DO_ASSERT(m_main_surface_->initialize(device_, window_handle_, host_handle_,
            create_info.width, create_info.height, RenderBackendApiType::D3D12,
            backend_.get(), true),
            "GfxContext::initializeD3D12: failed to initialize main viewport surface.");
        cmd_ = device_->createCommandList();

        OutputDebugStringA("[GFX] D3D12 initialize done\n");
    }

    void GfxContext::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::shutdown", "shutdown");
        cmd_ = nullptr;
        m_secondary_surfaces_.clear();
        if (m_main_surface_) {
            m_main_surface_->shutdown();
            m_main_surface_.reset();
        }
        if (device_) {
            device_->waitForIdle();
            device_->runGarbageCollection();
        }
        device_ = nullptr;

        if (backend_) {
            GfxBackend::Destroy(backend_);
        }
    }

    void GfxContext::waitForIdle() {
        if (device_) device_->waitForIdle();
    }

    void GfxContext::clearGarbage() {
        if (device_) device_->runGarbageCollection();
    }

    Bool GfxContext::acquireOpenGLContext() {
        return m_api_type_ != RenderBackendApiType::OpenGL || static_cast<OpenGLBackend*>(backend_.get())->acquireContext();
    }

    void GfxContext::releaseOpenGLContext() {
        if (m_api_type_ == RenderBackendApiType::OpenGL) static_cast<OpenGLBackend*>(backend_.get())->releaseContext();
    }

    const DynamicArray<GfxTextureHandle>& GfxContext::getSwapchainTextures() const {
        static const DynamicArray<GfxTextureHandle> kEmptyTextures{};
        return m_main_surface_ ? m_main_surface_->getTextures() : kEmptyTextures;
    }

    GfxFramebufferHandle GfxContext::getSwapchainFramebuffer(UInt32 image_index) const {
        return m_main_surface_ ? m_main_surface_->getFramebuffer(image_index) : nullptr;
    }

    Vector2i GfxContext::getSwapchainExtent2d() const {
        return m_main_surface_ ? m_main_surface_->extent() : Vector2i(0, 0);
    }

    Bool GfxContext::acquireNextSwapchainImage(UInt32& image_index) {
        return m_main_surface_ ? m_main_surface_->acquire(image_index) : false;
    }

    Bool GfxContext::presentSwapchainImage(UInt32 image_index) {
        return m_main_surface_ ? m_main_surface_->present(image_index) : false;
    }

    Bool GfxContext::recreateSwapchain(UInt32 width, UInt32 height) {
        return m_main_surface_ ? m_main_surface_->resize(width, height) : false;
    }

    GfxViewportSurface* GfxContext::createViewportSurface(GLFWwindow* window, UInt32 w, UInt32 h) {
        auto surface = create_scope<GfxViewportSurface>();
        if (!surface->initialize(device_, window, nullptr, w, h, m_api_type_, backend_.get(), false)) {
            return nullptr;
        }
        m_secondary_surfaces_.push_back(std::move(surface));
        return m_secondary_surfaces_.back().get();
    }

    void GfxContext::destroyViewportSurface(GfxViewportSurface* surface) {
        if (!surface) return;
        for (auto it = m_secondary_surfaces_.begin(); it != m_secondary_surfaces_.end(); ++it) {
            if (it->get() == surface) {
                auto scope = extract_scope(m_secondary_surfaces_, it);
                scope->shutdown();
                return;
            }
        }
    }

} // dodoe
