// do@Redlive

#ifdef DODOE_DEBUG_ENABLED

#pragma once

#include "dopch.h"

#include "imgui/imgui.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/draw_command_list.h"

struct GLFWwindow;

namespace dodoe {

    class GfxContext;
    class GfxViewportSurface;
    class ImGuiDrawRenderer;
    class PipelineStateCache;
    class ShaderLibrary;
    struct ImGuiRenderPacket;

    struct ImGuiViewportRenderer {
        struct ViewportRenderData {
            GfxViewportSurface* surface{nullptr};
            GfxBufferHandle vertex_buffer{};
            GfxBufferHandle index_buffer{};
            GfxBufferHandle constant_buffer{};
            GfxCommandListLifetimeTrackerHandle tracker{};
            DrawCommandList recorder;
            UInt32 image_index{0};
            Bool suspended{false};
            Bool frame_presentable{false};
            Bool resize_pending{false};
            Int32 resize_width{0};
            Int32 resize_height{0};
        };

        static void Install(GfxContext& gfx, ImGuiDrawRenderer& draw_renderer,
                            const PipelineStateCache* pipeline_cache,
                            const ShaderLibrary* shader_library);
        static void Uninstall();

        static void RenderWindowOnRenderThread(ImGuiViewport* viewport, const ImGuiRenderPacket& packet);

    private:
        static void RendererCreateWindow(ImGuiViewport* viewport);
        static void RendererDestroyWindow(ImGuiViewport* viewport);
        static void RendererSetWindowSize(ImGuiViewport* viewport, ImVec2 size);

        static GfxContext* s_gfx;
        static GfxDeviceHandle s_device;
        static ImGuiDrawRenderer* s_draw_renderer;
        static const PipelineStateCache* s_pipeline_cache;
        static const ShaderLibrary* s_shader_library;
    };

} // namespace dodoe

#endif // DODOE_DEBUG_ENABLED
