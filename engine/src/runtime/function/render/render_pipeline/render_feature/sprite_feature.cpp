// do@Redlive

#include "sprite_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_sprite_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

	void SpriteFeature::initialize(SharedRenderService& resources) {
	    auto* cache = resources.getBindingLayoutCache();
	    m_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::All)
	            .addItem(GfxBindingLayoutItem::Sampler(0))
	            .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));

	    m_traditional_tex_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Pixel)
	            .addItem(GfxBindingLayoutItem::Texture_SRV(0)));
	}

	void SpriteFeature::shutdown() {
	    m_binding_layout.reset();
	    m_traditional_tex_layout.reset();
	}

	void SpriteFeature::collectPasses(PassCollector& collector) {
	    collector.addPass<SpritePass>(m_binding_layout, m_traditional_tex_layout);
	}

} // namespace dodoe
