// do@Redlive

#include "render_view_family.h"
#include "render_view.h"
#include "../render_scene/render_scene.h"

namespace dodoe {

    RenderView& RenderViewFamily::createView(const Identifier id) {
        m_views.emplace_back(id);
        return m_views.back();
    }

    void RenderViewFamily::buildVisiblePrimitives(const RenderScene& scene) {
        for (auto& view : m_views) {
            view.buildVisiblePrimitives(scene);
            view.buildVisibleSprites(scene);
        }
    }

} // dodoe
