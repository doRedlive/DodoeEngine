// do@Redlive

#pragma once

#include "dopch.h"

#include "render_scene.h"

namespace dodoe {

    struct RenderSceneFamilyCreateInfo {

    };

    class RenderSceneFamily : public Managed<RenderSceneFamily, RenderSceneFamilyCreateInfo> {
        // Scope<RenderScene> m_active_scene;
    public:

    private:
        Bool initialize(const RenderSceneFamilyCreateInfo& info);
        void shutdown();
    };

} // dodoe