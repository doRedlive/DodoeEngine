// do@Redlive

#ifdef DODOE_EDITOR

#include "dopch.h"

#include "glfw/glfw3.h"

namespace dodoe {

    class ImGuiBuilder {
    public:
        static void SetupImGui(GLFWwindow* window);
        static void PrepareImGui();
        static void CleanupImGui();

    private:
        static inline bool s_glfwBackendInit = false;
    };

} // dodoe

#endif//DODOE_EDITOR
