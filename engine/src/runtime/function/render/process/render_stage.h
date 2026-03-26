//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_RENDER_STAGE_H
#define DODOE_RENDER_STAGE_H

#include "dopch.h"

namespace dodoe {

	struct QuadDrawContext;

	struct RenderStageCreateInfo {

	};

	class RenderStage {
	public:
		virtual ~RenderStage() = default;

		template <typename TStage>
		static Scope<RenderStage> create(const RenderStageCreateInfo& create_info) {
			static_assert(std::is_base_of_v<RenderStage, TStage>, "T must inherit from RenderStage");
			auto stage = create_scope<TStage>();
			stage->initialize(create_info);
			return stage;
		}

		static void destroy(Scope<RenderStage>& render_stage) {
			if (!render_stage) { return; }
			render_stage->shutdown();
			render_stage.reset();
		}

		virtual void flush() = 0;
		virtual void queue_draw_context(const QuadDrawContext& context) = 0;

		virtual void initialize(const RenderStageCreateInfo& create_info) = 0;
		virtual void shutdown() = 0;
	};

} // dodoe

#endif//DODOE_RENDER_STAGE_H
