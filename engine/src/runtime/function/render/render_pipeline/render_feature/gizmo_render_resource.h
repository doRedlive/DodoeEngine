// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class GizmoRenderResource {
        GfxBindingLayoutHandle m_binding_layout{};

    public:
        void reset();

        [[nodiscard]] GfxBindingLayoutHandle getOrCreateBindingLayout(DrawCommandList& command_list);
    };

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
