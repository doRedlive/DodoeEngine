//
// Created by GreenMuffin on 2025/11/27.
//

#ifndef DODOE_UTIL_H
#define DODOE_UTIL_H

#include "dopch.h"


namespace dodoe {
    struct Color {
        float r{1.0f};
        float g{1.0f};
        float b{1.0f};
        float a{1.0f};

        constexpr Color() = default;
        constexpr Color(float in_r, float in_g, float in_b, float in_a = 1.0f)
            : r(in_r), g(in_g), b(in_b), a(in_a) {}
        explicit constexpr Color(uint32_t rgba32)
            : r(static_cast<float>((rgba32 >> 24) & 0xFF) / 255.0f),
              g(static_cast<float>((rgba32 >> 16) & 0xFF) / 255.0f),
              b(static_cast<float>((rgba32 >> 8) & 0xFF) / 255.0f),
              a(static_cast<float>(rgba32 & 0xFF) / 255.0f) {}
        constexpr Color(uint32_t rgb24, float in_a)
            : r(static_cast<float>((rgb24 >> 16) & 0xFF) / 255.0f),
              g(static_cast<float>((rgb24 >> 8) & 0xFF) / 255.0f),
              b(static_cast<float>(rgb24 & 0xFF) / 255.0f),
              a(in_a) {}

        [[nodiscard]] uint32_t to_rgba32() const {
            return to_rgba32(r, g, b, a);
        }

        static uint32_t to_rgba32(float in_r, float in_g, float in_b, float in_a = 1.0f) {
            const uint32_t ur = channel_from_float(in_r);
            const uint32_t ug = channel_from_float(in_g);
            const uint32_t ub = channel_from_float(in_b);
            const uint32_t ua = channel_from_float(in_a);
            return (ur << 24) | (ug << 16) | (ub << 8) | ua;
        }

        Vector4f to_vec4() const {
            return {r, g, b, a};
        }

        static Color white() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
        static Color black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
        static Color red()   { return {1.0f, 0.0f, 0.0f, 1.0f}; }
        static Color green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
        static Color blue()  { return {0.0f, 0.0f, 1.0f, 1.0f}; }
        static Color gray()  { return {0.5f, 0.5f, 0.5f, 1.0f}; }

    private:
        static uint32_t channel_from_float(float value) {
            const float clamped = std::max(0.0f, std::min(1.0f, value));
            return static_cast<uint32_t>(clamped * 255.0f + 0.5f);
        }
    };

    struct Rect {
        Vector2f pos{}; // left bottom is zero point
        Vector2f size{};

        Rect() = default;
        Rect(Vector2f in_pos, Vector2f in_size) : pos(in_pos), size(in_size) {}
        Rect(float x, float y, float width, float height) : pos(x, y), size(width, height) {}

        [[nodiscard]] bool contains(const Vector2f& point) const {
            return point.x >= pos.x && point.x <= pos.x + size.x &&
                point.y >= pos.y && point.y <= pos.y + size.y;
        }
    };
 
} // dodoe

#endif //DODOE_UTIL_H
