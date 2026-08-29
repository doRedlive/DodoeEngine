// do@Redlive

#include "skybox_feature.h"

#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    void SkyboxFeature::initialize(SharedRenderService& resources) {
        m_shared_render_service = &resources;

        auto* gfx = resources.getGfxContext();
        m_skybox_cb = create_ref<GfxBuffer>(
            GfxBufferDesc()
                .setByteSize(sizeof(Matrix4f))
                .setIsConstantBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                .setDebugName("SkyboxCB"));
        m_skybox_cb->initializeGpu(gfx->getDevice());
    }

    void SkyboxFeature::shutdown() {
        m_skybox_cb.reset();
    }

    void SkyboxFeature::registerGraphImports(RenderGraphImportRegistry& imports,
                                             const RenderView& view) {
        (void)view;
        if (m_skybox_cb) {
            imports.publish<SkyboxConstantBufferKey>(m_skybox_cb);
        }
    }

    void SkyboxFeature::collectPasses(PassCollector& collector) {
        collector.addPass<SkyboxPass>();
    }

} // namespace dodoe
