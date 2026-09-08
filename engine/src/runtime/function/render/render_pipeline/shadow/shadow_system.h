// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    class RenderScene;
    class RenderView;
    class RenderViewFamily;

    struct ShadowFrameData {
        Bool has_shadow{false};
        Vector3f light_direction{0.3f, -0.8f, -0.5f};
        Vector3f bounds_center{0.0f};
        Float bounds_extent{25.0f};
        Matrix4f light_view_projection{1.0f};
        Vector4f shadow_params{0.005f, 0.2f, 0.0f, 2.0f};
    };

    class ShadowSystem final {
    public:
        [[nodiscard]] static ShadowFrameData buildFrameData(const RenderView& view,
                                                             const RenderScene& scene);

        static void setupView(const RenderScene& scene, RenderViewFamily& view_family);
    };

} // namespace dodoe
