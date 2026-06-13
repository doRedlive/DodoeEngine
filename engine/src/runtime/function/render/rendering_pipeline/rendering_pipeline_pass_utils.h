// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_graph/render_graph_pass.h"

namespace dodoe::rendering_pipeline_utils {

    [[nodiscard]] inline Vector3f ExtractCameraPosition(const RenderView& view) {
        const Matrix4f inverse_view = Math::Inverse(view.getViewMatrix());
        return Vector3f(inverse_view[3]);
    }

    [[nodiscard]] inline ViewportState BuildViewportState(const RenderView& view, const Vector2i& fallback_extent) {
        const auto viewport_rect = view.getViewportRect();
        const Float offset_x = static_cast<Float>(viewport_rect.x);
        const Float offset_y = static_cast<Float>(viewport_rect.y);
        const Float width = viewport_rect.z > 0 ? static_cast<Float>(viewport_rect.z) : static_cast<Float>(fallback_extent.x);
        const Float height = viewport_rect.w > 0 ? static_cast<Float>(viewport_rect.w) : static_cast<Float>(fallback_extent.y);
        return ViewportState().addViewportAndScissorRect(
            Viewport(offset_x, offset_x + width, offset_y, offset_y + height, 0.0f, 1.0f)
        );
    }

    [[nodiscard]] inline Matrix4f BuildDirectionalLightViewProjection(const Vector3f& direction) {
        const Vector3f light_direction = glm::length(direction) > 0.0001f
            ? glm::normalize(direction)
            : glm::normalize(Vector3f(0.3f, -0.8f, -0.5f));
        const Vector3f light_eye = Vector3f(0.0f, 0.0f, 0.0f) - light_direction * 30.0f;
        const Matrix4f view = glm::lookAt(light_eye, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));
        const Matrix4f proj = glm::orthoRH_ZO(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 80.0f);
        return proj * view;
    }

    [[nodiscard]] inline GraphicsPipelineDesc BuildFullscreenPipelineDesc(
        const GfxShaderHandle& vertex_shader,
        const GfxShaderHandle& pixel_shader,
        const GfxBindingLayoutHandle& binding_layout)
    {
        auto pipeline_desc = GraphicsPipelineDesc()
            .setVertexShader(vertex_shader)
            .setPixelShader(pixel_shader)
            .addBindingLayout(binding_layout)
            .setPrimType(PrimitiveType::TriangleList);
        DepthStencilState depth_stencil_state;
        depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();
        RasterState raster_state;
        raster_state.setCullNone();
        RenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state);
        render_state.setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);
        return pipeline_desc;
    }

    inline void RecordSingleInputFullscreenPass(
        const RenderGraphPassContext& context,
        RenderGraphCommandList& command_list,
        const GfxGraphicsPipelineHandle& pipeline,
        const GfxBindingLayoutHandle& binding_layout,
        const GfxSamplerHandle& sampler,
        const RenderGraphTextureHandle input,
        const RenderGraphTextureHandle output,
        const char* marker)
    {
        const auto device = context.getGfxContext()->getDevice();
        const auto input_texture = command_list.resolveTexture(input);
        const auto output_texture = command_list.resolveTexture(output);
        auto framebuffer = device->createFramebuffer(FramebufferDesc().addColorAttachment(output_texture));
        auto binding_set = device->createBindingSet(
            BindingSetDesc()
                .addItem(BindingSetItem::Texture_SRV(0, input_texture))
                .addItem(BindingSetItem::Sampler(0, sampler)),
            binding_layout
        );
        const auto viewport_state = BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

        command_list.open();
        command_list.beginMarker(marker);
        command_list.setTextureState(input_texture, AllSubresources, ResourceStates::ShaderResource);
        command_list.setTextureState(output_texture, AllSubresources, ResourceStates::RenderTarget);
        command_list.commitBarriers();
        command_list.clearTextureFloat(output, AllSubresources, Color(0.0f, 0.0f, 0.0f, 1.0f));
        command_list.setGraphicsState(
            GraphicsState()
                .setPipeline(pipeline)
                .setFramebuffer(framebuffer)
                .setViewport(viewport_state)
                .addBindingSet(binding_set)
        );
        command_list.draw(DrawArguments().setVertexCount(6).setInstanceCount(1));
        command_list.setTextureState(output_texture, AllSubresources, ResourceStates::ShaderResource);
        command_list.commitBarriers();
        command_list.endMarker();
        command_list.close();
    }

} // namespace dodoe::rendering_pipeline_utils
