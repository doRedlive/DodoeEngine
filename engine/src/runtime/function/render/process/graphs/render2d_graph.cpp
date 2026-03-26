//
// Created by Redlive on 2026/3/25.
//

#include "render2d_graph.h"

#include "../stages/sprite_stage.h"
#include "../stages/debug_stage.h"

namespace dodoe {

    void Render2dGraph::initialize(const RenderGraphCreateInfo& create_info) {
        sprite_stage_ = RenderStage::create<SpriteStage>({});
        debug_stage_  = RenderStage::create<DebugStage>({});
    }

    void Render2dGraph::shutdown() {
        RenderStage::destroy(sprite_stage_);
        RenderStage::destroy(debug_stage_);
    }

    void Render2dGraph::prepare() {
        if (g_render_resource) {
            auto contexts = g_render_resource->gain_quad_draw_contexts();
            for (const auto& context : contexts) {
                switch (context.stage) {
                    case RenderStageType::Debug:
                        debug_stage_->queue_draw_context(context);
                        break;
                    case RenderStageType::Ui:
                    case RenderStageType::Sprite:
                    default:
                        sprite_stage_->queue_draw_context(context);
                        break;
                }
            }
        }
    }

    void Render2dGraph::flush() {
        sprite_stage_->flush();
        debug_stage_->flush();
    }

} // dodoe
