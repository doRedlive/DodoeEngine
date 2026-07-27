// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"

namespace dodoe {

    struct alignas(16) UIInstance {
        Vector2f position;
        Vector2f size;
        Vector2f uv_min;
        Vector2f uv_max;
        UInt32 color;
        UInt32 atlas_index;
        Float depth;
        UInt32 flags;
        Rect clip_rect;
    };

    static_assert(sizeof(UIInstance) == 64, "UIInstance must be 64 bytes");
    static_assert(alignof(UIInstance) == 16, "UIInstance must be 16-byte aligned");

    struct Thickness {
        Float left{0};
        Float top{0};
        Float right{0};
        Float bottom{0};

        Thickness() = default;
        Thickness(Float uniform) : left(uniform), top(uniform), right(uniform), bottom(uniform) {}
        Thickness(Float l, Float t, Float r, Float b) : left(l), top(t), right(r), bottom(b) {}
    };

    struct NineSliceMargins {
        Float left{0};
        Float top{0};
        Float right{0};
        Float bottom{0};
    };

    enum class FillMethod : UInt8 {
        None,
        Horizontal,
        Vertical,
        Radial90,
        Radial180,
        Radial360,
    };

    enum class TextAnchor : UInt8 {
        UpperLeft,
        UpperCenter,
        UpperRight,
        MiddleLeft,
        MiddleCenter,
        MiddleRight,
        LowerLeft,
        LowerCenter,
        LowerRight,
    };

    enum class LayoutDirection : UInt8 {
        Horizontal,
        Vertical,
    };

    enum class Alignment : UInt8 {
        Start,
        Center,
        End,
        Stretch,
    };

    enum class ButtonState : UInt8 {
        Normal,
        Hovered,
        Pressed,
        Disabled,
    };

} // namespace dodoe
