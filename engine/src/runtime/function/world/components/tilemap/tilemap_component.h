// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/pixel2d/tileset.h"

REFLECTION_TYPE(TilemapComponent)

namespace dodoe {

    STRUCT(TilemapComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(TilemapComponent)

        META(Enable)
        UInt32 map_width{0};
        META(Enable)
        UInt32 map_height{0};
        META(Enable)
        UInt32 tile_width{16};
        META(Enable)
        UInt32 tile_height{16};

        META(Enable)
        DynamicArray<PPtr<Tileset>> tilesets;
        Bool dirty{true};

        [[nodiscard]] const Tileset* findTilesetByGid(UInt32 gid) const {
            for (auto it = tilesets.rbegin(); it != tilesets.rend(); ++it) {
                if (const Tileset* tileset = it->get(); tileset && gid >= tileset->first_gid) {
                    return tileset;
                }
            }
            return nullptr;
        }
    };

} // dodoe

