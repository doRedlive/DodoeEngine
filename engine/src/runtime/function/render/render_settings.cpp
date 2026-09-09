// do@Redlive

#include "render_settings.h"

namespace dodoe {

    bool RenderSettings::Initialize(const RenderSettingsInitInfo& info) {
        if (info.api == RenderBackendApiType::None) return false;
        if (info.pipeline == RenderingPipelineType::None) return false;

        m_api = info.api;
        m_pipeline = info.pipeline;
        m_enable_single_thread = info.enable_single_thread;
        m_enable_baseline_renderer = info.enable_baseline_renderer;
        m_present_mode = info.present_mode;
        m_windowless = info.windowless;
        m_feature_settings.enable_gpu_driven = info.enable_gpu_driven;
        m_feature_settings.enable_async_compute = info.enable_async_compute;
        m_feature_settings.enable_bindless = info.enable_bindless;
        m_feature_settings.culling_path = info.culling_path;

        return true;
    }

    void RenderSettings::ResolveFeatures(const RenderFeatureSettings& settings) {
        ResolveFeatures(settings, m_device_caps);
    }

    void RenderSettings::ResolveFeatures(const RenderFeatureSettings& settings, const RenderDeviceCapabilities& device_caps) {
        m_device_caps = device_caps;
        m_feature_settings = settings;

        ResolvedRenderFeatures resolved{};
        const RenderDeviceCapabilities& caps = device_caps;

        resolved.bindless_active = settings.enable_bindless && caps.bindless_supported;
        if (settings.enable_bindless && !caps.bindless_supported) {
            DO_WARN("RenderSettings: bindless requested but not supported by device, falling back to bound resources");
        }

        if (settings.enable_gpu_driven) {
            if (m_api != RenderBackendApiType::D3D12) {
                resolved.gpu_driven_fallback_reason = "GPU-driven shaders are only available on D3D12";
            } else if (!caps.bindless_supported) {
                resolved.gpu_driven_fallback_reason = "bindless not supported by device";
            } else if (!caps.compute_queue_supported) {
                resolved.gpu_driven_fallback_reason = "compute queue not supported by device";
            } else {
                resolved.gpu_driven_active = true;
            }
            if (!resolved.gpu_driven_active) {
                DO_WARN("RenderSettings: gpu-driven requested but unavailable ({}), falling back to CPU-driven path",
                    resolved.gpu_driven_fallback_reason);
            }
        } else {
            resolved.gpu_driven_fallback_reason = "disabled by project settings";
        }

        if (!resolved.gpu_driven_active && m_feature_settings.culling_path != CullingPath::CpuOnly) {
            DO_WARN("RenderSettings: requested GPU culling path is unavailable, forcing CpuOnly");
            m_feature_settings.culling_path = CullingPath::CpuOnly;
        }

        resolved.async_compute_active = settings.enable_async_compute && caps.compute_queue_supported && resolved.gpu_driven_active;

        m_resolved_features = resolved;
        m_gpu_driven_supported = resolved.gpu_driven_active;
    }

    String RenderSettings::GetRenderBackendApiTypeStr(){
        switch (m_api)
        {
        case RenderBackendApiType::None:
            return "Unknown";
        case RenderBackendApiType::OpenGL:
            return "OpenGL";
        case RenderBackendApiType::Vulkan:
            return "Vulkan";
        case RenderBackendApiType::D3D12:
            return "D3D12";
        default:
            return "Unknown";
        }
    }

} // dodoe
