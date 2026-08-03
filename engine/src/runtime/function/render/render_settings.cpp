// do@Redlive

#include "render_settings.h"

namespace dodoe {

    bool RenderSettings::Initialize(const RenderSettingsInitInfo& info) {
        if (info.api == RenderBackendApiType::None) return false;
        if (info.pipeline == RenderingPipelineType::None) return false;

        m_api = info.api;
        m_pipeline = info.pipeline;
        m_threading_mode = info.threading_mode;
        m_present_mode = info.present_mode;

        if (m_api == RenderBackendApiType::OpenGL && m_threading_mode == ThreadingMode::TripleThread) {
            m_threading_mode = ThreadingMode::DualThread;
        }

        return true;
    }

    void RenderSettings::ResolveFeatures(const RenderFeatureSettings& settings) {
        m_feature_settings = settings;

        ResolvedRenderFeatures resolved{};
        const auto& caps = m_device_caps;

        resolved.bindless_active = m_api != RenderBackendApiType::OpenGL && settings.enable_bindless && caps.bindless_supported;

        if (settings.enable_gpu_driven) {
            if (!caps.bindless_supported) {
                resolved.gpu_driven_fallback_reason = "bindless not supported by device";
            } else if (!caps.compute_queue_supported) {
                resolved.gpu_driven_fallback_reason = "compute queue not supported by device";
            } else {
                resolved.gpu_driven_active = true;
            }
        } else {
            resolved.gpu_driven_fallback_reason = "disabled by project settings";
        }

        resolved.async_compute_active = settings.enable_async_compute && caps.compute_queue_supported && resolved.gpu_driven_active;

        m_resolved_features = resolved;
        m_gpu_driven_supported = resolved.gpu_driven_active;
    }

} // dodoe
