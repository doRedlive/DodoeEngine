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

     struct WindowCloseEvent {
         explicit WindowCloseEvent(const uint32_t window_id) : window_id(window_id) { }
         uint32_t window_id;
     };
    
     struct WindowFocusEvent {
         explicit WindowFocusEvent(const uint32_t window_id) : window_id(window_id) { }
         uint32_t window_id;
     };
    
     struct WindowLostFocusEvent {
         explicit WindowLostFocusEvent(const uint32_t window_id) : window_id(window_id) { }
         uint32_t window_id;
     };

     struct WindowMovedEvent {
         explicit WindowMovedEvent(const uint32_t window_id) : window_id(window_id) { }
         uint32_t window_id;
     };

     struct WindowResizeEvent {
         WindowResizeEvent(const unsigned int width, const unsigned int height, const uint32_t window_id)
             : width(width), height(height), window_id(window_id){ }
         unsigned int width;
         unsigned int height;
         uint32_t window_id;
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

}; // dodoe

#endif //DODOE_EVENT_H