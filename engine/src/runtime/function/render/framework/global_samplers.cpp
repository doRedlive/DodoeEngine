// do@Redlive

#include "global_samplers.h"

namespace dodoe {

    void GlobalSamplers::initialize(GfxDevice* device) {
        DO_ASSERT(device != nullptr, "GlobalSamplers device is null");

        s_point = device->createSampler(
            GfxSamplerDesc()
                .setAllFilters(true)
                .setAllAddressModes(GfxSamplerAddressMode::Clamp)
        );

        s_bilinear = device->createSampler(
            GfxSamplerDesc()
                .setAllFilters(true)
                .setAllAddressModes(GfxSamplerAddressMode::Clamp)
        );

        s_screen = device->createSampler(GfxSamplerDesc());
    }

    void GlobalSamplers::reset() {
        s_point = nullptr;
        s_bilinear = nullptr;
        s_screen = nullptr;
    }

} // dodoe
