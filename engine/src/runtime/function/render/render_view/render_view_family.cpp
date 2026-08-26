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
        DO_PROFILE_SCOPE_CATEGORY("RenderViewFamily::buildVisiblePrimitives", "frame");
        for (auto& view : m_views) {
            view.buildVisiblePrimitives(scene);
            view.buildVisibleSprites(scene);
        }
    }

    void RenderViewFamily::buildVisibleSprites(const RenderScene& scene) {
        DO_PROFILE_SCOPE_CATEGORY("RenderViewFamily::buildVisibleSprites", "frame");
        for (auto& view : m_views) {
            view.buildVisibleSprites(scene);
        }
    }

} // dodoe
