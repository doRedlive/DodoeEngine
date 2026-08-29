// do@Redlive

#include "gfx_context.h"

#include "../render/render_settings.h"

#include "vulkan/vulkan.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace dodoe {

    namespace {

        thread_local Bool t_in_render_scope{false};

        class RhiMessageCallback : public GfxMessageCallback {
        public:
            void message(GfxMessageSeverity severity, const char* message_text) override {
                const char* text = message_text ? message_text : "(empty message)";
                switch (severity) {
                case GfxMessageSeverity::Info:
                    DO_INFO("RHI: {}", text);
                    break;
                case GfxMessageSeverity::Warning:
                    DO_WARN("RHI: {}", text);
                    break;
                case GfxMessageSeverity::Error:
                default:
                    DO_ERROR("RHI: {}", text);
                    break;
                }
            }
        };

    }

    GfxRenderScope::GfxRenderScope() { t_in_render_scope = true; }
    GfxRenderScope::~GfxRenderScope() { t_in_render_scope = false; }

    Bool GfxContext::IsInRenderScope() { return t_in_render_scope; }

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
        m_window_handle = create_info.window_handle;
        m_host_handle = create_info.host_handle;
        m_api_type = create_info.api_type;
        DO_INFO("GfxContext: initializing backend {} (validation={}, extent={}x{})",
            RenderSettings::GetRenderBackendApiTypeStr(), create_info.enable_validation,
            create_info.width, create_info.height);

        if (create_info.api_type == RenderBackendApiType::Vulkan) {
            initializeVulkan(create_info);
        } else if (create_info.api_type == RenderBackendApiType::OpenGL) {
            initializeOpenGL(create_info);
        } else if (create_info.api_type == RenderBackendApiType::D3D12) {
            initializeD3D12(create_info);
        } else {
            DO_ERROR("GfxContext: unsupported render backend API ({})", static_cast<int>(create_info.api_type));
        }

        if (m_device) {
            DeviceCapabilities caps{};
            caps.bindless_supported = m_device->queryFeatureSupport(cutie::Feature::HeapDirectlyIndexed);
            caps.compute_queue_supported = m_device->queryFeatureSupport(cutie::Feature::ComputeQueue);
            RenderSettings::SetDeviceCapabilities(caps);
            DO_INFO("GfxContext: device capabilities (bindless={}, compute_queue={})",
                caps.bindless_supported, caps.compute_queue_supported);
        } else {
            DO_ERROR("GfxContext: graphics device creation failed");
        }
        RenderSettings::ResolveFeatures(create_info.feature_settings);
        m_gpu_driven_supported = RenderSettings::GetResolvedFeatures().gpu_driven_active;
        DO_INFO("GfxContext: resolved gpu-driven rendering={}", m_gpu_driven_supported);
    }

    void GfxContext::initializeVulkan(const GfxContextCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::initializeVulkan", "startup");
        DO_PROFILE_MARK("GfxContext::initializeVulkan.createBackend", "startup");
        OutputDebugStringA("[GFX] Vulkan initialize begin\n");

        m_backend = GfxBackend::Create({create_info.window_handle, create_info.host_handle, RenderBackendApiType::Vulkan, create_info.enable_validation, create_info.width, create_info.height});
        DO_ASSERT(m_backend != nullptr, "GfxContext::initializeVulkan: failed to create Vulkan backend.");
        auto* vulkan_backend = static_cast<VulkanBackend*>(m_backend.get());

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

        m_device = vulkan::createDevice(device_desc);
        DO_ASSERT(m_device != nullptr, "GfxBackend::initializeVulkan: failed to create cutie vulkan device.");
        DO_INFO("GfxContext: Vulkan device created");

        if (create_info.enable_validation) {
            m_device = validation::createValidationLayer(m_device);
            DO_ASSERT(m_device != nullptr, "GfxBackend::initializeVulkan: failed to create validation layer.");
            DO_INFO("GfxContext: Vulkan validation layer enabled");
        }

        m_main_surface = create_scope<GfxViewportSurface>();
        DO_ASSERT(m_main_surface->initialize(m_device, m_window_handle, m_host_handle,
            create_info.width, create_info.height, RenderBackendApiType::Vulkan,
            m_backend.get(), true),
            "GfxContext::initializeVulkan: failed to initialize main viewport surface.");
        m_cmd = m_device->createCommandList();
        DO_ASSERT(m_cmd != nullptr, "GfxContext::initializeVulkan: failed to create command list.");

        OutputDebugStringA("[GFX] Vulkan initialize done\n");
    }

    void GfxContext::initializeOpenGL(const GfxContextCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::initializeOpenGL", "startup");
        DO_PROFILE_MARK("GfxContext::initializeOpenGL.createBackend", "startup");
        OutputDebugStringA("[GFX] OpenGL initialize begin\n");

        m_backend = GfxBackend::Create({create_info.window_handle, nullptr, RenderBackendApiType::OpenGL, create_info.enable_validation, create_info.width, create_info.height});
        DO_ASSERT(m_backend != nullptr, "GfxContext::initializeOpenGL: failed to create OpenGL backend.");
        auto* opengl_backend = static_cast<OpenGLBackend*>(m_backend.get());

        auto* error_callback = new RhiMessageCallback();

        opengl::DeviceDesc device_desc{};
        device_desc.messageCallback = error_callback;
        device_desc.glLoaderFunc = reinterpret_cast<opengl::GLloaderFunc>(glfwGetProcAddress);

        m_device = opengl::createDevice(device_desc);
        DO_ASSERT(m_device != nullptr, "GfxBackend::initializeOpenGL: failed to create cutie opengl device.");
        DO_INFO("GfxContext: OpenGL device created");

        m_main_surface = create_scope<GfxViewportSurface>();
        DO_ASSERT(m_main_surface->initialize(m_device, m_window_handle, m_host_handle,
            create_info.width, create_info.height, RenderBackendApiType::OpenGL,
            m_backend.get(), true),
            "GfxContext::initializeOpenGL: failed to initialize main viewport surface.");
        m_cmd = m_device->createCommandList();
        DO_ASSERT(m_cmd != nullptr, "GfxContext::initializeOpenGL: failed to create command list.");

        OutputDebugStringA("[GFX] OpenGL initialize done\n");
    }

    void GfxContext::initializeD3D12(const GfxContextCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::initializeD3D12", "startup");
        DO_PROFILE_MARK("GfxContext::initializeD3D12.createBackend", "startup");
        OutputDebugStringA("[GFX] D3D12 initialize begin\n");

        m_backend = GfxBackend::Create({create_info.window_handle, create_info.host_handle, RenderBackendApiType::D3D12, create_info.enable_validation, create_info.width, create_info.height});
        DO_ASSERT(m_backend != nullptr, "GfxContext::initializeD3D12: failed to create D3D12 backend.");
        auto* d3d12_backend = static_cast<D3D12Backend*>(m_backend.get());

        auto* error_callback = new RhiMessageCallback();

        d3d12::DeviceDesc device_desc{};
        device_desc.errorCB = error_callback;
        device_desc.pDevice = d3d12_backend->getDevice();
        device_desc.pGraphicsCommandQueue = d3d12_backend->getGraphicsQueue();
        device_desc.pComputeCommandQueue = d3d12_backend->getComputeQueue();
        device_desc.pCopyCommandQueue = d3d12_backend->getCopyQueue();

        m_device = d3d12::createDevice(device_desc);
        DO_ASSERT(m_device != nullptr, "GfxBackend::initializeD3D12: failed to create cutie d3d12 device.");
        DO_INFO("GfxContext: D3D12 device created");

        if (create_info.enable_validation) {
            m_device = validation::createValidationLayer(m_device);
            DO_ASSERT(m_device != nullptr, "GfxBackend::initializeD3D12: failed to create validation layer.");
            DO_INFO("GfxContext: D3D12 validation layer enabled");
        }

        m_main_surface = create_scope<GfxViewportSurface>();
        DO_ASSERT(m_main_surface->initialize(m_device, m_window_handle, m_host_handle,
            create_info.width, create_info.height, RenderBackendApiType::D3D12,
            m_backend.get(), true),
            "GfxContext::initializeD3D12: failed to initialize main viewport surface.");
        m_cmd = m_device->createCommandList();
        DO_ASSERT(m_cmd != nullptr, "GfxContext::initializeD3D12: failed to create command list.");

        OutputDebugStringA("[GFX] D3D12 initialize done\n");
    }

    void GfxContext::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::shutdown", "shutdown");
        DO_INFO("GfxContext: shutting down");
        m_cmd = nullptr;
        m_secondary_surfaces.clear();
        if (m_main_surface) {
            m_main_surface->shutdown();
            m_main_surface.reset();
        }
        GDrawCommandList.shutdown();
        if (m_device) {
            m_device->waitForIdle();
            m_device->runGarbageCollection();
        }
        m_device = nullptr;

        if (m_backend) {
            GfxBackend::Destroy(m_backend);
        }
    }

    void GfxContext::waitForIdle() {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::waitForIdle", "synchronization");
        if (m_device) m_device->waitForIdle();
    }

    void GfxContext::clearGarbage() {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::clearGarbage", "synchronization");
        if (m_device) m_device->runGarbageCollection();
    }

    Bool GfxContext::acquireOpenGLContext() {
        return m_api_type != RenderBackendApiType::OpenGL || static_cast<OpenGLBackend*>(m_backend.get())->acquireContext();
    }

    void GfxContext::releaseOpenGLContext() {
        if (m_api_type == RenderBackendApiType::OpenGL) static_cast<OpenGLBackend*>(m_backend.get())->releaseContext();
    }

    const DynamicArray<GfxTextureHandle>& GfxContext::getSwapchainTextures() const {
        static const DynamicArray<GfxTextureHandle> kEmptyTextures{};
        return m_main_surface ? m_main_surface->getTextures() : kEmptyTextures;
    }

    GfxFramebufferHandle GfxContext::getSwapchainFramebuffer(UInt32 image_index) const {
        return m_main_surface ? m_main_surface->getFramebuffer(image_index) : nullptr;
    }

    Vector2i GfxContext::getSwapchainExtent2D() const {
        return m_main_surface ? m_main_surface->extent() : Vector2i(0, 0);
    }

    Bool GfxContext::acquireNextSwapchainImage(UInt32& image_index) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::acquireNextSwapchainImage", "swapchain");
        return m_main_surface ? m_main_surface->acquire(image_index) : false;
    }

    Bool GfxContext::presentSwapchainImage(UInt32 image_index) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::presentSwapchainImage", "swapchain");
        return m_main_surface ? m_main_surface->present(image_index) : false;
    }

    Bool GfxContext::recreateSwapchain(UInt32 width, UInt32 height) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::recreateSwapchain", "swapchain");
        DO_INFO("GfxContext: recreating swapchain ({}x{})", width, height);
        return m_main_surface ? m_main_surface->resize(width, height) : false;
    }

    GfxViewportSurface* GfxContext::createViewportSurface(GLFWwindow* window, UInt32 w, UInt32 h) {
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::createViewportSurface", "swapchain");
        auto surface = create_scope<GfxViewportSurface>();
        if (!surface->initialize(m_device, window, nullptr, w, h, m_api_type, m_backend.get(), false)) {
            DO_ERROR("GfxContext: failed to create secondary viewport surface ({}x{})", w, h);
            return nullptr;
        }
        m_secondary_surfaces.push_back(std::move(surface));
        DO_INFO("GfxContext: created secondary viewport surface ({}x{})", w, h);
        return m_secondary_surfaces.back().get();
    }

    void GfxContext::destroyViewportSurface(GfxViewportSurface* surface) {
        if (!surface) return;
        DO_PROFILE_SCOPE_CATEGORY("GfxContext::destroyViewportSurface", "swapchain");
        for (auto it = m_secondary_surfaces.begin(); it != m_secondary_surfaces.end(); ++it) {
            if (it->get() == surface) {
                auto scope = extract_scope(m_secondary_surfaces, it);
                scope->shutdown();
                DO_INFO("GfxContext: destroyed secondary viewport surface");
                return;
            }
        }
        DO_WARN("GfxContext: requested viewport surface was not owned by this context");
    }

} // dodoe
