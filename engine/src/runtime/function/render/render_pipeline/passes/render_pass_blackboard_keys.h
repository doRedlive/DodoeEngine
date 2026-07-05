// do@Redlive

#pragma once

#include "runtime/function/render/render_graph/render_graph_resource.h"

namespace dodoe {

    struct SceneTextures {
        RenderGraphTextureHandle albedo{};
        RenderGraphTextureHandle normal{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle material{};
        RenderGraphTextureHandle depth{};
        RenderGraphBufferHandle instance_scene_data{};
    };

    struct SceneTexturesKey {
    };
    struct ShadowMapKey {
    };
    struct SceneHdrKey {
    };
    struct ToneMappedColorKey {
    };
    struct SceneColorKey {
    };
    struct FxaaColorKey {
    };
    struct SpriteColorKey {
    };
    struct ImGuiColorKey {
    };

} // dodoe
