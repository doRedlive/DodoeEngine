// do@Redlive

#include "render_reference_layer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

namespace dodoe {

    RenderReferenceLayer::RenderReferenceLayer(const String& name) :
        Layer(name.c_str()) {
    }

    void RenderReferenceLayer::attach() {
        auto* render_system = GetRenderSystem();
        auto* gfx = render_system ? render_system->getGfx() : nullptr;
        if (!gfx) {
            DO_ERROR("RenderReferenceLayer: graphics context is unavailable");
            return;
        }

        m_baseline = BaselineRenderer::Create({gfx->getDevice()});
        if (!m_baseline) {
            DO_ERROR("RenderReferenceLayer: failed to create baseline renderer");
            return;
        }

        BaselineRenderer* baseline = m_baseline.get();
        render_system->setBaselineRendererHook([baseline](GfxContext& gfx, UInt32 image_index) {
            baseline->render(gfx, image_index);
        });
        DO_INFO("RenderReferenceLayer: baseline renderer hook installed");
    }

    void RenderReferenceLayer::detach() {
        if (auto* render_system = GetRenderSystem()) {
            render_system->setBaselineRendererHook(nullptr);
        }
        BaselineRenderer::Destroy(m_baseline);
    }

} // namespace dodoe
