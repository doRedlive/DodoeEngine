//
// Created by Redlive 2026/3/18.
//

#ifndef DODOE_RENDER_API_H
#define DODOE_RENDER_API_H

#include "dopch.h"

namespace dodoe {

    enum class RenderApiType {
        None,
        OpenGL,
        Vulkan,
        DX12,
    };

    struct RenderApiInitInfo {
        RenderApiType api_type;
    };

    class RenderApi {
    public:
        static void initialize(RenderApiInitInfo init_info);

        static RenderApiType api_type();

    private:
        static RenderApiType api_type_;
    };

} // dodoe

#endif//DOODE_RENDER_API_H