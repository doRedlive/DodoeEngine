// do@Redlive

#include "render_settings.h"

namespace dodoe {

    bool RenderSettings::Initialize(const RenderSettingsInitInfo& info) {
        if (info.api == RenderBackendApiType::None) return false;
        if (info.pipeline == RenderingPipelineType::None) return false;

        m_api = info.api;
        m_pipeline = info.pipeline;
        m_threading_mode = info.threading_mode;

        return true;
    }

} // dodoe
