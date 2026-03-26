// 
// Created by Redlive on 2026/3/25.
//

#include "sprite_stage.h"

#include "runtime/function/render/render_resource.h"

#include "runtime/resource/resource_manager.h"

namespace dodoe {

	void SpriteStage::initialize(const RenderStageCreateInfo& create_info) {
		(void)create_info;
		sprite_batch_ = RenderBatch::create({});
		sprite_pipeline_ = RenderPipeline::create({
			ResourceManager::self().load_shader("quad2d", "engine/res/shaders/quad2d.vert", "engine/res/shaders/quad2d.frag"),	
		});
	}

	void SpriteStage::shutdown() {
		RenderBatch::destroy(sprite_batch_);
		RenderPipeline::destroy(sprite_pipeline_);
	}

	void SpriteStage::flush() {
		sprite_pipeline_->attach();
		sprite_batch_->flush();
		sprite_pipeline_->detach();
	}

	void SpriteStage::queue_draw_context(const QuadDrawContext& context) {
		if (context.stage != RenderStageType::Sprite) return;
		sprite_batch_->queue_draw_context(context);
	}

} // dodoe
