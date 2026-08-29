// do@Redlive

#pragma once

#include "dopch.h"

#include "backend/gfx_backend.h"
#include "backend/vulkan_backend.h"
#include "backend/opengl_backend.h"
#include "backend/d3d12_backend.h"

#include "gfx_viewport_surface.h"
#include "gfx.h"
#include "draw_command_list.h"
#include "runtime/function/render/render_settings.h"

struct GLFWwindow;

namespace dodoe {

    enum class RenderBackendApiType;

    class GfxRenderScope {
    public:
        GfxRenderScope();
        ~GfxRenderScope();
        GfxRenderScope(const GfxRenderScope&) = delete;
        GfxRenderScope& operator=(const GfxRenderScope&) = delete;
    };

    struct GfxContextCreateInfo {
        GLFWwindow* window_handle{nullptr};
        RenderBackendApiType api_type{};
        Bool enable_validation{true};
        RenderFeatureSettings feature_settings{};
        void* host_handle{nullptr};
        UInt32 width{0};
        UInt32 height{0};
    };

    class GfxContext {
        GfxDeviceHandle m_device{};
        GfxCommandListHandle m_cmd{};

        Scope<GfxViewportSurface> m_main_surface{nullptr};
        DynamicArray<Scope<GfxViewportSurface>> m_secondary_surfaces{};

        Scope<GfxBackend> m_backend{nullptr};
        GLFWwindow* m_window_handle{nullptr};
        void* m_host_handle{nullptr};
        RenderBackendApiType m_api_type{};
        Bool m_gpu_driven_supported{false};

    public:
        static Scope<GfxContext> Create(const GfxContextCreateInfo& create_info);
        static void Destroy(Scope<GfxContext>& backend);

        [[nodiscard]] GfxDeviceHandle getDevice() const { return m_device; }
        [[nodiscard]] const DynamicArray<GfxTextureHandle>& getSwapchainTextures() const;
        [[nodiscard]] GfxFramebufferHandle getSwapchainFramebuffer(UInt32 image_index) const;
        [[nodiscard]] Vector2i getSwapchainExtent2D() const;
        [[nodiscard]] Bool acquireNextSwapchainImage(UInt32& image_index);
        [[nodiscard]] Bool presentSwapchainImage(UInt32 image_index);
        [[nodiscard]] Bool recreateSwapchain(UInt32 width, UInt32 height);
        [[nodiscard]] Bool acquireOpenGLContext();
        void releaseOpenGLContext();
        [[nodiscard]] static Bool IsInRenderScope();
        [[nodiscard]] const GfxCommandListHandle& getCommandList() { return m_cmd; }

        [[nodiscard]] Bool isGpuDrivenSupported() const { return m_gpu_driven_supported; }
        [[nodiscard]] VulkanBackend* getVulkanBackend() const {
            return m_backend && m_api_type == RenderBackendApiType::Vulkan ? static_cast<VulkanBackend*>(m_backend.get()) : nullptr;
        }
        [[nodiscard]] OpenGLBackend* getOpenGLBackend() const {
            return m_backend && m_api_type == RenderBackendApiType::OpenGL ? static_cast<OpenGLBackend*>(m_backend.get()) : nullptr;
        }
        [[nodiscard]] D3D12Backend* getD3D12Backend() const {
            return m_backend && m_api_type == RenderBackendApiType::D3D12 ? static_cast<D3D12Backend*>(m_backend.get()) : nullptr;
        }
        [[nodiscard]] GfxBackend* getBackend() const { return m_backend.get(); }

        [[nodiscard]] GfxViewportSurface* createViewportSurface(GLFWwindow* window, UInt32 w, UInt32 h);
        void destroyViewportSurface(GfxViewportSurface* surface);
        [[nodiscard]] UInt32 getSecondarySurfaceCount() const { return static_cast<UInt32>(m_secondary_surfaces.size()); }

        void waitForIdle();
        void clearGarbage();

    private:
        friend class DrawCommandList;

        void initialize(const GfxContextCreateInfo& create_info);
        void shutdown();

        void initializeVulkan(const GfxContextCreateInfo& create_info);
        void initializeOpenGL(const GfxContextCreateInfo& create_info);
        void initializeD3D12(const GfxContextCreateInfo& create_info);
    };

} // dodoe
