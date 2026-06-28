// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_pass_type.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class IMeshPassProcessor {
    public:
        virtual ~IMeshPassProcessor() = default;
        virtual void reset() = 0;
        [[nodiscard]] virtual const GfxBindingLayoutHandle& getBindingLayout() const = 0;
        [[nodiscard]] virtual const GfxBufferHandle& getConstantBuffer() const = 0;
    };

} // dodoe
