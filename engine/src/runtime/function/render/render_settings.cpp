// do@Redlive

#include "render_settings.h"

#include <algorithm>

namespace dodoe {

    namespace {

        Bool IsForwardPipeline(const RenderingPipelineType pipeline) {
            return pipeline == RenderingPipelineType::Forward
                || pipeline == RenderingPipelineType::ForwardPlus;
        }

        Bool IsDeferredPipeline(const RenderingPipelineType pipeline) {
            return pipeline == RenderingPipelineType::Deferred
                || pipeline == RenderingPipelineType::DeferredPlus;
        }

        AntialiasingMode ResolveAaMode(const RenderingPipelineType pipeline, const Bool baseline,
                                       const AntialiasingMode mode) {
            if (mode == AntialiasingMode::None) {
                return AntialiasingMode::None;
            }
            if (baseline || IsDeferredPipeline(pipeline)) {
                return (mode == AntialiasingMode::FXAA || mode == AntialiasingMode::TAA)
                    ? mode : AntialiasingMode::TAA;
            }
            if (IsForwardPipeline(pipeline)) {
                return (mode == AntialiasingMode::MSAA || mode == AntialiasingMode::FXAA)
                    ? mode : AntialiasingMode::MSAA;
            }
            return AntialiasingMode::None;
        }

        UInt32 SnapSampleCount(UInt32 sample_count) {
            sample_count = std::clamp(sample_count, 1u, 8u);
            while (sample_count & (sample_count - 1u)) {
                sample_count &= sample_count - 1u;
            }
            return sample_count;
        }

        const char* AaModeName(const AntialiasingMode mode) {
            switch (mode) {
                case AntialiasingMode::MSAA: return "MSAA";
                case AntialiasingMode::FXAA: return "FXAA";
                case AntialiasingMode::TAA:  return "TAA";
                default: return "None";
            }
        }

    }

    bool RenderSettings::Initialize(const RenderSettingsInitInfo& info) {
        if (info.api == RenderBackendApiType::None) return false;
        if (info.pipeline == RenderingPipelineType::None) return false;

        m_api = info.api;
        m_pipeline = info.pipeline;
        m_enable_single_thread = info.enable_single_thread;
        m_enable_baseline_renderer = info.enable_baseline_renderer;
        m_present_mode = info.present_mode;
        m_windowless = info.windowless;
        m_create_default_view_target = info.create_default_view_target;
        m_aa_mode.store(ResolveAaMode(info.pipeline, info.enable_baseline_renderer, info.aa_mode), std::memory_order_relaxed);
        m_msaa_sample_count.store(SnapSampleCount(info.msaa_sample_count), std::memory_order_relaxed);
        m_feature_settings.enable_gpu_driven = info.enable_gpu_driven;
        m_feature_settings.enable_async_compute = info.enable_async_compute;
        m_feature_settings.enable_bindless = info.enable_bindless;
        m_feature_settings.culling_path = info.culling_path;

        return true;
    }

    void RenderSettings::SetAntialiasingMode(const AntialiasingMode mode) {
        const AntialiasingMode resolved = ResolveAaMode(m_pipeline, m_enable_baseline_renderer, mode);
        const AntialiasingMode previous = m_aa_mode.exchange(resolved, std::memory_order_relaxed);
        if (previous != resolved) {
            DO_INFO("RenderSettings: antialiasing mode changed ({} -> {})", AaModeName(previous), AaModeName(resolved));
        }
    }

    void RenderSettings::SetMsaaSampleCount(const UInt32 sample_count) {
        const UInt32 snapped = SnapSampleCount(sample_count);
        const UInt32 previous = m_msaa_sample_count.exchange(snapped, std::memory_order_relaxed);
        if (previous != snapped) {
            DO_INFO("RenderSettings: msaa sample count changed ({}x -> {}x)", previous, snapped);
        }
    }

    Bool RenderSettings::IsTaaEnabled() {
        return GetAntialiasingMode() == AntialiasingMode::TAA
            && (IsDeferredPipeline(m_pipeline) || m_enable_baseline_renderer);
    }

    Bool RenderSettings::IsMsaaEnabled() {
        return GetAntialiasingMode() == AntialiasingMode::MSAA && IsForwardPipeline(m_pipeline);
    }

    Bool RenderSettings::IsFxaaEnabled() {
        return GetAntialiasingMode() == AntialiasingMode::FXAA
            && (IsForwardPipeline(m_pipeline) || IsDeferredPipeline(m_pipeline) || m_enable_baseline_renderer);
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
