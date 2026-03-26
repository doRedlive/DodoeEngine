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

        Vector4f to_vec4() const {
            return {r, g, b, a};
        }

        static Color white() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
        static Color black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
        static Color red()   { return {1.0f, 0.0f, 0.0f, 1.0f}; }
        static Color green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
        static Color blue()  { return {0.0f, 0.0f, 1.0f, 1.0f}; }
        static Color gray()  { return {0.5f, 0.5f, 0.5f, 1.0f}; }
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