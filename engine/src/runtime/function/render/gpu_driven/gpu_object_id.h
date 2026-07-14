// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/container/slot_map.h"

namespace dodoe {

    enum class GpuObjectType : UInt32 {
        Invalid   = 0,
        Sprite    = 1,
        Primitive = 2,
        Light     = 3,
    };

    using GpuObjectHandle = SlotHandle<8>;

    using GpuResourceIndex = UInt32;
    static constexpr GpuResourceIndex kInvalidResourceIndex = ~0u;

} // dodoe
