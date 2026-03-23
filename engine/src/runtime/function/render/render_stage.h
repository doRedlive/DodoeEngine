//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_RENDER_STAGE_H
#define DODOE_RENDER_STAGE_H

#include "dopch.h"

#include "backend/draw_defs.h"
#include "backend/shader.h"
#include "backend/frame_buffer.h"
#include "backend/render_process.h"
#include "render_batch.h"

#include "camera/camera.h"

namespace dodoe {

	struct RenderStageCreateInfo {
		ui32 framebuffer_width{0};
		ui32 framebuffer_height{0};
		Camera* camera{nullptr};
	};

	class RenderStage {
	public:
		static Scope<RenderStage> create(RenderStageCreateInfo create_info);
		static void destroy(Scope<RenderStage>& render_stage);

		void queue_draw_context(const TextureDrawContext& draw_context);
		void flush();

	private:
		Scope<RenderBatch> render_batch_;
		Scope<FrameBuffer> frame_buffer_;
		Scope<RenderPipeline> render_pipeline_; 

		void initialize(RenderStageCreateInfo create_info);
		void shutdown();
	};

} // dodoe

#endif//DODOE_RENDER_STAGE_H
