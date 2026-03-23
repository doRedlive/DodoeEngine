//
// Created by Redlive on 2026/3/21.
//

#ifndef DODOE_MATH_H
#define DODOE_MATH_H

#include "dopch.h"

#include "runtime/core/utils/util.h"

#include "glm/glm.hpp"
#include "glm/gtc/epsilon.hpp"
#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {

    class Math {
    public:

        [[nodiscard]] static Vector2f max(const Vector2f& left, const Vector2f& right) { return glm::max(left, right); } 
        [[nodiscard]] static Vector2f clamp(const Vector2f& value, const Vector2f& min, const Vector2f& max) { return glm::clamp(value, min, max); }
        [[nodiscard]] static Matrix4f ortho(float left, float right, float bottom, float top, float near, float far) { return glm::ortho(left, right, bottom, top, near, far); }
        [[nodiscard]] static Matrix4f scale(const Matrix4f& value, const Vector3f& scale) { return glm::scale(value, scale); }
        [[nodiscard]] static Matrix4f translate(const Matrix4f& value, const Vector3f& translation) { return glm::translate(value, translation); }
        [[nodiscard]] static Matrix4f rotate(const Matrix4f& value, float radians, const Vector3f& axis) { return glm::rotate(value, radians, axis); }

        template <typename T>
        [[nodiscard]] static constexpr T epsilon() { return glm::epsilon<T>(); }

        template <typename T>
        [[nodiscard]] static auto epsilonEqual(const T& x, const T& y, const T& epsilon) { return glm::epsilonEqual(x, y, epsilon); }
    };

} // dodoe

#endif//DODOE_MATH_H
