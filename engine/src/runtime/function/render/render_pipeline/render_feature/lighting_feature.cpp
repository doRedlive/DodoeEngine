// do@Redlive

#include "lighting_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_deferred_light_pass.h"
#include "runtime/function/render/render_service/shared_render_service.h"

namespace dodoe {

    void LightingFeature::initialize(SharedRenderService& resources) {
        GfxBufferDesc()
            .setByteSize(256)
            .setIsConstantBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
            .setDebugName("DeferredLightPass ConstantBuffer");
    }

    void LightingFeature::shutdown() {
        m_resource.constant_buffer.reset();
    }

    void LightingFeature::exportResources(ResourceRegistry& registry,
                                          const RenderView& view) {
        registry.registerBuffer("DeferredLightConstantBuffer", m_resource.constant_buffer);
    }

    void LightingFeature::collectPasses(PassCollector& collector) {
        collector.addPass<DeferredLightPass>(m_resource.constant_buffer);
    }

} // dodoe
