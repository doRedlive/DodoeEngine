// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/shadow/shadow_view_data.h"

namespace dodoe {

    class RenderScene;
    class RenderView;
    class RenderViewFamily;
    class PrimitiveSceneInfo;
    struct MeshPassRelevance;

    class ShadowSystem final {
    public:
        [[nodiscard]] static ShadowViewData BuildFrameData(const RenderView& view,
                                                            const RenderScene& scene);

        static void SetupView(const RenderScene& scene, RenderViewFamily& view_family);
    };

} // namespace dodoe
