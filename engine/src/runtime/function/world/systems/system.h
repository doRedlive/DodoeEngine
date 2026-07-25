//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_SYSTEM_H
#define DODOE_SYSTEM_H

#include "dopch.h"

#include <entt/entt.hpp>

#include "runtime/function/world/registry.h"

namespace dodoe {

    struct SystemAccess {
        DynamicArray<entt::id_type> reads{};
        DynamicArray<entt::id_type> writes{};
        bool structural{false};
    };

    struct SystemAccessBuilder {
        SystemAccess access{};

        template<typename... Ts>
        SystemAccessBuilder& readsComponents() {
            (access.reads.push_back(entt::type_hash<Ts>::value()), ...);
            return *this;
        }

        template<typename... Ts>
        SystemAccessBuilder& writesComponents() {
            (access.writes.push_back(entt::type_hash<Ts>::value()), ...);
            return *this;
        }

        SystemAccessBuilder& hasStructuralChanges(bool v = true) {
            access.structural = v;
            return *this;
        }

        SystemAccess build() { return std::move(access); }
    };

    class System {
    public:
        virtual ~System();

        virtual void start(Registry& reg);
        virtual void update(Registry& reg, float dt);
        virtual void finalize(Registry& reg);

        [[nodiscard]] virtual SystemAccess getAccess() const { return {}; }
    };

} // dodoe

#endif//DODOE_SYSTEM_H
