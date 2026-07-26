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
        using Value = SceneTextures;
    };
    struct ShadowMapKey {
        using Value = RenderGraphTextureHandle;
    };
    struct SceneHdrKey {
        using Value = RenderGraphTextureHandle;
    };
    struct ToneMappedColorKey {
        using Value = RenderGraphTextureHandle;
    };
    struct SceneColorKey {
        using Value = RenderGraphTextureHandle;
    };
    struct FxaaColorKey {
        using Value = RenderGraphTextureHandle;
    };
    struct SpriteColorKey {
        using Value = RenderGraphTextureHandle;
    };
    struct ImGuiColorKey {
        using Value = RenderGraphTextureHandle;
    };

} // dodoe
