//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_SYSTEM_H
#define DODOE_SYSTEM_H

#include "dopch.h"

#include "runtime/core/world/entity.h"
#include "runtime/core/world/registry.h"
#include "runtime/core/world/world.h"
#include "runtime/core/world/world_manager.h"

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