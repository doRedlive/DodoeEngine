// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_graph/render_graph_pass.h"
#include "../render_view/render_view_family.h"
#include "runtime/core/math/math.h"

namespace dodoe::rendering_pipeline_utils {

    inline RenderGraphTextureDesc MakeSwapchainRT2D(
        const Vector2i& swapchain_extent,
        const GfxFormat format,
        const String& debug_name)
    {
        return MakeRenderTarget2D(
            static_cast<UInt32>(swapchain_extent.x),
            static_cast<UInt32>(swapchain_extent.y),
            format, debug_name);
    }

    inline RenderGraphTextureDesc MakeSwapchainDepth2D(
        const Vector2i& swapchain_extent,
        const GfxFormat format,
        const String& debug_name)
    {
        return MakeDepthTarget2D(
            static_cast<UInt32>(swapchain_extent.x),
            static_cast<UInt32>(swapchain_extent.y),
            format, debug_name);
    }

    [[nodiscard]] inline Vector3f ExtractCameraPosition(const RenderView& view) {
        const Matrix4f inverse_view = Math::Inverse(view.getViewMatrix());
        return Vector3f(inverse_view[3]);
    }

    [[nodiscard]] inline GfxViewportState BuildViewportState(const RenderView& view, const Vector2i& fallback_extent) {
        const auto viewport_rect = view.getViewportRect();
        const Float offset_x = static_cast<Float>(viewport_rect.x);
        const Float offset_y = static_cast<Float>(viewport_rect.y);
        const Float width = viewport_rect.z > 0 ? static_cast<Float>(viewport_rect.z) : static_cast<Float>(fallback_extent.x);
        const Float height = viewport_rect.w > 0 ? static_cast<Float>(viewport_rect.w) : static_cast<Float>(fallback_extent.y);
        return GfxViewportState().addViewportAndScissorRect(
            GfxViewport(offset_x, offset_x + width, offset_y, offset_y + height, 0.0f, 1.0f)
        );
    }

    [[nodiscard]] inline Matrix4f BuildDirectionalLightViewProjection(const Vector3f& direction) {
        const Vector3f light_direction = Math::Length(direction) > 0.0001f
            ? Math::Normalize(direction)
            : Math::Normalize(Vector3f(0.3f, -0.8f, -0.5f));
        const Vector3f light_eye = Vector3f(0.0f, 0.0f, 0.0f) - light_direction * 30.0f;
        const Matrix4f view = Math::LookAt(light_eye, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));
        const Matrix4f proj = Math::OrthoRH_ZO(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 80.0f);
        return proj * view;
    }

    [[nodiscard]] inline GfxGraphicsPipelineDesc BuildFullscreenPipelineDesc(
        const GfxShaderHandle& vertex_shader,
        const GfxShaderHandle& pixel_shader,
        const GfxBindingLayoutHandle& binding_layout,
        const Bool additive_blend = false)
    {
        if (!binding_layout) {
            DO_ERROR("BuildFullscreenPipelineDesc: binding_layout is null!");
        }
        if (!vertex_shader) {
            DO_ERROR("BuildFullscreenPipelineDesc: vertex_shader is null!");
        }
        if (!pixel_shader) {
            DO_ERROR("BuildFullscreenPipelineDesc: pixel_shader is null!");
        }
        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(vertex_shader)
            .setPixelShader(pixel_shader)
            .addBindingLayout(binding_layout)
            .setPrimType(GfxPrimitiveType::TriangleList);
        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state);
        render_state.setRasterState(raster_state);
        if (additive_blend) {
            GfxBlendState blend_state;
            GfxBlendState::RenderTarget blend_target;
            blend_target.enableBlend()
                .setSrcBlend(GfxBlendFactor::One)
                .setDestBlend(GfxBlendFactor::One);
            blend_state.setRenderTarget(0, blend_target);
            render_state.setBlendState(blend_state);
        }
        pipeline_desc.setRenderState(render_state);
        return pipeline_desc;
    }

} // namespace dodoe::rendering_pipeline_utils
