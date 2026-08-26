// do@Redlive

#include "opengl_backend.h"

#include "runtime/function/render/render_settings.h"

#include <glad/gl.h>

namespace dodoe {

    Bool OpenGLBackend::initialize(const GfxBackendCreateInfo& info) {
        initCommonState(info);
        if (!window_handle_) {
            return false;
        }

        glfwMakeContextCurrent(window_handle_);
        m_context_owner = std::this_thread::get_id();

        switch (RenderSettings::GetPresentMode()) {
        case PresentMode::VSync:
            glfwSwapInterval(1);
            break;
        case PresentMode::Immediate:
            glfwSwapInterval(0);
            break;
        case PresentMode::Mailbox:
        default:
            glfwSwapInterval(-1);
            break;
        }

        if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
            DO_ERROR("OpenGLBackend: gladLoadGL failed");
            return false;
        }

        updateFramebufferSize();
        if (m_fb_width <= 0 && info.width > 0) m_fb_width = static_cast<Int32>(info.width);
        if (m_fb_height <= 0 && info.height > 0) m_fb_height = static_cast<Int32>(info.height);

        return true;
    }

    void OpenGLBackend::shutdown() {
        releaseContext();
        window_handle_ = nullptr;
    }

    Bool OpenGLBackend::acquireContext() {
        std::lock_guard<std::mutex> lock(m_context_mutex);
        if (!window_handle_) return false;
        const auto current_thread = std::this_thread::get_id();
        if (m_context_owner != std::thread::id{} && m_context_owner != current_thread) return false;
        glfwMakeContextCurrent(window_handle_);
        if (glfwGetCurrentContext() != window_handle_) return false;
        m_context_owner = current_thread;
        return true;
    }

    void OpenGLBackend::releaseContext() {
        std::lock_guard<std::mutex> lock(m_context_mutex);
        if (!window_handle_ || m_context_owner != std::this_thread::get_id()) return;
        glfwMakeContextCurrent(nullptr);
        m_context_owner = {};
    }

    void OpenGLBackend::updateFramebufferSize() {
        if (window_handle_) {
            glfwGetFramebufferSize(window_handle_, &m_fb_width, &m_fb_height);
        }
    }

} // dodoe
