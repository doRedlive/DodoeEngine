// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"

REFLECTION_TYPE(TileLayerComponent)

namespace dodoe {

    STRUCT(TileLayerComponent, WhiteListFields) {
        REFLECTION_BODY(TileLayerComponent)

        META(Enable)
        String layer_name;
        META(Enable)
        UInt32 layer_width{0};
        META(Enable)
        UInt32 layer_height{0};
        META(Enable)
        DynamicArray<UInt32> tiles;
        META(Enable)
        Bool visible{true};
        META(Enable)
        float opacity{1.0f};
        META(Enable)
        Int32 offset_x{0};
        META(Enable)
        Int32 offset_y{0};
    };

} // dodoe

