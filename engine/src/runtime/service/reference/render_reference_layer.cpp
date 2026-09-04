// do@Redlive

#include "render_reference_layer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/service/world/scene_importer.h"

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

        auto* shared_render_service = render_system->getSharedRenderService();
        const auto* shader_library = shared_render_service ? shared_render_service->getShaderLibrary() : nullptr;

        m_baseline = BaselineRenderer::Create({gfx->getDevice(), shader_library, shared_render_service});
        if (!m_baseline) {
            DO_ERROR("RenderReferenceLayer: failed to create baseline renderer");
            return;
        }

        BaselineRenderer* baseline = m_baseline.get();
        render_system->setBaselineRendererHook([baseline](GfxContext& gfx, UInt32 image_index,
                                                          RenderViewFamily& view_family, RenderScene& scene) {
            baseline->render(gfx, image_index, view_family, scene);
        });
        DO_INFO("RenderReferenceLayer: baseline renderer hook installed");

        SceneImporter::ImportModel("Models/backpack/backpack.obj");
        SceneImporter::ImportModel("Models/marry/Marry.obj");
    }

    void RenderReferenceLayer::detach() {
        if (auto* render_system = GetRenderSystem()) {
            render_system->setBaselineRendererHook(nullptr);
        }
        BaselineRenderer::Destroy(m_baseline);
    }

} // namespace dodoe
