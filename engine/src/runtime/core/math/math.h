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
        static constexpr float PI = 3.1415926f;

        static float rad2deg(float rad) {
            return rad * 180.0f / PI;
        }

        template <typename... Args>
        [[nodiscard]] static auto max(Args&&... args) { return glm::max(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto min(Args&&... args) { return glm::min(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto clamp(Args&&... args) { return glm::clamp(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto ortho(Args&&... args) { return glm::ortho(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto perspective(Args&&... args) { return glm::perspective(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto lookAt(Args&&... args) { return glm::lookAt(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto translate(Args&&... args) { return glm::translate(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto scale(Args&&... args) { return glm::scale(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto rotate(Args&&... args) { return glm::rotate(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto radians(Args&&... args) { return glm::radians(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto degrees(Args&&... args) { return glm::degrees(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto dot(Args&&... args) { return glm::dot(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto cross(Args&&... args) { return glm::cross(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto length(Args&&... args) { return glm::length(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto normalize(Args&&... args) { return glm::normalize(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto distance(Args&&... args) { return glm::distance(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto inverse(Args&&... args) { return glm::inverse(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto transpose(Args&&... args) { return glm::transpose(std::forward<Args>(args)...); }

        template <typename T>
        [[nodiscard]] static constexpr T epsilon() { return glm::epsilon<T>(); }

        template <typename T>
        [[nodiscard]] static auto epsilonEqual(const T& x, const T& y, const T& epsilon) { return glm::epsilonEqual(x, y, epsilon); }
    };

} // dodoe

#endif//DODOE_MATH_H
