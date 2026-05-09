//
// Created by GreenMuffin on 2025/12/10.
//

#ifndef DODOE_INPUT_H
#define DODOE_INPUT_H

#include "key_code.h"
#include "mouse_code.h"

namespace dodoe {

    class InputManager;

    class Input {
    public:

        static void initialize(InputManager* input_manager);
        static void shutdown();

        static bool IsKeyPressed(KeyCode key_code);
        static bool IsMouseButtonPressed(MouseCode mouse_code);
        static Vector2f GetMousePosition();
        static Vector2f GetMouseWindowPosition();
        static float get_mouse_x();
        static float get_mouse_y();

        static InputManager* input_manager_;
    };
} // dodoe


#endif //DODOE_INPUT_H
