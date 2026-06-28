// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/epsilon.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/packing.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/euler_angles.hpp"

namespace dodoe {

    using Quaternion = glm::quat;

    class Math {
    public:
        static constexpr float PI = 3.1415926535f;

        static float Rad2Deg(float rad) {
            return rad * 180.0f / PI;
        }

        // --- Arithmetic / Comparison ---
        template <typename... Args>
        [[nodiscard]] static auto Max(Args&&... args) { return (glm::max)(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Min(Args&&... args) { return (glm::min)(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Clamp(Args&&... args) { return glm::clamp(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Abs(Args&&... args) { return glm::abs(std::forward<Args>(args)...); }

        // --- Trigonometry ---
        template <typename... Args>
        [[nodiscard]] static auto Sin(Args&&... args) { return glm::sin(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Cos(Args&&... args) { return glm::cos(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Radians(Args&&... args) { return glm::radians(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Degrees(Args&&... args) { return glm::degrees(std::forward<Args>(args)...); }

        // --- Vector math ---
        template <typename... Args>
        [[nodiscard]] static auto Dot(Args&&... args) { return glm::dot(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Cross(Args&&... args) { return glm::cross(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Length(Args&&... args) { return glm::length(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Normalize(Args&&... args) { return glm::normalize(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Distance(Args&&... args) { return glm::distance(std::forward<Args>(args)...); }

        // --- Matrix ---
        template <typename... Args>
        [[nodiscard]] static auto Inverse(Args&&... args) { return glm::inverse(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Transpose(Args&&... args) { return glm::transpose(std::forward<Args>(args)...); }

        // --- Projection ---
        template <typename... Args>
        [[nodiscard]] static auto Ortho(Args&&... args) { return glm::ortho(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto OrthoRH_ZO(Args&&... args) { return glm::orthoRH_ZO(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Perspective(Args&&... args) { return glm::perspective(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto LookAt(Args&&... args) { return glm::lookAt(std::forward<Args>(args)...); }

        // --- Transform ---
        template <typename... Args>
        [[nodiscard]] static auto Translate(Args&&... args) { return glm::translate(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Scale(Args&&... args) { return glm::scale(std::forward<Args>(args)...); }

        template <typename... Args>
        [[nodiscard]] static auto Rotate(Args&&... args) { return glm::rotate(std::forward<Args>(args)...); }

        // --- Quaternion ---
        template <typename... Args>
        [[nodiscard]] static auto EulerAngles(Args&&... args) { return glm::eulerAngles(std::forward<Args>(args)...); }

        // --- Packing ---
        template <typename... Args>
        [[nodiscard]] static auto PackSnorm4x8(Args&&... args) { return glm::packSnorm4x8(std::forward<Args>(args)...); }

        // --- Epsilon ---
        template <typename T>
        [[nodiscard]] static constexpr T Epsilon() { return glm::epsilon<T>(); }

        template <typename T>
        [[nodiscard]] static auto EpsilonEqual(const T& x, const T& y, const T& epsilon) { return glm::epsilonEqual(x, y, epsilon); }
    };

} // dodoe
