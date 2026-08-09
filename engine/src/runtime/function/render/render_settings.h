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
    };

    enum class ThreadingMode {
        TripleThread,
        DualThread,
        SingleThread,
    };

    enum class PresentMode {
        VSync = 0,
        Mailbox,
        Immediate,
    };

    STRUCT(RenderSettingsInitInfo, WhiteListFields) {
        REFLECTION_BODY(RenderSettingsInitInfo)

        META(Enable)
        RenderBackendApiType api{ RenderBackendApiType::D3D12 };
        META(Enable)
        RenderingPipelineType pipeline{ RenderingPipelineType::Deferred };
        META(Enable)
        ThreadingMode threading_mode{ ThreadingMode::TripleThread };
        META(Enable)
        PresentMode present_mode{ PresentMode::Mailbox };
    };

    struct DeviceCapabilities {
        Bool bindless_supported{false};
        Bool compute_queue_supported{false};
        Bool mesh_shader_supported{false};
        Bool ray_tracing_supported{false};
    };

    enum class CullingPath {
        CpuOnly = 0,
        GpuOnly,
        CpuThenGpuVerify,
    };

    struct RenderFeatureSettings {
        Bool enable_gpu_driven{true};
        Bool enable_async_compute{false};
        Bool enable_bindless{true};
        CullingPath culling_path{CullingPath::CpuOnly};
    };

    struct ResolvedRenderFeatures {
        Bool gpu_driven_active{false};
        Bool async_compute_active{false};
        Bool bindless_active{false};
        String gpu_driven_fallback_reason{};
    };

    class RenderSettings {
    public:
        [[nodiscard]] static Bool Initialize(const RenderSettingsInitInfo& info);

        [[nodiscard]] static RenderBackendApiType GetRenderBackendApiType() { return m_api; }
        [[nodiscard]] static Bool IsClipSpaceYDown() {
            return m_api == RenderBackendApiType::D3D12 || m_api == RenderBackendApiType::Vulkan;
        }
        [[nodiscard]] static RenderingPipelineType GetRenderingPipelineType() { return m_pipeline; }
        [[nodiscard]] static ThreadingMode GetThreadingMode() { return m_threading_mode; }
        [[nodiscard]] static PresentMode GetPresentMode() { return m_present_mode; }

        [[nodiscard]] static Bool IsGpuDrivenSupported() { return m_gpu_driven_supported; }
        static void SetGpuDrivenSupported(const Bool supported) { m_gpu_driven_supported = supported; }

        [[nodiscard]] static Bool IsBindlessActive() { return m_resolved_features.bindless_active; }

        [[nodiscard]] static const DeviceCapabilities& GetDeviceCapabilities() { return m_device_caps; }
        static void SetDeviceCapabilities(const DeviceCapabilities& caps) { m_device_caps = caps; }

        [[nodiscard]] static const RenderFeatureSettings& GetFeatureSettings() { return m_feature_settings; }

        [[nodiscard]] static const ResolvedRenderFeatures& GetResolvedFeatures() { return m_resolved_features; }

        static void ResolveFeatures(const RenderFeatureSettings& settings);

    private:
        inline static RenderBackendApiType m_api{ RenderBackendApiType::None };
        inline static RenderingPipelineType m_pipeline{ RenderingPipelineType::None };
        inline static ThreadingMode m_threading_mode{ ThreadingMode::TripleThread };
        inline static PresentMode m_present_mode{ PresentMode::Mailbox };
        inline static Bool m_gpu_driven_supported{ false };

        inline static DeviceCapabilities m_device_caps{};
        inline static RenderFeatureSettings m_feature_settings{};
        inline static ResolvedRenderFeatures m_resolved_features{};
    };

} // dodoe
