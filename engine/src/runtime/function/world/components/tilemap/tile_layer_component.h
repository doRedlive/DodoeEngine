// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"

REFLECTION_TYPE(TileLayerComponent)

namespace dodoe {

    STRUCT(TileLayerComponent, WhiteListFields, ScriptBind) {
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

        [[nodiscard]] UInt32 getTile(Int32 x, Int32 y) const {
            if (x < 0 || y < 0 || x >= static_cast<Int32>(layer_width) || y >= static_cast<Int32>(layer_height)) return 0;
            return tiles[static_cast<Size_t>(y) * layer_width + x];
        }

        bool setTile(Int32 x, Int32 y, UInt32 gid) {
            if (x < 0 || y < 0 || x >= static_cast<Int32>(layer_width) || y >= static_cast<Int32>(layer_height)) return false;
            Size_t i = static_cast<Size_t>(y) * layer_width + x;
            if (tiles[i] == gid) return false;
            tiles[i] = gid;
            return true;
        }

        void resize(UInt32 w, UInt32 h) {
            DynamicArray<UInt32> next(w * h, 0);
            UInt32 cw = std::min(w, layer_width), ch = std::min(h, layer_height);
            for (UInt32 yy = 0; yy < ch; ++yy)
                for (UInt32 xx = 0; xx < cw; ++xx)
                    next[yy * w + xx] = tiles[yy * layer_width + xx];
            tiles = std::move(next); layer_width = w; layer_height = h;
        }
    };

} // dodoe

