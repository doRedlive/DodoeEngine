// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_post_process_2d_pass.h"

namespace dodoe {

    class PostProcess2DFeature final : public IRenderFeature {
    public:
        void collectPasses(PassCollector& collector) override {
            collector.addPass<PostProcess2DPass>();
        }
    };

} // namespace dodoe
