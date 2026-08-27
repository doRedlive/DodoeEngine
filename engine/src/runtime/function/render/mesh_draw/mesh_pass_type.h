// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    enum class MeshPassType : UInt8 {
        Opaque = 0,
        Shadow,
        Transparent,
        Count
    };

} // dodoe
