//
// Created by Redlive on 2026/3/21.
//

#ifndef DODOE_MATH_H
#define DODOE_MATH_H

#include "dopch.h"

#include "runtime/core/utils/util.h"

#include "glm/glm.hpp"

namespace dodoe {

    class Math {
    public:

        [[nodiscard]] static const Vector2f& max(const Vector2f& left, const Vector2f& right) { return glm::max(left, right); } 
        [[nodiscard]] static const Vector2f& clamp(const Vector2f& value, const Vector2f& min, const Vector2f& max) { return glm::clamp(value, min, max); }

    };

} // dodoe

#endif//DODOE_MATH_H