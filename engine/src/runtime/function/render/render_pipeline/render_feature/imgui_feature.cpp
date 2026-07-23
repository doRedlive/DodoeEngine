// do@Redlive

#include "imgui_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_imgui_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/graphics/draw_command_list.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#endif

namespace dodoe {

    void ImGuiFeature::initialize(SharedRenderService& resources) {
        (void)resources;
#ifdef DODOE_DEBUG_ENABLED
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
            m_font_texture = GDrawCommandList.createTexture(font_desc, pixels, static_cast<Size_t>(width) * height * 4u);
            io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(m_font_texture.get()));
        }
#endif

        m_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(256)
                .setIsConstantBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                .setDebugName("ImGui ConstantBuffer"));

        m_binding_layout = GDrawCommandList.createBindingLayout(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .addItem(GfxBindingLayoutItem::PushConstants(0, 16))
                .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                .addItem(GfxBindingLayoutItem::Sampler(0)));
    }

    void ImGuiFeature::shutdown() {
#ifdef DODOE_DEBUG_ENABLED
        if (ImGui::GetCurrentContext()) {
            ImGui::GetIO().Fonts->SetTexID(ImTextureID_Invalid);
        }
#endif
        m_font_texture.reset();
        m_constant_buffer.reset();
        m_binding_layout.reset();
    }

    void ImGuiFeature::registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const {
        ImGuiPass{}.build(graph, context);
    }

} // namespace dodoe
