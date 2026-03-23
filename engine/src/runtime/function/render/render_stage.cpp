//
// Created by Redlive on 2026/3/17.
//

#include "render_stage.h"

#include "runtime/resource/resource_manager.h"

namespace dodoe {
	Scope<RenderStage> RenderStage::create(RenderStageCreateInfo create_info) {
		auto context = create_scope<RenderStage>();
		context->initialize(create_info);
		return context;
	}

	void RenderStage::destroy(Scope<RenderStage>& render_stage) {
		if (!render_stage) {
			return;
		}

		render_stage->shutdown();
		render_stage.reset();
	}

	void RenderStage::initialize(RenderStageCreateInfo create_info) {
		render_batch_ = RenderBatch::create({});
		frame_buffer_ = FrameBuffer::create({create_info.framebuffer_width, create_info.framebuffer_height});
		render_pipeline_ = RenderPipeline::create({
			ResourceManager::self().load_shader("quad2d", "engine/res/shaders/quad2d.vert", "engine/res/shaders/quad2d.frag"),
			create_info.camera
		});
	}

	void RenderStage::shutdown() {
		FrameBuffer::destroy(frame_buffer_);
		RenderBatch::destroy(render_batch_);
		RenderPipeline::destroy(render_pipeline_);
	}

	void RenderStage::queue_draw_context(const TextureDrawContext& draw_context) {
		if (!render_batch_) {
			return;
		}

		render_batch_->queue_draw_context(draw_context);
	}

	void RenderStage::flush() {
		DoAssert(render_batch_, "Renderbatch is null!");

		render_pipeline_->attach();
		render_batch_->flush();
	}

} // dodoe
