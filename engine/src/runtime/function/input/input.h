// do@Redlive

#pragma once

#include "input_types.h"

namespace dodoe {

    class InputManager;

    class DODOE_API Input {
    public:
        static void initialize(InputManager* input_manager);
        static void shutdown();

        static Bool RegisterActionMap(const String& map_name, Int priority = 0);
        static Bool UnregisterActionMap(StringView map_name);
        static Bool SetActionMapEnabled(StringView map_name, Bool enabled);
        static Bool SetActionMapConsume(StringView map_name, Bool consume);
        static Bool PushInputContext(StringView map_name);
        static Bool PopInputContext(StringView map_name);
        static Bool RegisterAction(StringView map_name, const String& action_name,
                                   InputActionValueType value_type);
        static Bool BindKey(StringView map_name, StringView action_name, KeyCode key, Float scale = 1.0f);
        static Bool BindKey2D(StringView map_name, StringView action_name, KeyCode key, Vector2f scale);
        static Bool BindMouseButton(StringView map_name, StringView action_name,
                                    MouseCode button, Float scale = 1.0f);
        static Bool BindMouseDelta(StringView map_name, StringView action_name, Float scale = 1.0f);
        static Bool BindMouseWheel(StringView map_name, StringView action_name, Float scale = 1.0f);
        static Bool SetBindingInteraction(StringView map_name, StringView action_name,
                                          InputInteraction interaction, Float hold_seconds = 0.5f);
        static Bool SetBindingTapParams(StringView map_name, StringView action_name, Size_t binding_index,
                                        Int tap_count, Float tap_window_seconds);
        static Bool SetBindingRepeatParams(StringView map_name, StringView action_name, Size_t binding_index,
                                           Float repeat_delay, Float repeat_rate);
        static Bool SetBindingProcessor(StringView map_name, StringView action_name, Size_t binding_index,
                                        const InputProcessor& processor);
        static Bool LoadActionAsset(const String& absolute_path);

        static Bool BindGamepadButton(StringView map_name, StringView action_name, GamepadButtonCode button,
                                      InputDeviceId device_id = kInvalidInputDeviceId, Float scale = 1.0f);
        static Bool BindGamepadAxis(StringView map_name, StringView action_name, GamepadAxisCode axis,
                                    InputDeviceId device_id = kInvalidInputDeviceId, Float scale = 1.0f);
        static Bool BindGamepadStick(StringView map_name, StringView action_name, GamepadAxisCode stick_axis,
                                     InputDeviceId device_id = kInvalidInputDeviceId, Float scale = 1.0f);
        static Bool BindComposite(StringView map_name, StringView action_name,
                                  const DynamicArray<InputBinding>& parts,
                                  InputDeviceId device_id = kInvalidInputDeviceId);

        static Bool SetBindingOverride(StringView map_name, StringView action_name, Size_t binding_index,
                                       const InputBinding& binding);
        static Bool ClearBindingOverride(StringView map_name, StringView action_name, Size_t binding_index);
        static Bool GetBinding(StringView map_name, StringView action_name, Size_t binding_index,
                               InputBinding& out);
        static Bool BeginRebindSession(StringView map_name, StringView action_name, Size_t binding_index);
        static void CancelRebindSession();
        static Bool IsRebindSessionActive();
        static Bool LoadConfigOverrides(const FsPath& project_path, const FsPath& user_path);
        static Bool SaveUserConfigOverrides(const FsPath& user_path);

        static InputActionId FindActionId(StringView map_name, StringView action_name);
        static InputActionId FindActionId(StringView qualified_name);

        static Bool IsActionDown(StringView action_name);
        static Bool WasActionPressed(StringView action_name);
        static Bool WasActionReleased(StringView action_name);
        static Float GetActionAxis(StringView action_name);
        static Vector2f GetActionVector2(StringView action_name);
        static Vector2f GetMousePosition();
        static Vector2f GetMouseDelta();
        static Vector2f GetMouseWheel();

        static Bool IsGamepadConnected(InputDeviceId device_id);
        static Bool IsGamepadButtonDown(InputDeviceId device_id, GamepadButtonCode button);
        static Bool IsGamepadButtonPressed(InputDeviceId device_id, GamepadButtonCode button);
        static Bool IsGamepadButtonReleased(InputDeviceId device_id, GamepadButtonCode button);
        static Float GetGamepadAxis(InputDeviceId device_id, GamepadAxisCode axis);

        static Bool IsActionDown(InputActionId action_id);
        static Bool WasActionPressed(InputActionId action_id);
        static Bool WasActionReleased(InputActionId action_id);
        static Float GetActionAxis(InputActionId action_id);
        static Vector2f GetActionVector2(InputActionId action_id);

        static InputSubscriptionId Subscribe(StringView action_name, InputActionPhase phase,
                                             InputActionListener listener);
        static InputSubscriptionId Subscribe(InputActionId action_id, InputActionPhase phase,
                                             InputActionListener listener);
        static void Unsubscribe(InputSubscriptionId subscription_id);

    private:
        static InputManager* input_manager_;
    };

} // namespace dodoe
