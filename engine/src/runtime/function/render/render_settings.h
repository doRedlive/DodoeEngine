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

    struct RenderSettingsInitInfo {
        RenderBackendApiType api{ RenderBackendApiType::Vulkan };
        RenderingPipelineType pipeline{ RenderingPipelineType::ForwardPlus };
    };

    class RenderSettings {
    public:
        [[nodiscard]] static Bool Initialize(const RenderSettingsInitInfo& info);

        [[nodiscard]] static RenderBackendApiType GetRenderBackendApiType() { return m_api; }
        [[nodiscard]] static RenderingPipelineType GetRenderingPipelineType() { return m_pipeline; }

    private:
        inline static RenderBackendApiType m_api{ RenderBackendApiType::None };
        inline static RenderingPipelineType m_pipeline{ RenderingPipelineType::None };
    };

} // dodoe
