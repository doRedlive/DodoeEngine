// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_present_pass.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class PresentFeature final : public IRenderFeature {
        GfxBufferHandle m_present_cb{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void collectPasses(PassCollector& collector) override;
    };

} // namespace dodoe
