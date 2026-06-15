//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_PHYSICS2D_SYSTEM_H
#define DODOE_PHYSICS2D_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/physics/physics_system.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

namespace dodoe {

    class Physics2dSystem : public System {
    public:
        ~Physics2dSystem() override;

        void start(Registry& reg) override;
        void update(Registry& reg, float dt) override;
        void finalize(Registry& reg) override;

        b2BodyId get_body_id(Registry& reg, const Entity& entity) const;

    private:
        struct RegistryState {
            std::unordered_map<ui32, b2BodyId> body_umap{};
            std::unordered_map<ui32, b2ShapeId> shape_umap{};
        };

        RegistryState& state_(Registry& reg);

        static b2BodyType rigidbody2d_type2box2d_type_(Rigidbody2dComponent::BodyType type);

        std::unordered_map<Registry*, RegistryState> registry_state_umap_{};
    };

} // dodoe

#endif//DODOE_PHYSICS2D_SYSTEM_H
