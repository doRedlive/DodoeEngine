// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_skybox_pass.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class SkyboxFeature final : public IRenderFeature {
        SharedRenderService* m_shared_render_service{nullptr};
        GfxBufferHandle m_skybox_cb{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void registerGraphImports(RenderGraphImportRegistry& imports,
                                  const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;
    };

} // namespace dodoe
