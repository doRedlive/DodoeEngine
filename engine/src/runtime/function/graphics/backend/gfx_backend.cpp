// do@Redlive

#include "gfx_backend.h"

#include "runtime/function/render/render_settings.h"

#include "vulkan_backend.h"
#include "opengl_backend.h"
#include "d3d12_backend.h"

namespace dodoe {

    void GfxBackend::initCommonState(const GfxBackendCreateInfo& info) {
        window_handle_ = info.window_handle;
        host_handle_ = info.host_handle;
        api_type_ = info.api_type;
        enable_validation_ = info.enable_validation;
        width_ = info.width;
        height_ = info.height;
    }

    Scope<GfxBackend> GfxBackend::Create(const GfxBackendCreateInfo& info) {
        switch (info.api_type) {
        case RenderBackendApiType::Vulkan:
            return Managed<VulkanBackend, GfxBackendCreateInfo>::Create(info);
        case RenderBackendApiType::OpenGL:
            return Managed<OpenGLBackend, GfxBackendCreateInfo>::Create(info);
        case RenderBackendApiType::D3D12:
            return Managed<D3D12Backend, GfxBackendCreateInfo>::Create(info);
        default:
            return nullptr;
        }
    }

    void GfxBackend::Destroy(Scope<GfxBackend>& obj) {
        if (!obj) return;
        obj->shutdown();
        obj.reset();
    }

} // dodoe
