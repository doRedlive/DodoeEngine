// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"

namespace dodoe {

    struct TilesetAsset {
        String name;
        UInt32 first_gid{1};
        UInt32 tile_width{16};
        UInt32 tile_height{16};
        UInt32 columns{0};
        UInt32 tile_count{0};
        String image_path;
        Identifier texture_id{0};
    };

} // dodoe

