//
// Created by Redlive on 2026/3/18.
//

#include "render_api.h"

namespace dodoe {

    RenderApiType RenderApi::api_type_ = RenderApiType::None;

    void RenderApi::initialize(RenderApiInitInfo init_info) {
        DoAssert(init_info.api_type != RenderApiType::None, "The rendering api is not set correctly!");

        api_type_ = init_info.api_type;
    }

    RenderApiType RenderApi::api_type() {
        return api_type_;
    }

} // dodoe