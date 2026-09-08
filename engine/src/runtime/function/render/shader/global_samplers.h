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

        static GfxSamplerHandle Point() { return s_point; }
        static GfxSamplerHandle Bilinear() { return s_bilinear; }
        static GfxSamplerHandle Screen() { return s_screen; }
    };

} // dodoe
