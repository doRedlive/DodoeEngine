// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_feature/render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_msaa_resolve_pass.h"

namespace dodoe {

    class MsaaResolveFeature final : public IRenderFeature {
    public:
        void collectPasses(PassCollector& collector) override {
            collector.addPass<MsaaResolvePass>();
        }
    };

} // namespace dodoe
