// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    enum class GpuObjectDirtyFlags : UInt32 {
        None             = 0,
        ObjectMeta       = 1 << 0,
        Transform        = 1 << 1,
        Bounds           = 1 << 2,
        MaterialIndex    = 1 << 3,
        TextureIndex     = 1 << 4,
        InstanceData     = 1 << 5,
        All              = (1 << 6) - 1
    };

    inline GpuObjectDirtyFlags operator|(const GpuObjectDirtyFlags lhs, const GpuObjectDirtyFlags rhs) {
        return static_cast<GpuObjectDirtyFlags>(static_cast<UInt32>(lhs) | static_cast<UInt32>(rhs));
    }

    inline GpuObjectDirtyFlags& operator|=(GpuObjectDirtyFlags& lhs, const GpuObjectDirtyFlags rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    inline Bool HasAnyFlags(const GpuObjectDirtyFlags lhs, const GpuObjectDirtyFlags rhs) {
        return (static_cast<UInt32>(lhs) & static_cast<UInt32>(rhs)) != 0;
    }

} // dodoe
