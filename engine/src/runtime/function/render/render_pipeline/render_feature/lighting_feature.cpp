// do@Redlive

#include "lighting_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_deferred_light_pass.h"

namespace dodoe {

    void LightingFeature::collectPasses(PassCollector& collector) {
        collector.addPass<DeferredLightPass>();
    }

} // dodoe
