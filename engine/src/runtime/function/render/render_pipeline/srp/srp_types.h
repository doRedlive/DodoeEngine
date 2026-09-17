// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    enum class SrpFormat : Int32 {
        RGBA8_UNORM = 0,
        RGBA16_FLOAT = 1,
        RGBA32_FLOAT = 2,
        RG16_FLOAT = 3,
        D32 = 4,
    };

    struct SrpMeshDrawSettings {
        const char* phase{nullptr};
        Int32 queue_min{0};
        Int32 queue_max{2500};
        Int32 layer_mask{~0};
        Int32 material_override{0};
        Int32 enable_instancing{0};
    };

    struct SrpRasterPassDesc {
        const char* name{nullptr};
        Int32 phase{0};
        Int32 execute_id{0};
        const Int32* color_handles{nullptr};
        const Int32* color_loads{nullptr};
        const Float* color_clears{nullptr};
        Int32 color_count{0};
        Int32 depth_handle{-1};
        Int32 depth_load{2};
        Float depth_clear{1.0f};
        const Int32* read_texture_handles{nullptr};
        Int32 read_texture_count{0};
    };

} // dodoe
