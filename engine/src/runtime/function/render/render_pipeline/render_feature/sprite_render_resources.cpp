// do@Redlive

#include "sprite_render_resources.h"

#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/framework/descriptor_table_manager.h"
#include "runtime/function/render/framework/global_samplers.h"

namespace dodoe {

    void SpriteRenderResources::reset() {
        m_input_layout = nullptr;
        m_binding_layout = nullptr;
        m_binding_set = nullptr;
        m_pipeline = GfxGraphicsPipelineHandle{};
        m_framebuffer = nullptr;
        m_framebuffer_texture = nullptr;
        m_bound_vp_buffer = nullptr;
    }

    void SpriteRenderResources::renderSprites(
        const RenderPassContext& pass_context,
        const RenderGraphPassContext& context,
        DrawCommandList& command_list,
        const GfxTextureHandle& color_target,
        const GfxBufferHandle& quad_vertex_buffer,
        const GfxBufferHandle& quad_index_buffer,
        const GfxBufferHandle& instance_buffer,
        const GfxBufferHandle& vp_buffer,
        const UInt32 visible_count)
    {
        if (visible_count == 0) {
            return;
        }

        const auto device = context.getGfxContext()->getDevice();
        const auto* shader_library = pass_context.getShaderLibrary();
        const auto* pipeline_cache = pass_context.getPipelineStateCache();
        if (!device || !shader_library || !pipeline_cache) {
            return;
        }

        if (!m_framebuffer || !m_framebuffer_texture
            || m_framebuffer_texture->getDesc().format != color_target->getDesc().format
            || m_framebuffer_texture->getDesc().width != color_target->getDesc().width
            || m_framebuffer_texture->getDesc().height != color_target->getDesc().height) {
            m_framebuffer_texture = color_target;
            m_framebuffer = command_list.createFramebuffer(
                GfxFramebufferDesc().addColorAttachment(color_target));
            m_pipeline = GfxGraphicsPipelineHandle{};
        }

        const auto sprite_vs = shader_library->getSpriteVertexShader();
        const auto sprite_ps = shader_library->getSpritePixelShader();
        if (!sprite_vs || !sprite_ps) {
            return;
        }

        if (!m_input_layout) {
            constexpr UInt32 kQuadVertexStride = 28;
            GfxVertexAttributeDesc sprite_attribs[] = {
                GfxVertexAttributeDesc().setName("POSITION").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kQuadVertexStride),
                GfxVertexAttributeDesc().setName("TEXCOORD").setFormat(GfxFormat::RG32_FLOAT).setOffset(12).setElementStride(kQuadVertexStride),
                GfxVertexAttributeDesc().setName("COLOR").setFormat(GfxFormat::RGBA8_UNORM).setOffset(20).setElementStride(kQuadVertexStride),
                GfxVertexAttributeDesc().setName("TEXINDEX").setFormat(GfxFormat::R32_UINT).setOffset(24).setElementStride(kQuadVertexStride),
            };
            m_input_layout = device->createInputLayout(sprite_attribs, 4, sprite_vs);
        }

        if (!m_binding_layout) {
            m_binding_layout = device->createBindingLayout(
                GfxBindingLayoutDesc().setVisibility(GfxShaderType::All)
                    .addItem(GfxBindingLayoutItem::Sampler(0))
                    .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));
        }

        if (m_binding_layout && (!m_binding_set || !m_bound_vp_buffer || m_bound_vp_buffer->getRHIHandle() != vp_buffer->getRHIHandle())) {
            m_bound_vp_buffer = vp_buffer;
            m_binding_set = command_list.createBindingSet(
                GfxBindingSetDesc()
                    .addItem(GfxBindingSetItem::Sampler(0, GlobalSamplers::point()))
                    .addItem(GfxBindingSetItem::ConstantBuffer(0, vp_buffer->getRHIHandle())),
                m_binding_layout);
        }

        GfxBindingLayoutHandle desc_table_layout = nullptr;
        GfxBindingSetHandle desc_table = nullptr;
        if (pass_context.getTextureManager()) {
            auto* desc_mgr = pass_context.getTextureManager()->getDescriptorTable();
            if (desc_mgr && desc_mgr->getDescriptorTable()) {
                auto* raw_table = desc_mgr->getDescriptorTable();
                desc_table = create_ref<GfxBindingSet>(cutie::BindingSetHandle(raw_table));
                desc_table_layout = raw_table->getLayout();
            }
        }

        if (!m_pipeline && m_framebuffer && m_input_layout && m_binding_layout) {
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
                .setInputLayout(m_input_layout)
                .addBindingLayout(m_binding_layout)
                .setPrimType(GfxPrimitiveType::TriangleList)
                .setRenderState(render_state);
            if (desc_table_layout) {
                pipeline_desc.addBindingLayout(desc_table_layout);
            }
            m_pipeline = pipeline_cache->resolveGraphicsPipeline(pipeline_desc, m_framebuffer->getInfo(), command_list);
        }

        if (!m_pipeline || !m_framebuffer || !m_binding_set) {
            return;
        }

        DynamicArray<GfxBindingSetHandle> bs_arr = {m_binding_set};
        if (desc_table) {
            bs_arr.push_back(desc_table);
        }
        DynamicArray<GfxVertexBufferBinding> vbs;
        vbs.push_back(GfxVertexBufferBinding().setBuffer(quad_vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0));
        GfxIndexBufferBinding ib = GfxIndexBufferBinding().setBuffer(quad_index_buffer->getRHIHandle()).setFormat(GfxFormat::R16_UINT).setOffset(0);
        auto vp = GfxViewportState().addViewportAndScissorRect(GfxViewport(
            0, static_cast<float>(context.getGfxContext()->getSwapchainExtent2d().x),
            0, static_cast<float>(context.getGfxContext()->getSwapchainExtent2d().y),
            0, 1));

        command_list.setGraphicsState(m_framebuffer, m_pipeline, bs_arr, vp, vbs, ib);
        command_list.drawIndexed(GfxDrawArguments().setVertexCount(6).setInstanceCount(visible_count));
    }

} // dodoe
