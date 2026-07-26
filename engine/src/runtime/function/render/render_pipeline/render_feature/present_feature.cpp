// do@Redlive

#include "present_feature.h"

#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_service/shared_render_service.h"

namespace dodoe {

    void PresentFeature::initialize(SharedRenderService& resources) {
        auto* gfx = resources.getGfxContext();
        m_present_cb = create_ref<GfxBuffer>(
            GfxBufferDesc()
                .setByteSize(16)
                .setIsConstantBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                .setDebugName("PresentViewportCB"));
        m_present_cb->initializeRHI(gfx->getDevice());
    }

    void PresentFeature::shutdown() {
        m_present_cb.reset();
    }

    void PresentFeature::registerGraphImports(RenderGraphImportRegistry& imports,
                                              const RenderView& view) {
        if (m_present_cb) {
            imports.publish<PresentViewportConstantBufferKey>(m_present_cb);
        }
    }

    void PresentFeature::collectPasses(PassCollector& collector) {
        collector.addPass<PresentPass>();
    }

} // namespace dodoe
