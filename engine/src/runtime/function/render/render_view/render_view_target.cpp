// do@Redlive

#include "render_view_target.h"

namespace dodoe {

    Bool RenderViewTarget::initialize(const RenderViewTargetCreateInfo& info) {
        m_viewport = RenderViewport(info.logical, info.window, info.pixel);
        m_camera   = info.camera;
        return true;
    }

    void RenderViewTarget::shutdown() {
        m_camera = nullptr;
    }

} // namespace dodoe
