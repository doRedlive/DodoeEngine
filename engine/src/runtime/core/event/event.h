// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"
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
        KeyPressedEvent(const KeyCode key, const bool repeat) : key(key), repeat(repeat) { }
        KeyCode key;
        bool repeat;
    };
    struct KeyReleasedEvent {
        explicit KeyReleasedEvent(const KeyCode key) : key(key) { }
        KeyCode key;
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

    struct AssetReimportedEvent {
        AssetReimportedEvent(const UUID in_asset_id, const String& in_source_path)
            : asset_id(in_asset_id), source_path(in_source_path) { }
        UUID asset_id{0};
        String source_path{};
    };

}; // dodoe
