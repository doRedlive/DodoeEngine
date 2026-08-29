// do@Redlive

#include "gfx_backend.h"

#include "runtime/function/render/render_settings.h"

#include "vulkan_backend.h"
#include "opengl_backend.h"
#include "d3d12_backend.h"

namespace dodoe {

    void GfxBackend::initCommonState(const GfxBackendCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxBackend::initCommonState", "startup");
        window_handle_ = info.window_handle;
        host_handle_ = info.host_handle;
        api_type_ = info.api_type;
        enable_validation_ = info.enable_validation;
        width_ = info.width;
        height_ = info.height;

        DO_INFO("GfxBackend: common state initialized (validation={}, extent={}x{})",
            enable_validation_, width_, height_);
    }

    void GfxBackend::reportNativeMessage(GfxNativeMessageSeverity severity, const char* message_text) const {
        if (!message_text || *message_text == '\0') return;

        const char* tag = "RHI";
        switch (api_type_) {
        case RenderBackendApiType::OpenGL: tag = "OpenGL"; break;
        case RenderBackendApiType::Vulkan: tag = "Vulkan"; break;
        case RenderBackendApiType::D3D12:  tag = "D3D12";  break;
        default: break;
        }

        switch (severity) {
        case GfxNativeMessageSeverity::Fatal:
        case GfxNativeMessageSeverity::Error:
            DO_ERROR("[{}] {}", tag, message_text);
            break;
        case GfxNativeMessageSeverity::Warning:
            DO_WARN("[{}] {}", tag, message_text);
            break;
        default:
            DO_INFO("[{}] {}", tag, message_text);
            break;
        }
    }

    Scope<GfxBackend> GfxBackend::Create(const GfxBackendCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("GfxBackend::Create", "startup");
        switch (info.api_type) {
        case RenderBackendApiType::Vulkan:
            DO_INFO("GfxBackend: creating Vulkan backend");
            return Managed<VulkanBackend, GfxBackendCreateInfo>::Create(info);
        case RenderBackendApiType::OpenGL:
            DO_INFO("GfxBackend: creating OpenGL backend");
            return Managed<OpenGLBackend, GfxBackendCreateInfo>::Create(info);
        case RenderBackendApiType::D3D12:
            DO_INFO("GfxBackend: creating D3D12 backend");
            return Managed<D3D12Backend, GfxBackendCreateInfo>::Create(info);
        default:
            DO_ERROR("GfxBackend: unsupported render backend API ({})", static_cast<int>(info.api_type));
            return nullptr;
        }
    }

    void GfxBackend::Destroy(Scope<GfxBackend>& obj) {
        if (!obj) return;
        DO_PROFILE_SCOPE_CATEGORY("GfxBackend::Destroy", "shutdown");
        DO_INFO("GfxBackend: shutting down backend");
        obj->shutdown();
        obj.reset();
    }

} // dodoe
