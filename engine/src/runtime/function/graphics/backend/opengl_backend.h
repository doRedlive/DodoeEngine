// do@Redlive
#pragma once

#include "dopch.h"

#include "GLFW/glfw3.h"

namespace dodoe {

    struct OpenGLBackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
    };

    class OpenGLBackend : public Managed<OpenGLBackend, OpenGLBackendCreateInfo> {
        friend class Managed<OpenGLBackend, OpenGLBackendCreateInfo>;

        GLFWwindow* m_window_handle{nullptr};
        Int32 m_fb_width{0};
        Int32 m_fb_height{0};

    public:
        [[nodiscard]] Vector2i getSwapchainExtent2d() const { return Vector2i(m_fb_width, m_fb_height); }
        [[nodiscard]] GLFWwindow* getWindow() const { return m_window_handle; }
        void updateFramebufferSize();

    private:
        Bool initialize(const OpenGLBackendCreateInfo& info);
        void shutdown();
    };

} // dodoe
