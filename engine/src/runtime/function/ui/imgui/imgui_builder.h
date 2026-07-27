// do@Redlive

#ifdef DODOE_DEBUG_ENABLED

#pragma once

#include "dopch.h"

#include "glfw/glfw3.h"

struct ImGuiContext;

namespace dodoe {

    class ImGuiBuilder {
    public:
        static void SetupImGui(GLFWwindow* window);
        static void PrepareImGui();
        static void CleanupImGui();

        static ImGuiContext* GetContext() { return s_context; }

    private:
        static inline Bool s_glfwBackendInit = false;
        static inline ImGuiContext* s_context = nullptr;
    };

} // namespace dodoe

#endif // DODOE_DEBUG_ENABLED
