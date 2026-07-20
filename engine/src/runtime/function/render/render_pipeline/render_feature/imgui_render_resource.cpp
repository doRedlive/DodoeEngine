// do@Redlive

#include "imgui_render_resource.h"

#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"
#endif

#include <cstring>

namespace dodoe {

    void ImGuiRenderResource::reset() {
#ifdef DODOE_DEBUG_ENABLED
        if (ImGui::GetCurrentContext()) {
            ImGui::GetIO().Fonts->SetTexID(ImTextureID_Invalid);
        }
#endif
        m_font_texture = nullptr;
        m_constant_buffer = nullptr;
        m_binding_layout = nullptr;
        m_pipeline = GfxGraphicsPipelineHandle{};
        m_framebuffer = nullptr;
        m_framebuffer_texture = nullptr;
        m_binding_sets.clear();
    }

    GfxTextureHandle ImGuiRenderResource::getOrCreateFontTexture(
        DrawCommandList& command_list)
    {
#ifdef DODOE_DEBUG_ENABLED
        if (!m_font_texture) {
            ImGuiIO& io = ImGui::GetIO();
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            if (pixels && width > 0 && height > 0) {
                GfxTextureDesc font_desc = GfxTextureDesc()
                    .setDimension(GfxTextureDimension::Texture2D)
                    .setWidth(width)
                    .setHeight(height)
                    .setFormat(GfxFormat::RGBA8_UNORM)
                    .setMipLevels(1)
                    .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                    .setDebugName("ImGui Font Texture");
                m_font_texture = command_list.createTexture(font_desc, pixels, static_cast<Size_t>(width) * height * 4u);
                io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(m_font_texture.get()));
            }
        }
        return m_font_texture;
#else
        (void)command_list;
        return nullptr;
#endif
    }

    GfxBufferHandle ImGuiRenderResource::getOrCreateConstantBuffer(
        DrawCommandList& command_list)
    {
#ifdef DODOE_DEBUG_ENABLED
        if (!m_constant_buffer) {
            m_constant_buffer = command_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(256)
                    .setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                    .setDebugName("ImGui ConstantBuffer"));
            m_binding_sets.clear();
        }
        return m_constant_buffer;
#else
        (void)command_list;
        return nullptr;
#endif
    }

    void ImGuiRenderResource::invalidateBindingSets() {
        m_binding_sets.clear();
    }

    GfxBindingLayoutHandle ImGuiRenderResource::getOrCreateBindingLayout(
        DrawCommandList& command_list)
    {
#ifdef DODOE_DEBUG_ENABLED
        if (!m_binding_layout) {
            m_binding_layout = command_list.createBindingLayout(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::All)
                    .addItem(GfxBindingLayoutItem::PushConstants(0, 16))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Sampler(0)));
            m_pipeline = GfxGraphicsPipelineHandle{};
            m_binding_sets.clear();
        }
        return m_binding_layout;
#else
        (void)command_list;
        return nullptr;
#endif
    }

    GfxFramebufferHandle ImGuiRenderResource::getOrCreateFramebuffer(
        DrawCommandList& command_list,
        const GfxTextureHandle& output)
    {
#ifdef DODOE_DEBUG_ENABLED
        if (!m_framebuffer || !m_framebuffer_texture
            || m_framebuffer_texture != output) {
            m_framebuffer_texture = output;
            m_framebuffer = command_list.createFramebuffer(
                GfxFramebufferDesc().addColorAttachment(output));
            m_pipeline = GfxGraphicsPipelineHandle{};
        }
        return m_framebuffer;
#else
        (void)command_list;
        (void)output;
        return nullptr;
#endif
    }

    GfxGraphicsPipelineHandle ImGuiRenderResource::getOrCreatePipeline(
        PipelineStateCache* pipeline_cache,
        GfxShaderHandle imgui_vs,
        GfxShaderHandle imgui_ps,
        GfxInputLayoutHandle input_layout,
        const GfxFramebufferInfo& framebuffer_info,
        DrawCommandList& command_list)
    {
#ifdef DODOE_DEBUG_ENABLED
        if (!m_pipeline && m_framebuffer && input_layout && m_binding_layout && pipeline_cache && imgui_vs && imgui_ps) {
            GfxDepthStencilState depth_stencil_state;
            depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();

            GfxBlendState blend_state;
            blend_state.targets[0]
                .enableBlend()
                .setSrcBlend(GfxBlendFactor::SrcAlpha)
                .setDestBlend(GfxBlendFactor::InvSrcAlpha)
                .setBlendOp(GfxBlendOp::Add)
                .setSrcBlendAlpha(GfxBlendFactor::One)
                .setDestBlendAlpha(GfxBlendFactor::InvSrcAlpha)
                .setBlendOpAlpha(GfxBlendOp::Add);

            GfxRasterState raster_state;
            raster_state.setCullNone().setScissorEnable(false);

            GfxRenderState render_state;
            render_state.setBlendState(blend_state);
            render_state.setDepthStencilState(depth_stencil_state);
            render_state.setRasterState(raster_state);

            auto pipeline_desc = GfxGraphicsPipelineDesc()
                .setInputLayout(input_layout)
                .setVertexShader(imgui_vs)
                .setPixelShader(imgui_ps)
                .addBindingLayout(m_binding_layout)
                .setPrimType(GfxPrimitiveType::TriangleList)
                .setRenderState(render_state);
            m_pipeline = pipeline_cache->resolveGraphicsPipeline(pipeline_desc, framebuffer_info, command_list);
        }
        return m_pipeline;
#else
        (void)pipeline_cache;
        (void)imgui_vs;
        (void)imgui_ps;
        (void)input_layout;
        (void)framebuffer_info;
        (void)command_list;
        return nullptr;
#endif
    }

    GfxBindingSetHandle ImGuiRenderResource::getOrCreateBindingSet(
        DrawCommandList& command_list,
        GfxTexture* texture)
    {
#ifdef DODOE_DEBUG_ENABLED
        if (!texture) {
            return nullptr;
        }
        const auto found = m_binding_sets.find(texture);
        if (found != m_binding_sets.end() && found->second) {
            return found->second;
        }
        auto binding_set = command_list.createBindingSet(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::PushConstants(0, sizeof(float) * 4))
                .addItem(GfxBindingSetItem::Texture_SRV(0, texture->getRHIHandle()))
                .addItem(GfxBindingSetItem::Sampler(0, GlobalSamplers::screen())),
            m_binding_layout);
        if (binding_set) {
            m_binding_sets.emplace(texture, binding_set);
        }
        return binding_set;
#else
        (void)command_list;
        (void)texture;
        return nullptr;
#endif
    }

} // dodoe
