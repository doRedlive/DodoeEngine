//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_COLLIDER2D_COMPONENT_H
#define DODOE_COLLIDER2D_COMPONENT_H

#include "dopch.h"

namespace dodoe {

    struct BoxCollider2dComponent {
        Vector2f offset{ 0.0f,0.0f };
        Vector2f size{ 10.0f, 10.0f };

        float density{ 1.0f };
        float friction{ 0.5f };
        float restitution{ 0.0f };
        float restitution_threshold{ 0.5f };

        BoxCollider2dComponent() = default;
    };

} // dodoe

#endif//DODOE_COLLIDER2D_COMPONENT_H