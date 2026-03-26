//
// Created by Redlive on 2026/3/25.
//

#ifndef DODOE_DEBUG_STAGE_H
#define DODOE_DEBUG_STAGE_H

#include "../render_stage.h"

namespace dodoe {

    class DebugStage : public RenderStage {
    public:
        void flush() override;
        void queue_draw_context(const QuadDrawContext& context) override;

        void initialize(const RenderStageCreateInfo& create_info) override;
        void shutdown() override;
    };

} // dodoe

#endif//DODOE_DEBUG_STAGE_H
