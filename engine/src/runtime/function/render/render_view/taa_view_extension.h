// do@Redlive

#pragma once

#include "dopch.h"

#include "view_extension.h"

namespace dodoe {

    class TaaViewExtension final : public IViewExtension {
    public:
        Vector2f jitter_ndc{0.0f, 0.0f};
        Matrix4f unjittered_view_projection{1.0f};

        void reset() override {
            jitter_ndc = Vector2f(0.0f, 0.0f);
            unjittered_view_projection = Matrix4f(1.0f);
        }
    };

} // dodoe
