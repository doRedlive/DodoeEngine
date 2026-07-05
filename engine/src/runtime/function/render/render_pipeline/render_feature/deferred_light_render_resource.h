// do@Redlive

#pragma once

#include "runtime/function/render/render_pipeline/render_pass_context.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class DeferredLightRenderResource {
        GfxBufferHandle m_constant_buffer{};

    public:
        void reset();
        [[nodiscard]] GfxBufferHandle getOrCreateConstantBuffer();
    };

} // dodoe
