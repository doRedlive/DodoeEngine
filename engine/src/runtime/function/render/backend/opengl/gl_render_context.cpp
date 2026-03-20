//
// Created by Redlive on 2026/3/18.
//

#include "gl_render_context.h"

namespace dodoe {

    namespace {
        void GLAPIENTRY glErrorCallback(GLenum, GLenum type, GLuint, GLenum, GLsizei, const GLchar* msg, const void*) {
            if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR) {
                DoError("[OpenGL err]: {}", msg);
            }
        }
    }

    void GlRenderContext::initialize(RenderContextCreateInfo create_info) {
        glfwMakeContextCurrent(create_info.window);

        DoAssert(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "OpenGL load failed!");

        if (glfwExtensionSupported("GL_KHR_debug")) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glErrorCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_FALSE);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_FALSE);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        }
    }

    void GlRenderContext::shutdown() {
        DoDebug("OpenGL shutdown.");
    }

} // dodoe