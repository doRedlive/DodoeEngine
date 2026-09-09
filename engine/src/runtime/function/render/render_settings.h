// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"

REFLECTION_TYPE(RenderSettingsInitInfo)

namespace dodoe {

    enum class RenderBackendApiType {
        None = 0,

        OpenGL,
        Vulkan,
        D3D12,
    };

    enum class RenderingPipelineType {
        None = 0,

        Forward,
        ForwardPlus,
        Deferred,
        DeferredPlus,
        Only2D,
        OnlyGUI,
    };

    enum class PresentMode {
        VSync = 0,
        Mailbox,
        Immediate,
    };

    enum class CullingPath {
        CpuOnly = 0,
        GpuOnly,
        CpuThenGpuVerify,
    };

    STRUCT(RenderSettingsInitInfo, WhiteListFields) {
        REFLECTION_BODY(RenderSettingsInitInfo)

        META(Enable)
        RenderBackendApiType api{ RenderBackendApiType::D3D12 };
        META(Enable)
        RenderingPipelineType pipeline{ RenderingPipelineType::Deferred };
        META(Enable)
        Bool enable_single_thread{ false };
        META(Enable)
        Bool enable_baseline_renderer{ false };
        META(Enable)
        PresentMode present_mode{ PresentMode::Mailbox };
        META(Enable)
        Bool windowless{ false };

        META(Enable)
        Bool enable_gpu_driven{ false };
        META(Enable)
        Bool enable_async_compute{ false };
        META(Enable)
        Bool enable_bindless{ false };
        META(Enable)
        CullingPath culling_path{ CullingPath::CpuOnly };
    };

    struct RenderDeviceCapabilities {
        Bool bindless_supported{false};
        Bool compute_queue_supported{false};
        Bool mesh_shader_supported{false};
        Bool ray_tracing_supported{false};
    };

    struct RenderFeatureSettings {
        Bool enable_gpu_driven{false};
        Bool enable_async_compute{false};
        Bool enable_bindless{false};
        CullingPath culling_path{CullingPath::CpuOnly};
    };

    struct ResolvedRenderFeatures {
        Bool gpu_driven_active{false};
        Bool async_compute_active{false};
        Bool bindless_active{false};
        String gpu_driven_fallback_reason{};
    };

    class RenderSettings {
    private:
        inline static RenderBackendApiType m_api{ RenderBackendApiType::None };
        inline static RenderingPipelineType m_pipeline{ RenderingPipelineType::None };
        inline static Bool m_enable_single_thread{ false };
        inline static Bool m_enable_baseline_renderer{ false };
        inline static PresentMode m_present_mode{ PresentMode::Mailbox };
        inline static Bool m_windowless{ false };
        inline static Bool m_gpu_driven_supported{ false };

        inline static RenderDeviceCapabilities m_device_caps{};
        inline static RenderFeatureSettings m_feature_settings{};
        inline static ResolvedRenderFeatures m_resolved_features{};

    public:
        [[nodiscard]] static Bool Initialize(const RenderSettingsInitInfo& info);

        [[nodiscard]] static RenderBackendApiType GetRenderBackendApiType() { return m_api; }
        [[nodiscard]] static String GetRenderBackendApiTypeStr();
        [[nodiscard]] static RenderingPipelineType GetRenderingPipelineType() { return m_pipeline; }
        [[nodiscard]] static Bool IsSingleThread() { return m_enable_single_thread; }
        [[nodiscard]] static PresentMode GetPresentMode() { return m_present_mode; }
        [[nodiscard]] static Bool IsWindowless() { return m_windowless; }
        [[nodiscard]] static Bool IsEnableBaselineRender() { return m_enable_baseline_renderer; }

        [[nodiscard]] static Bool IsGpuDrivenSupported() { return m_gpu_driven_supported; }
        static void SetGpuDrivenSupported(const Bool supported) { m_gpu_driven_supported = supported; }

        [[nodiscard]] static Bool IsBindlessActive() { return m_resolved_features.bindless_active; }

        [[nodiscard]] static Bool IsTaaEnabled() {
            return m_pipeline == RenderingPipelineType::Deferred
                || m_pipeline == RenderingPipelineType::DeferredPlus
                || m_enable_baseline_renderer;
        }

        [[nodiscard]] static Bool IsMsaaEnabled() {
            return m_pipeline == RenderingPipelineType::Forward
                || m_pipeline == RenderingPipelineType::ForwardPlus;
        }

        [[nodiscard]] static UInt32 GetMsaaSampleCount() {
            if (!IsMsaaEnabled() || GetRenderBackendApiType() == RenderBackendApiType::OpenGL) {
                return 1u;
            }
            return 4u;
        }

        [[nodiscard]] static const RenderDeviceCapabilities& GetDeviceCapabilities() { return m_device_caps; }
        static void SetDeviceCapabilities(const RenderDeviceCapabilities& caps) { m_device_caps = caps; }

        [[nodiscard]] static const RenderFeatureSettings& GetFeatureSettings() { return m_feature_settings; }

        [[nodiscard]] static const ResolvedRenderFeatures& GetResolvedFeatures() { return m_resolved_features; }

        static void ResolveFeatures(const RenderFeatureSettings& settings);
        static void ResolveFeatures(const RenderFeatureSettings& settings, const RenderDeviceCapabilities& device_caps);
    };

} // dodoe
