// do@Redlive

#include "input_backend_glfw.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "GLFW/glfw3.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#endif

namespace dodoe {

    InputBackendGlfw* InputBackendGlfw::s_active = nullptr;

    Bool InputBackendGlfw::initialize(InputRawState& raw_state, void* native_window) {
        if (!native_window) return false;
        m_window = native_window;
        m_raw_state = &raw_state;
        GLFWwindow* window = static_cast<GLFWwindow*>(native_window);
        glfwSetWindowUserPointer(window, this);
        s_active = this;

        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_KeyCallback(w, key, scancode, action, mods);
#endif
            auto* self = static_cast<InputBackendGlfw*>(glfwGetWindowUserPointer(w));
            if (self && self->m_raw_state) {
                InputRawState& raw = *self->m_raw_state;
                if (action == GLFW_PRESS) {
                    InputButtonState& state = raw.keys[static_cast<KeyCode>(key)];
                    if (!state.down) state.pressed_this_frame = true;
                    state.down = true;
                } else if (action == GLFW_RELEASE) {
                    const auto it = raw.keys.find(static_cast<KeyCode>(key));
                    if (it != raw.keys.end()) {
                        if (it->second.down) it->second.released_this_frame = true;
                        it->second.down = false;
                    }
                }
            }
            switch (action) {
            case GLFW_PRESS: { KeyPressedEvent e(static_cast<KeyCode>(key), false); EventSystem::Enqueue<KeyPressedEvent>(e); break; }
            case GLFW_RELEASE: { KeyReleasedEvent e(static_cast<KeyCode>(key)); EventSystem::Enqueue<KeyReleasedEvent>(e); break; }
            case GLFW_REPEAT: { KeyPressedEvent e(static_cast<KeyCode>(key), true); EventSystem::Enqueue<KeyPressedEvent>(e); break; }
            }
        });

        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int keycode) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_CharCallback(w, keycode);
#endif
        });

        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);
#endif
            auto* self = static_cast<InputBackendGlfw*>(glfwGetWindowUserPointer(w));
            if (self && self->m_raw_state) {
                InputRawState& raw = *self->m_raw_state;
                const Size_t index = static_cast<Size_t>(button);
                if (index < raw.mouse_buttons.size()) {
                    InputButtonState& state = raw.mouse_buttons[index];
                    if (action == GLFW_PRESS) {
                        if (!state.down) state.pressed_this_frame = true;
                        state.down = true;
                    } else {
                        if (state.down) state.released_this_frame = true;
                        state.down = false;
                    }
                }
            }
            switch (action) {
            case GLFW_PRESS: { MouseButtonPressedEvent e(static_cast<MouseCode>(button)); EventSystem::Enqueue<MouseButtonPressedEvent>(e); break; }
            case GLFW_RELEASE: { MouseButtonReleasedEvent e(static_cast<MouseCode>(button)); EventSystem::Enqueue<MouseButtonReleasedEvent>(e); break; }
            }
        });

        glfwSetScrollCallback(window, [](GLFWwindow* w, double x_offset, double y_offset) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_ScrollCallback(w, x_offset, y_offset);
#endif
            auto* self = static_cast<InputBackendGlfw*>(glfwGetWindowUserPointer(w));
            if (self && self->m_raw_state) {
                self->m_raw_state->mouse_wheel += Vector2f(static_cast<Float>(x_offset), static_cast<Float>(y_offset));
            }
            MouseScrolledEvent e(static_cast<Float>(x_offset), static_cast<Float>(y_offset));
            EventSystem::Enqueue<MouseScrolledEvent>(e);
        });

        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x_pos, double y_pos) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_CursorPosCallback(w, x_pos, y_pos);
#endif
            auto* self = static_cast<InputBackendGlfw*>(glfwGetWindowUserPointer(w));
            if (self && self->m_raw_state) {
                InputRawState& raw = *self->m_raw_state;
                const Vector2f position(static_cast<Float>(x_pos), static_cast<Float>(y_pos));
                raw.mouse_delta += position - raw.mouse_position;
                raw.mouse_position = position;
            }
            MouseMovedEvent e(static_cast<Float>(x_pos), static_cast<Float>(y_pos));
            EventSystem::Enqueue<MouseMovedEvent>(e);
        });

        glfwSetJoystickCallback([](int joystick_id, int event) {
            if (s_active && event == GLFW_DISCONNECTED) s_active->onJoystickDisconnected(joystick_id);
        });

        return true;
    }

    void InputBackendGlfw::shutdown() {
        glfwSetJoystickCallback(nullptr);
        if (s_active == this) s_active = nullptr;
        if (m_window) {
            GLFWwindow* window = static_cast<GLFWwindow*>(m_window);
            glfwSetKeyCallback(window, nullptr);
            glfwSetCharCallback(window, nullptr);
            glfwSetMouseButtonCallback(window, nullptr);
            glfwSetScrollCallback(window, nullptr);
            glfwSetCursorPosCallback(window, nullptr);
        }
        m_raw_state = nullptr;
        m_window = nullptr;
    }

    void InputBackendGlfw::poll(InputRawState& raw_state) {
        for (Size_t slot = 0; slot < kMaxGamepads; ++slot) {
            const int joystick_id = static_cast<int>(slot);
            GamepadState& pad = raw_state.gamepads[slot];
            if (!glfwJoystickPresent(joystick_id)) {
                if (pad.connected) {
                    for (auto& button : pad.buttons) {
                        if (button.down) button.released_this_frame = true;
                        button.down = false;
                        button.pressed_this_frame = false;
                    }
                }
                pad.connected = false;
                continue;
            }
            pad.connected = true;

            GLFWgamepadstate gamepad;
            if (glfwGetGamepadState(joystick_id, &gamepad)) {
                for (Size_t i = 0; i < kGamepadButtonCount; ++i) {
                    const Bool down = gamepad.buttons[i] == GLFW_PRESS;
                    InputButtonState& state = pad.buttons[i];
                    if (down && !state.down) state.pressed_this_frame = true;
                    else if (!down && state.down) state.released_this_frame = true;
                    state.down = down;
                }
                for (Size_t i = 0; i < kGamepadAxisCount; ++i) {
                    pad.axes[i] = gamepad.axes[i];
                }
            } else {
                int button_count = 0;
                const unsigned char* buttons = glfwGetJoystickButtons(joystick_id, &button_count);
                const Size_t available_buttons = static_cast<Size_t>(button_count);
                for (Size_t i = 0; i < kGamepadButtonCount; ++i) {
                    const Bool down = i < available_buttons && buttons[i] == GLFW_PRESS;
                    InputButtonState& state = pad.buttons[i];
                    if (down && !state.down) state.pressed_this_frame = true;
                    else if (!down && state.down) state.released_this_frame = true;
                    state.down = down;
                }
                int axis_count = 0;
                const float* axes = glfwGetJoystickAxes(joystick_id, &axis_count);
                const Size_t available_axes = static_cast<Size_t>(axis_count);
                for (Size_t i = 0; i < kGamepadAxisCount; ++i) {
                    pad.axes[i] = i < available_axes ? axes[i] : 0.0f;
                }
            }
        }
    }

    void InputBackendGlfw::onJoystickDisconnected(Int joystick_id) {
        if (joystick_id < 0 || static_cast<Size_t>(joystick_id) >= kMaxGamepads) return;
        if (!m_raw_state) return;
        GamepadState& pad = m_raw_state->gamepads[static_cast<Size_t>(joystick_id)];
        for (auto& button : pad.buttons) {
            if (button.down) button.released_this_frame = true;
            button.down = false;
            button.pressed_this_frame = false;
        }
        pad.connected = false;
    }

} // namespace dodoe
