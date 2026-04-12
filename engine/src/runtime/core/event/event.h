//
// Created by GreenMuffin on 2025/10/18.
//

#ifndef DODOE_EVENT_H
#define DODOE_EVENT_H

#include "dopch.h"

#include "runtime/function/window/window.h"
#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

namespace dodoe {

    struct ApplicationQuitEvent {};

    struct WindowCloseEvent {};

    struct WindowFocusEvent {};

    struct WindowLostFocusEvent {};

    struct WindowMovedEvent {};

    struct WindowResizeEvent {
        WindowResizeEvent(const unsigned int width, const unsigned int height)
            : width(width), height(height){ }
        unsigned int width;
        unsigned int height;
    };
    
    struct KeyPressedEvent {
        KeyPressedEvent(const KeyCode scancode, const bool repeat) : scancode(scancode), repeat(repeat) { }
        KeyCode scancode;
        bool repeat;
    };
    struct KeyReleasedEvent {
        explicit KeyReleasedEvent(const KeyCode scancode) : scancode(scancode) { }
        KeyCode scancode;
    };
    
    struct MouseButtonPressedEvent {
        explicit MouseButtonPressedEvent(const MouseCode button) : button(button) { }
        MouseCode button;
    };

    struct MouseButtonReleasedEvent {
        explicit MouseButtonReleasedEvent(const MouseCode button) : button(button) { }
        MouseCode button;
    };

    struct MouseMovedEvent {
        explicit MouseMovedEvent(const Vector2f position) : position(position) { }
        explicit MouseMovedEvent(const float x, const float y) : position(Vector2f(x, y)) { }
        Vector2f position;
    };

    struct MouseScrolledEvent {
        MouseScrolledEvent(const float x_offset, const float y_offset) : x_offset(x_offset), y_offset(y_offset) { }
        float x_offset, y_offset;
    };

    struct BeforeOneTickEvent {};
    struct AfterOneTickEvent {};

    // struct ViewportChangedEvent {
    //     Vector2f window_size;
    //     Vector2f pixel_size;

    //     ViewportChangedEvent(const Vector2f& window, const Vector2f& pixel) : window_size(window), pixel_size(pixel) { }
    // };

}; // dodoe

#endif //DODOE_EVENT_H