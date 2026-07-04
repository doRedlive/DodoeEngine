// do@Redlive

#ifdef DODOE_EDITOR

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
        static inline bool s_glfwBackendInit = false;
        static inline ImGuiContext* s_context = nullptr;
    };

} // dodoe

#endif//DODOE_EDITOR
