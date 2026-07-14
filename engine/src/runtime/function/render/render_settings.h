// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    enum class RenderBackendApiType {
        None = 0,

        OpenGL,
        Vulkan,
        DX12,
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

    struct RenderSettingsInitInfo {
        RenderBackendApiType api{ RenderBackendApiType::DX12 };
        RenderingPipelineType pipeline{ RenderingPipelineType::Deferred };
        ThreadingMode threading_mode{ ThreadingMode::TripleThread };
    };

    class RenderSettings {
    public:
        [[nodiscard]] static Bool Initialize(const RenderSettingsInitInfo& info);

        [[nodiscard]] static RenderBackendApiType GetRenderBackendApiType() { return m_api; }
        [[nodiscard]] static RenderingPipelineType GetRenderingPipelineType() { return m_pipeline; }
        [[nodiscard]] static ThreadingMode GetThreadingMode() { return m_threading_mode; }

        [[nodiscard]] static Bool IsGpuDrivenSupported() { return m_gpu_driven_supported; }
        static void SetGpuDrivenSupported(const Bool supported) { m_gpu_driven_supported = supported; }

    private:
        inline static RenderBackendApiType m_api{ RenderBackendApiType::None };
        inline static RenderingPipelineType m_pipeline{ RenderingPipelineType::None };
        inline static ThreadingMode m_threading_mode{ ThreadingMode::TripleThread };
        inline static Bool m_gpu_driven_supported{ false };
    };

} // dodoe
