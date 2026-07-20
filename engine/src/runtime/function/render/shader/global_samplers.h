// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class GlobalSamplers {
        inline static GfxSamplerHandle s_point{};
        inline static GfxSamplerHandle s_bilinear{};
        inline static GfxSamplerHandle s_screen{};

    public:
        GlobalSamplers() = delete;

        static void initialize(GfxDevice* device);
        static void reset();

        static GfxSamplerHandle point() { return s_point; }
        static GfxSamplerHandle bilinear() { return s_bilinear; }
        static GfxSamplerHandle screen() { return s_screen; }
    };

} // dodoe
