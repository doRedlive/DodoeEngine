// do@Redlive

#include "render_view_family.h"

namespace dodoe {

    void RenderViewFamily::reset() {
        m_views.clear();
        m_time_seconds = 0.0f;
        m_delta_seconds = 0.0f;
    }

    void RenderViewFamily::shutdown() {
        reset();
    }

} // dodoe
