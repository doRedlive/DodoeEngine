// do@Redlive

#ifdef DODOE_DEBUG_ENABLED

#include "imgui_viewport_renderer.h"

#include "imgui_builder.h"
#include "imgui_draw_renderer.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/gfx_viewport_surface.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"

#include <algorithm>

namespace dodoe {

    namespace {
        constexpr UInt32 kViewportVertexCapacity = 65536;
        constexpr UInt32 kViewportIndexCapacity = 65536;
    }

    GfxContext* ImGuiViewportRenderer::s_gfx = nullptr;
    GfxDeviceHandle ImGuiViewportRenderer::s_device{};
    ImGuiDrawRenderer* ImGuiViewportRenderer::s_draw_renderer = nullptr;
    const PipelineStateCache* ImGuiViewportRenderer::s_pipeline_cache = nullptr;
    const ShaderLibrary* ImGuiViewportRenderer::s_shader_library = nullptr;

    void ImGuiViewportRenderer::Install(GfxContext& gfx, ImGuiDrawRenderer& draw_renderer,
                                        const PipelineStateCache* pipeline_cache,
                                        const ShaderLibrary* shader_library) {
        if (s_draw_renderer) {
            Uninstall();
        }
        s_gfx = &gfx;
        s_device = gfx.getDevice();
        s_draw_renderer = &draw_renderer;
        s_pipeline_cache = pipeline_cache;
        s_shader_library = shader_library;

        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        platform_io.Renderer_CreateWindow = &RendererCreateWindow;
        platform_io.Renderer_DestroyWindow = &RendererDestroyWindow;
        platform_io.Renderer_SetWindowSize = &RendererSetWindowSize;
        platform_io.Renderer_RenderWindow = &RendererRenderWindow;
        platform_io.Renderer_SwapBuffers = &RendererSwapBuffers;
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    }

    void ImGuiViewportRenderer::Uninstall() {
        if (ImGui::GetCurrentContext()) {
            ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_RendererHasViewports;
            ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
            platform_io.Renderer_CreateWindow = nullptr;
            platform_io.Renderer_DestroyWindow = nullptr;
            platform_io.Renderer_SetWindowSize = nullptr;
            platform_io.Renderer_RenderWindow = nullptr;
            platform_io.Renderer_SwapBuffers = nullptr;
            for (ImGuiViewport* viewport : platform_io.Viewports) {
                if (viewport && viewport->RendererUserData) {
                    RendererDestroyWindow(viewport);
                }
            }
        }
        s_gfx = nullptr;
        s_device = nullptr;
        s_draw_renderer = nullptr;
        s_pipeline_cache = nullptr;
        s_shader_library = nullptr;
    }

    void ImGuiViewportRenderer::RendererCreateWindow(ImGuiViewport* viewport) {
        if (!s_gfx || !viewport) {
            return;
        }
        if (viewport->ID == ImGui::GetMainViewport()->ID) {
            return;
        }

        auto* window = static_cast<GLFWwindow*>(viewport->PlatformHandle);
        if (!window) {
            return;
        }
        const int width = std::max(1, static_cast<int>(viewport->Size.x));
        const int height = std::max(1, static_cast<int>(viewport->Size.y));

        auto* data = new ViewportRenderData();
        data->surface = s_gfx->createViewportSurface(window, static_cast<UInt32>(width), static_cast<UInt32>(height));
        if (!data->surface) {
            delete data;
            return;
        }

        data->vertex_buffer = create_ref<GfxBuffer>(GfxBufferDesc()
            .setByteSize(kViewportVertexCapacity * sizeof(ImDrawVert))
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("Viewport ImGuiVertexBuffer"));
        data->vertex_buffer->initializeGpu(s_device);

        data->index_buffer = create_ref<GfxBuffer>(GfxBufferDesc()
            .setByteSize(kViewportIndexCapacity * sizeof(ImDrawIdx))
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("Viewport ImGuiIndexBuffer"));
        data->index_buffer->initializeGpu(s_device);

        data->constant_buffer = create_ref<GfxBuffer>(GfxBufferDesc()
            .setByteSize(16)
            .setIsConstantBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
            .setDebugName("Viewport ImGuiConstantBuffer"));
        data->constant_buffer->initializeGpu(s_device);

        data->tracker = s_device->createCommandListLifetimeTracker(GfxCommandQueue::Graphics);
        data->recorder.setDevice(s_device);

        viewport->RendererUserData = data;
    }

    void ImGuiViewportRenderer::RendererDestroyWindow(ImGuiViewport* viewport) {
        if (!viewport) {
            return;
        }
        auto* data = static_cast<ViewportRenderData*>(viewport->RendererUserData);
        viewport->RendererUserData = nullptr;
        if (!data) {
            return;
        }

        if (s_gfx && data->surface) {
            s_gfx->waitForIdle();
            s_gfx->destroyViewportSurface(data->surface);
        }
        data->tracker = nullptr;
        data->vertex_buffer = nullptr;
        data->index_buffer = nullptr;
        data->constant_buffer = nullptr;
        delete data;
    }

    void ImGuiViewportRenderer::RendererSetWindowSize(ImGuiViewport* viewport, ImVec2 size) {
        if (!viewport) {
            return;
        }
        auto* data = static_cast<ViewportRenderData*>(viewport->RendererUserData);
        if (!data || !data->surface || !s_gfx) {
            return;
        }

        const int width = static_cast<int>(size.x);
        const int height = static_cast<int>(size.y);
        if (width <= 0 || height <= 0) {
            data->suspended = true;
            return;
        }

        const auto extent = data->surface->extent();
        if (width == extent.x && height == extent.y) {
            data->suspended = false;
            return;
        }

        s_gfx->waitForIdle();
        data->suspended = false;
        if (!data->surface->resize(static_cast<UInt32>(width), static_cast<UInt32>(height))) {
            data->suspended = true;
        }
    }

    void ImGuiViewportRenderer::RendererRenderWindow(ImGuiViewport* viewport, void*) {
        if (!viewport || !s_draw_renderer || !s_device) {
            return;
        }
        auto* data = static_cast<ViewportRenderData*>(viewport->RendererUserData);
        if (!data || !data->surface || !viewport->DrawData) {
            return;
        }
        if (viewport->DrawData->CmdListsCount <= 0) {
            return;
        }

        data->frame_presentable = false;
        ImGuiRenderPacket packet;
        ImGuiBuilder::SerializeImGuiDrawData(viewport->DrawData, packet);
        if (packet.lists.empty()) {
            return;
        }

        if (!data->surface->acquire(data->image_index)) {
            data->suspended = true;
            return;
        }
        data->suspended = false;
        data->frame_presentable = true;

        const auto framebuffer = data->surface->getFramebuffer(data->image_index);
        if (!framebuffer) {
            return;
        }

        data->recorder.beginFrame();
        s_draw_renderer->render(packet, framebuffer, framebuffer->getFramebufferInfo(),
                                data->vertex_buffer, data->index_buffer, data->constant_buffer,
                                data->recorder, s_pipeline_cache, s_shader_library);

        if (data->tracker) {
            auto cmd = s_device->createCommandList(
                GfxCommandListParameters().setLifetimeTracker(data->tracker.Get()));
            cmd->open();
            data->recorder.execute(cmd);
            cmd->close();
            s_device->executeCommandList(cmd);
            data->tracker->runGarbageCollection();
        }
    }

    void ImGuiViewportRenderer::RendererSwapBuffers(ImGuiViewport* viewport, void*) {
        if (!viewport) {
            return;
        }
        auto* data = static_cast<ViewportRenderData*>(viewport->RendererUserData);
        if (!data || data->suspended || !data->surface || !data->frame_presentable) {
            return;
        }
        data->frame_presentable = false;
        if (data->surface->isOpenGL()) {
            return;
        }
        data->surface->present(data->image_index);
    }

} // namespace dodoe

#endif // DODOE_DEBUG_ENABLED
