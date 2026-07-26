// do@Redlive

#pragma once

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class RenderTargetHandle;

    struct GBufferRenderTargetKey {
        using Value = RenderTargetHandle*;
    };

    struct ShadowMapRenderTargetKey {
        using Value = RenderTargetHandle*;
    };

    struct ImGuiFontTextureKey {
        using Value = GfxTextureHandle;
    };

    struct SkyboxConstantBufferKey {
        using Value = GfxBufferHandle;
    };

    struct PresentViewportConstantBufferKey {
        using Value = GfxBufferHandle;
    };

} // namespace dodoe
