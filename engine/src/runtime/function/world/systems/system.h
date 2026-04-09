//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_SYSTEM_H
#define DODOE_SYSTEM_H

#include "dopch.h"

#include "runtime/function/world/entity.h"
#include "runtime/function/world/registry.h"
#include "runtime/function/world/world.h"

namespace dodoe {

    namespace system {

        class System {
        public:
            virtual ~System() = default;

            virtual void start(Registry& reg) { }
            virtual void update(Registry& reg) { }
            virtual void finalize(Registry& reg) { }

        };

    } // system

} // dodoe

#endif//DODOE_SYSTEM_H