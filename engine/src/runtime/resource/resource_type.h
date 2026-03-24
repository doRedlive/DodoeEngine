//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_RESOURCE_TYPE_H
#define DODOE_RESOURCE_TYPE_H

#include "dopch.h"

#include "runtime/function/animation/animation.h"

namespace dodoe {

    struct AnimClip2dRes {
        Ref<AnimClip2d> clip;
        std::string name;
        identifier id;
    };

} // dodoe

#endif//DODOE_RESOURCE_TYPE_H