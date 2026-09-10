// do@Redlive

#pragma once

#include "dopch.h"

#include "view_extension.h"
#include "runtime/function/render/render_pipeline/shadow/shadow_view_data.h"

namespace dodoe {

    class ShadowViewExtension final : public IViewExtension {
    private:
        ShadowViewData data{};

    public:
        void reset() override { data = ShadowViewData{}; }

        [[nodiscard]] ShadowViewData& getData() { return data; }
        [[nodiscard]] const ShadowViewData& getData() const { return data; }
    };

} // namespace dodoe
