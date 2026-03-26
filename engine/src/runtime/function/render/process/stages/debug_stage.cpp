// 
// Created by Redlive on 2026/3/25.
//

#include "debug_stage.h"

#include "runtime/resource/resource_manager.h"

namespace dodoe {

    void DebugStage::initialize(const RenderStageCreateInfo& create_info) {
        (void)create_info;
        debug_batch_ = RenderBatch::create({});
        debug_pipeline_ = RenderPipeline::create({
            ResourceManager::self().load_shader("quad2d", "engine/res/shaders/quad2d.vert", "engine/res/shaders/quad2d.frag"),
        });
    }

    void DebugStage::shutdown() {
        RenderBatch::destroy(debug_batch_);
        RenderPipeline::destroy(debug_pipeline_);
    }

    void DebugStage::flush() {
        debug_pipeline_->attach();
        debug_batch_->flush();
        debug_pipeline_->detach();
    }

    
    void DebugStage::queue_draw_context(const QuadDrawContext& context) {
        if (context.stage != RenderStageType::Debug) return;
        debug_batch_->queue_draw_context(context);
    }

} // dodoe
