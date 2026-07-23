// do@Redlive

#include "sprite_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_sprite_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    void SpriteFeature::initialize(SharedRenderService& resources) {
        (void)resources;
        m_binding_layout = GDrawCommandList.createBindingLayout(
            GfxBindingLayoutDesc().setVisibility(GfxShaderType::All)
                .addItem(GfxBindingLayoutItem::Sampler(0))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));

        m_traditional_tex_layout = GDrawCommandList.createBindingLayout(
            GfxBindingLayoutDesc().setVisibility(GfxShaderType::Pixel)
                .addItem(GfxBindingLayoutItem::Texture_SRV(0)));
    }

    void SpriteFeature::shutdown() {
        m_binding_layout.reset();
        m_traditional_tex_layout.reset();
    }

    void SpriteFeature::registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const {
        SpritePass{}.build(graph, context);
    }

} // namespace dodoe
