// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class RenderTargetHandle;

    struct GBufferRenderTargetKey {
        using Value = RenderTargetHandle*;
    };

    struct ShadowMapRenderTargetKey {
        using Value = RenderTargetHandle*;
    };

    struct TaaHistoryReadKey {
        using Value = RenderTargetHandle*;
    };

    struct TaaHistoryWriteKey {
        using Value = RenderTargetHandle*;
    };

    struct TaaPrevDepthReadKey {
        using Value = RenderTargetHandle*;
    };

    struct TaaPrevDepthWriteKey {
        using Value = RenderTargetHandle*;
    };

    struct TaaFrameParams {
        Matrix4f prev_unjittered_view_projection{1.0f};
        Matrix4f current_unjittered_view_projection{1.0f};
        Vector2f current_jitter_uv{0.0f, 0.0f};
        Vector2f prev_jitter_uv{0.0f, 0.0f};
        Bool reset_history{false};
    };

    struct TaaFrameParamsKey {
        using Value = TaaFrameParams;
    };

    struct ImGuiFontTextureKey {
        using Value = GfxTextureHandle;
    };

    struct ImGuiConstantBufferKey {
        using Value = GfxBufferHandle;
    };

    struct SkyboxConstantBufferKey {
        using Value = GfxBufferHandle;
    };

    struct PresentViewportConstantBufferKey {
        using Value = GfxBufferHandle;
    };

} // namespace dodoe
