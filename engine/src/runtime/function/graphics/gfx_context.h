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
        GfxDeviceHandle device_{};
        GfxCommandListHandle cmd_{};

        Scope<GfxViewportSurface> m_main_surface_{nullptr};
        DynamicArray<Scope<GfxViewportSurface>> m_secondary_surfaces_{};

        Scope<GfxBackend> backend_{nullptr};
        GLFWwindow* window_handle_{nullptr};
        void* host_handle_{nullptr};
        RenderBackendApiType m_api_type_{};
        Bool m_gpu_driven_supported{false};

    public:
        static Scope<GfxContext> Create(const GfxContextCreateInfo& create_info);
        static void Destroy(Scope<GfxContext>& backend);

        [[nodiscard]] GfxDeviceHandle getDevice() const { return device_; }
        [[nodiscard]] const DynamicArray<GfxTextureHandle>& getSwapchainTextures() const;
        [[nodiscard]] GfxFramebufferHandle getSwapchainFramebuffer(UInt32 image_index) const;
        [[nodiscard]] Vector2i getSwapchainExtent2d() const;
        [[nodiscard]] Bool acquireNextSwapchainImage(UInt32& image_index);
        [[nodiscard]] Bool presentSwapchainImage(UInt32 image_index);
        [[nodiscard]] Bool recreateSwapchain(UInt32 width, UInt32 height);
        [[nodiscard]] Bool acquireOpenGLContext();
        void releaseOpenGLContext();
        [[nodiscard]] static Bool inRenderScope();
        [[nodiscard]] const GfxCommandListHandle& getCommandList() { return cmd_; }

        [[nodiscard]] Bool isGpuDrivenSupported() const { return m_gpu_driven_supported; }
        [[nodiscard]] VulkanBackend* getVulkanBackend() const {
            return backend_ && m_api_type_ == RenderBackendApiType::Vulkan ? static_cast<VulkanBackend*>(backend_.get()) : nullptr;
        }
        [[nodiscard]] OpenGLBackend* getOpenGLBackend() const {
            return backend_ && m_api_type_ == RenderBackendApiType::OpenGL ? static_cast<OpenGLBackend*>(backend_.get()) : nullptr;
        }
        [[nodiscard]] D3D12Backend* getD3D12Backend() const {
            return backend_ && m_api_type_ == RenderBackendApiType::D3D12 ? static_cast<D3D12Backend*>(backend_.get()) : nullptr;
        }
        [[nodiscard]] GfxBackend* getBackend() const { return backend_.get(); }

        [[nodiscard]] GfxViewportSurface* createViewportSurface(GLFWwindow* window, UInt32 w, UInt32 h);
        void destroyViewportSurface(GfxViewportSurface* surface);
        [[nodiscard]] UInt32 getSecondarySurfaceCount() const { return static_cast<UInt32>(m_secondary_surfaces_.size()); }

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
