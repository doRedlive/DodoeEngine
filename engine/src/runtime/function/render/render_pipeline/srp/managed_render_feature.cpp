// do@Redlive

#include "managed_render_feature.h"

#include "managed_render_pass.h"
#include "srp_bridge.h"

namespace dodoe {

    void ManagedRenderFeature::initialize(SharedRenderService& resources) {
        (void)resources;
        SrpBridge::Self().featureInitialize(m_managed_id);
    }

    void ManagedRenderFeature::onResize(const UInt32 width, const UInt32 height) {
        SrpBridge::Self().featureOnResize(m_managed_id, width, height);
    }

    void ManagedRenderFeature::shutdown() {
        SrpBridge::Self().featureDispose(m_managed_id);
    }

    void ManagedRenderFeature::collectPasses(PassCollector& collector) {
        collector.addPass<ManagedRenderPass>(m_managed_id, m_phase);
    }

} // dodoe
