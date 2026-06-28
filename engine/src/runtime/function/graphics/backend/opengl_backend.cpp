// do@Redlive

#include "opengl_backend.h"

#include <glad/gl.h>

namespace dodoe {

    Bool OpenGLBackend::initialize(const OpenGLBackendCreateInfo& info) {
        m_window_handle = info.window_handle;
        if (!m_window_handle) {
            return false;
        }

        glfwMakeContextCurrent(m_window_handle);

        if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
            DO_ERROR("OpenGLBackend: gladLoadGL failed");
            return false;
        }

        updateFramebufferSize();

        return true;
    }

    void OpenGLBackend::shutdown() {
        m_window_handle = nullptr;
    }

    void OpenGLBackend::updateFramebufferSize() {
        if (m_window_handle) {
            glfwGetFramebufferSize(m_window_handle, &m_fb_width, &m_fb_height);
        }
    }

} // dodoe
