//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_SYSTEM_H
#define DODOE_SYSTEM_H

#include "dopch.h"

#include "runtime/function/world/registry.h"

namespace dodoe {

    class System {
    public:
        virtual ~System() = default;

        virtual void start(Registry& reg) { (void)reg; }
        virtual void update(Registry& reg, float dt) { (void)reg; (void)dt; }
        virtual void finalize(Registry& reg) { (void)reg; }
    };

} // dodoe

#endif//DODOE_SYSTEM_H
