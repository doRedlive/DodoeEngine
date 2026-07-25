// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"

namespace dodoe {

    class LightingFeature final : public IRenderFeature {
    public:
        void collectPasses(PassCollector& collector) override;
    };

} // namespace dodoe
