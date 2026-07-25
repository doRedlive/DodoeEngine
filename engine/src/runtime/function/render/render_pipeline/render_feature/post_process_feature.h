// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_post_process_pass.h"

namespace dodoe {

    class PostProcessFeature final : public IRenderFeature {
    public:
        void collectPasses(PassCollector& collector) override {
            collector.addPass<PostProcessPass>();
        }
    };

} // namespace dodoe
