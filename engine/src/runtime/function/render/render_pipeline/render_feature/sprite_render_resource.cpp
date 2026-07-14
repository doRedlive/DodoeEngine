// do@Redlive

#include "sprite_render_resource.h"

namespace dodoe {

    void SpriteRenderResource::reset() {
        m_binding_layout = nullptr;
        m_pipeline = GfxGraphicsPipelineHandle{};
        m_pipeline_traditional = GfxGraphicsPipelineHandle{};
        m_framebuffer = nullptr;
        m_framebuffer_texture = nullptr;
        m_traditional_tex_layout = nullptr;
    }

    GfxFramebufferHandle SpriteRenderResource::getOrCreateFramebuffer(
        DrawCommandList& command_list,
        const GfxTextureHandle& color_target)
    {
        if (!m_framebuffer || !m_framebuffer_texture
            || m_framebuffer_texture != color_target) {
            m_framebuffer_texture = color_target;
            m_framebuffer = command_list.createFramebuffer(
                GfxFramebufferDesc().addColorAttachment(color_target));
            m_pipeline = GfxGraphicsPipelineHandle{};
            m_pipeline_traditional = GfxGraphicsPipelineHandle{};
        }
        return m_framebuffer;
    }

    GfxBindingLayoutHandle SpriteRenderResource::getOrCreateBindingLayout(
        DrawCommandList& command_list)
    {
        if (!m_binding_layout) {
            m_binding_layout = command_list.createBindingLayout(
                GfxBindingLayoutDesc().setVisibility(GfxShaderType::All)
                    .addItem(GfxBindingLayoutItem::Sampler(0))
                    .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));
        }
        return m_binding_layout;
    }

    GfxGraphicsPipelineHandle SpriteRenderResource::getOrCreatePipeline(
        PipelineStateCache* pipeline_cache,
        GfxShaderHandle sprite_vs,
        GfxShaderHandle sprite_ps,
        GfxInputLayoutHandle input_layout,
        const GfxFramebufferInfo& framebuffer_info,
        DrawCommandList& command_list,
        GfxBindingLayoutHandle desc_table_layout)
    {
        if (!m_pipeline && m_framebuffer && input_layout && m_binding_layout) {
            GfxBlendState blend;
            blend.targets[0]
                .enableBlend()
                .setSrcBlend(GfxBlendFactor::SrcAlpha)
                .setDestBlend(GfxBlendFactor::InvSrcAlpha)
                .setBlendOp(GfxBlendOp::Add)
                .setSrcBlendAlpha(GfxBlendFactor::One)
                .setDestBlendAlpha(GfxBlendFactor::InvSrcAlpha)
                .setBlendOpAlpha(GfxBlendOp::Add);
            GfxDepthStencilState ds;
            ds.disableDepthTest().disableDepthWrite().disableStencil();
            GfxRenderState render_state;
            render_state.setBlendState(blend).setDepthStencilState(ds);

            auto pipeline_desc = GfxGraphicsPipelineDesc()
                .setVertexShader(sprite_vs)
                .setPixelShader(sprite_ps)
                .setInputLayout(input_layout)
                .addBindingLayout(m_binding_layout)
                .setPrimType(GfxPrimitiveType::TriangleList)
                .setRenderState(render_state);
            if (desc_table_layout) {
                pipeline_desc.addBindingLayout(desc_table_layout);
            }
            m_pipeline = pipeline_cache->resolveGraphicsPipeline(pipeline_desc, framebuffer_info, command_list);
        }
        return m_pipeline;
    }

    GfxBindingLayoutHandle SpriteRenderResource::getOrCreateTraditionalTextureLayout(
        DrawCommandList& command_list)
    {
        if (!m_traditional_tex_layout) {
            m_traditional_tex_layout = command_list.createBindingLayout(
                GfxBindingLayoutDesc().setVisibility(GfxShaderType::Pixel)
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0)));
        }
        return m_traditional_tex_layout;
    }

    GfxGraphicsPipelineHandle SpriteRenderResource::getOrCreateTraditionalPipeline(
        PipelineStateCache* pipeline_cache,
        GfxShaderHandle sprite_vs,
        GfxShaderHandle sprite_ps,
        GfxInputLayoutHandle input_layout,
        const GfxFramebufferInfo& framebuffer_info,
        DrawCommandList& command_list)
    {
        if (!m_pipeline_traditional && m_framebuffer && input_layout && m_binding_layout && m_traditional_tex_layout) {
            GfxBlendState blend;
            blend.targets[0]
                .enableBlend()
                .setSrcBlend(GfxBlendFactor::SrcAlpha)
                .setDestBlend(GfxBlendFactor::InvSrcAlpha)
                .setBlendOp(GfxBlendOp::Add)
                .setSrcBlendAlpha(GfxBlendFactor::One)
                .setDestBlendAlpha(GfxBlendFactor::InvSrcAlpha)
                .setBlendOpAlpha(GfxBlendOp::Add);
            GfxDepthStencilState ds;
            ds.disableDepthTest().disableDepthWrite().disableStencil();
            GfxRenderState render_state;
            render_state.setBlendState(blend).setDepthStencilState(ds);

            auto pipeline_desc = GfxGraphicsPipelineDesc()
                .setVertexShader(sprite_vs)
                .setPixelShader(sprite_ps)
                .setInputLayout(input_layout)
                .addBindingLayout(m_binding_layout)
                .addBindingLayout(m_traditional_tex_layout)
                .setPrimType(GfxPrimitiveType::TriangleList)
                .setRenderState(render_state);
            m_pipeline_traditional = pipeline_cache->resolveGraphicsPipeline(pipeline_desc, framebuffer_info, command_list);
        }
        return m_pipeline_traditional;
    }

} // dodoe
