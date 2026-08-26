// do@Redlive

#ifdef DODOE_DEBUG_ENABLED

#pragma once

#include "dopch.h"

#include "glfw/glfw3.h"
#include "imgui/imgui.h"

#include "imgui_draw_renderer.h"

struct ImGuiContext;

namespace dodoe {

    class GfxContext;

    struct ImGuiRenderCmd {
        Vector4f        clip_rect;
        ImTextureID     texture_id;
        UInt32          vtx_offset;
        UInt32          idx_offset;
        UInt32          elem_count;
        ImDrawCallback  user_callback;
    };

    struct ImGuiRenderList {
        DynamicArray<ImDrawVert>       vertices;
        DynamicArray<ImDrawIdx>        indices;
        DynamicArray<ImGuiRenderCmd>   commands;
    };

    struct ImGuiRenderPacket {
        DynamicArray<ImGuiRenderList> lists;
        Vector2f display_pos;
        Vector2f display_size;
    };

    class ImGuiBuilder {
    public:
        static void SetupImGui(GLFWwindow* window);
        static void PrepareImGui();
        static void RenderImGui();
        static void CleanupImGui();

        static void InstallViewportRenderer(GfxContext& gfx, ImGuiDrawRenderer draw_renderer,
                                            const PipelineStateCache* pipeline_cache,
                                            const ShaderLibrary* shader_library);
        static void UninstallViewportRenderer();
        static void SerializeImGuiDrawData(const ImDrawData* draw_data, ImGuiRenderPacket& out_packet);

        static ImGuiContext* GetContext() { return s_context; }
        static const ImGuiRenderPacket& GetRenderPacket() { return s_packet; }
        static Bool GetViewportsEnabled() { return s_viewports_enabled; }

    private:
        static inline Bool s_glfwBackendInit = false;
        static inline ImGuiContext* s_context = nullptr;
        static inline ImGuiRenderPacket s_packet{};
        static inline Bool s_viewports_enabled = false;
        static inline Scope<ImGuiDrawRenderer> s_viewport_renderer{nullptr};
    };

} // namespace dodoe

#endif // DODOE_DEBUG_ENABLED
