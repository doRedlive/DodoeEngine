// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/layer/layer.h"

#include "baseline_renderer.h"

namespace dodoe {

    class DODOE_API RenderReferenceLayer final : public Layer {
        Scope<BaselineRenderer> m_baseline{nullptr};

    public:
        RenderReferenceLayer(const String& name);
        ~RenderReferenceLayer() override = default;

        void attach() override;
        void detach() override;
        void updateTick(float delta_time) override {}
        void renderTick() override {}
    };

} // namespace dodoe
