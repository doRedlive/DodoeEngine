//
// Created by Redlive on 2026/3/25.
//

#ifndef DODOE_SPRITE_STAGE_H
#define DODOE_SPRITE_STAGE_H

#include "../render_stage.h"
#include "../render_batch.h"
#include "runtime/function/render/backend/render_process.h"

namespace dodoe {

    class SpriteStage : public RenderStage {
    public:
        void flush() override;
        void queue_draw_context(const QuadDrawContext& context) override;
    
        void initialize(const RenderStageCreateInfo& create_info) override;
        void shutdown() override;

    private:
		Scope<RenderBatch> sprite_batch_;
		Scope<RenderPipeline> sprite_pipeline_; 
    };

} // dodoe

#endif//DODOE_SPRITE_STAGE_H
