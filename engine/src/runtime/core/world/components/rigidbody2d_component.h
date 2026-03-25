//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_RIGIDBODY2D_COMPONENT_H
#define DODOE_RIGIDBODY2D_COMPONENT_H

#include "dopch.h"

namespace dodoe {

    namespace component {

        struct Rigidbody2dComponent {
            enum class BodyType { Static = 0, Dynamic, Kinematic };
            BodyType type{ BodyType::Static };
            bool fixed_rotation{ false };

            Rigidbody2dComponent() = default;
        };

    } // component

} // dodoe

#endif//DODOE_RIGIDBODY2D_COMPONENT_H