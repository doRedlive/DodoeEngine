// do@Redlive

#include "only_2d_renderer.h"

#include "render_feature/render_builtin_features.h"
#include "render_feature/sprite_feature.h"
#include "render_feature/imgui_feature.h"

namespace dodoe {

    Bool Only2DRenderer::initialize(const RendererCreateInfo& info) {
        if (!initializeBase(info)) {
            return false;
        }

        m_features.push_back(create_scope<SpriteFeature>());
        m_features.push_back(create_scope<PostProcess2DFeature>());
        m_features.push_back(create_scope<ImGuiFeature>());
        m_features.push_back(create_scope<PresentFeature>());
        return true;
    }

    void Only2DRenderer::shutdown() {
        shutdownBase();
    }

    void Only2DRenderer::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
        RendererBase::initViews(scene, view_family);
        view_family.buildVisibleSprites(scene);
    }

    void Only2DRenderer::render(RenderViewFamily& view_family, RenderScene& scene,
                                 const UInt32 swapchain_image_index, DrawCommandList& out_commands) {
        initViews(scene, view_family);
        buildFrameDrawCommandList(view_family, scene, swapchain_image_index, out_commands);
    }

} // dodoe
