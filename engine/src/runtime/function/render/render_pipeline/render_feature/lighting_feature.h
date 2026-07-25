// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct LightingFeatureResource {
        GfxBufferHandle constant_buffer{};
    };

    class LightingFeature final : public IRenderFeature {
    private:
        LightingFeatureResource m_resource{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void exportResources(ResourceRegistry& registry,
                             const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;

        [[nodiscard]] const LightingFeatureResource& resource() const { return m_resource; }
    };

} // namespace dodoe
