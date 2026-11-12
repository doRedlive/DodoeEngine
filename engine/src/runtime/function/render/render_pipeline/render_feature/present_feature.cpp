// do@Redlive

#include "present_feature.h"

#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_service/shared_render_service.h"

namespace dodoe {

    void PresentFeature::collectPasses(PassCollector& collector) {
        collector.addPass<PresentPass>();
    }

} // namespace dodoe
