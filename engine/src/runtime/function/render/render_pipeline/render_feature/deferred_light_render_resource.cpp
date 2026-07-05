// do@Redlive

#include "deferred_light_render_resource.h"

namespace dodoe {

    void DeferredLightRenderResource::reset() {
        m_constant_buffer = nullptr;
    }

    GfxBufferHandle DeferredLightRenderResource::getOrCreateConstantBuffer() {
        if (!m_constant_buffer) {
            m_constant_buffer = GDrawCommandList.createBuffer(
                GfxBufferDesc()
                    .setByteSize(256)
                    .setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                    .setDebugName("DeferredLightPass ConstantBuffer"));
        }
        return m_constant_buffer;
    }

} // dodoe
