// do@Redlive

#include "opengl_backend.h"

#include "runtime/function/render/render_settings.h"

#include <glad/gl.h>

namespace dodoe {

	namespace {
		void GLAPIENTRY OnGlDebugMessage(GLenum, GLenum, GLuint, GLenum severity,
			GLsizei, const GLchar* message, const void* user_param) {
			auto* backend = static_cast<const OpenGLBackend*>(user_param);
			if (!backend || !message) return;

			GfxNativeMessageSeverity mapped = GfxNativeMessageSeverity::Info;
			switch (severity) {
			case GL_DEBUG_SEVERITY_HIGH:
				mapped = GfxNativeMessageSeverity::Error;
				break;
			case GL_DEBUG_SEVERITY_MEDIUM:
				mapped = GfxNativeMessageSeverity::Warning;
				break;
			default:
				break;
			}

			backend->reportNativeMessage(mapped, message);
		}
	}

    Bool OpenGLBackend::initialize(const GfxBackendCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("OpenGLBackend::initialize", "startup");
        initCommonState(info);
        if (!window_handle_) {
            DO_ERROR("OpenGLBackend::initialize: window handle is unavailable");
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

        if (enable_validation_ && GLAD_GL_VERSION_4_3) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(OnGlDebugMessage, this);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            DO_INFO("OpenGLBackend: native debug message callback registered");
        } else if (enable_validation_) {
            DO_WARN("OpenGLBackend: validation requested but OpenGL 4.3 debug output is unavailable");
        }

        updateFramebufferSize();
        if (m_fb_width <= 0 && info.width > 0) m_fb_width = static_cast<Int32>(info.width);
        if (m_fb_height <= 0 && info.height > 0) m_fb_height = static_cast<Int32>(info.height);

        DO_INFO("OpenGLBackend: initialized ({}x{})", m_fb_width, m_fb_height);
        return true;
    }

    void OpenGLBackend::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("OpenGLBackend::shutdown", "shutdown");
        releaseContext();
        window_handle_ = nullptr;
    }

    Bool OpenGLBackend::acquireContext() {
        DO_PROFILE_SCOPE_CATEGORY("OpenGLBackend::acquireContext", "synchronization");
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
        DO_PROFILE_SCOPE_CATEGORY("OpenGLBackend::releaseContext", "synchronization");
        std::lock_guard<std::mutex> lock(m_context_mutex);
        if (!window_handle_ || m_context_owner != std::this_thread::get_id()) return;
        glfwMakeContextCurrent(nullptr);
        m_context_owner = {};
    }

    void OpenGLBackend::updateFramebufferSize() {
        DO_PROFILE_SCOPE_CATEGORY("OpenGLBackend::updateFramebufferSize", "swapchain");
        if (window_handle_) {
            glfwGetFramebufferSize(window_handle_, &m_fb_width, &m_fb_height);
        }
    }

} // dodoe
