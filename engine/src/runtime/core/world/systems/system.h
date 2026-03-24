//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_SYSTEM_H
#define DODOE_SYSTEM_H

#include "dopch.h"

#include "../entity.h"
#include "../registry.h"
#include "../world.h"
#include "../world_context.h"
#include "../world_manager.h"

namespace dodoe {

    namespace system {

        class System {
        public:
            virtual ~System() = default;

            virtual void start(Registry& reg) { }
            virtual void update(Registry& reg, float dt) { }
            virtual void finalize(Registry& reg) { }

        protected:
            WorldContext& context{WorldManager::self().active_world().context};
        };

    } // system

} // dodoe

#endif//DODOE_SYSTEM_H