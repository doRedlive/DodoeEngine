// do@Redlive

#pragma once

#include "runtime/dopch.h"

#include <variant>

#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

namespace dodoe {

    using InputActionId = UInt32;
    constexpr InputActionId kInvalidInputActionId = 0;

    using InputDeviceId = UInt32;
    constexpr InputDeviceId kInvalidInputDeviceId = 0;
    constexpr InputDeviceId kKeyboardDeviceId = 1;
    constexpr InputDeviceId kMouseDeviceId = 2;
    constexpr InputDeviceId kGamepadDeviceIdBase = 10;

    constexpr Size_t kMaxGamepads = 4;
    constexpr Size_t kGamepadButtonCount = 15;
    constexpr Size_t kGamepadAxisCount = 6;

    enum class InputActionValueType { Button, Axis1D, Axis2D };

    enum class InputActionPhase { Waiting, Started, Performed, Canceled };

    enum class InputBindingType { Key, MouseButton, MouseDelta, MouseWheel, GamepadButton, GamepadAxis, Composite };

    enum class InputInteraction { Press, Hold, Tap, MultiTap, Chord, Toggle, Repeat };

    enum class GamepadButtonCode : uint16_t {
        A = 0,
        B = 1,
        X = 2,
        Y = 3,
        LB = 4,
        RB = 5,
        Back = 6,
        Start = 7,
        Guide = 8,
        LeftStick = 9,
        RightStick = 10,
        DpadUp = 11,
        DpadRight = 12,
        DpadDown = 13,
        DpadLeft = 14,
    };

    enum class GamepadAxisCode : uint16_t {
        LeftX = 0,
        LeftY = 1,
        RightX = 2,
        RightY = 3,
        LeftTrigger = 4,
        RightTrigger = 5,
    };

    enum class InputProcessorType { DeadZone, Normalize, Scale, Invert, Clamp };

    struct InputProcessor {
        InputProcessorType type{InputProcessorType::DeadZone};
        Float a{0.0f};
        Float b{1.0f};
    };

    struct InputButtonState {
        Bool down{false};
        Bool pressed_this_frame{false};
        Bool released_this_frame{false};
    };

    struct GamepadState {
        Bool connected{false};
        std::array<InputButtonState, kGamepadButtonCount> buttons{};
        std::array<Float, kGamepadAxisCount> axes{};
        Float dead_zone{0.2f};
    };

    struct InputRawState {
        std::unordered_map<KeyCode, InputButtonState> keys{};
        std::array<InputButtonState, 8> mouse_buttons{};
        Vector2f mouse_position{0.0f, 0.0f};
        Vector2f mouse_delta{0.0f, 0.0f};
        Vector2f mouse_wheel{0.0f, 0.0f};
        std::array<GamepadState, kMaxGamepads> gamepads{};
    };

    struct InputBinding {
        InputBindingType type{InputBindingType::Key};
        KeyCode key{KeyCode::Space};
        MouseCode mouse{MouseCode::None};
        Float scale{1.0f};
        Vector2f vector_scale{1.0f, 0.0f};
        InputInteraction interaction{InputInteraction::Press};
        Float hold_seconds{0.5f};

        InputDeviceId device_id{kInvalidInputDeviceId};
        GamepadButtonCode gamepad_button{GamepadButtonCode::A};
        GamepadAxisCode gamepad_axis{GamepadAxisCode::LeftX};
        Float dead_zone{0.0f};
        DynamicArray<InputProcessor> processors{};
        DynamicArray<InputBinding> composite_parts{};
        Int tap_count{2};
        Float tap_window_seconds{0.4f};
        Float repeat_delay{0.4f};
        Float repeat_rate{0.1f};
    };

    using InputActionValue = std::variant<Bool, Float, Vector2f>;

    struct InputActionEvent {
        InputActionId action_id{kInvalidInputActionId};
        String map_name{};
        String action_name{};
        InputActionPhase phase{InputActionPhase::Waiting};
        InputActionValue value{false};
    };

    struct InputActionState {
        InputActionValueType value_type{InputActionValueType::Button};
        InputActionPhase phase{InputActionPhase::Waiting};
        InputActionValue value{false};
        Bool is_down{false};
        Bool pressed_this_frame{false};
        Bool released_this_frame{false};
        Float held_seconds{0.0f};
    };

    using InputSubscriptionId = UInt64;
    using InputActionListener = std::function<void(const InputActionEvent&)>;

    struct InputBindingOverride {
        String map_name{};
        String action_name{};
        Size_t binding_index{0};
        InputBinding binding{};
    };

} // namespace dodoe
