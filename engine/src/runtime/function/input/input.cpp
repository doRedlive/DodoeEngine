// do@Redlive

#include "input.h"

#include "input_manager.h"

namespace dodoe {

    InputManager* Input::input_manager_ = nullptr;

    void Input::initialize(InputManager* input_manager) { input_manager_ = input_manager; }
    void Input::shutdown() { input_manager_ = nullptr; }

    Bool Input::RegisterActionMap(const String& map_name, Int priority) {
        return input_manager_ && input_manager_->registerActionMap(map_name, priority);
    }
    Bool Input::UnregisterActionMap(StringView map_name) {
        return input_manager_ && input_manager_->unregisterActionMap(map_name);
    }
    Bool Input::SetActionMapEnabled(StringView map_name, Bool enabled) {
        return input_manager_ && input_manager_->setActionMapEnabled(map_name, enabled);
    }
    Bool Input::SetActionMapConsume(StringView map_name, Bool consume) {
        return input_manager_ && input_manager_->setActionMapConsume(map_name, consume);
    }
    Bool Input::PushInputContext(StringView map_name) {
        return input_manager_ && input_manager_->pushInputContext(map_name);
    }
    Bool Input::PopInputContext(StringView map_name) {
        return input_manager_ && input_manager_->popInputContext(map_name);
    }
    Bool Input::RegisterAction(StringView map_name, const String& action_name, InputActionValueType value_type) {
        return input_manager_ && input_manager_->registerAction(map_name, action_name, value_type);
    }
    Bool Input::BindKey(StringView map_name, StringView action_name, KeyCode key, Float scale) {
        return input_manager_ && input_manager_->bindKey(map_name, action_name, key, scale);
    }
    Bool Input::BindKey2D(StringView map_name, StringView action_name, KeyCode key, Vector2f scale) {
        return input_manager_ && input_manager_->bindKey2D(map_name, action_name, key, scale);
    }
    Bool Input::BindMouseButton(StringView map_name, StringView action_name, MouseCode button, Float scale) {
        return input_manager_ && input_manager_->bindMouseButton(map_name, action_name, button, scale);
    }
    Bool Input::BindMouseDelta(StringView map_name, StringView action_name, Float scale) {
        return input_manager_ && input_manager_->bindMouseDelta(map_name, action_name, scale);
    }
    Bool Input::BindMouseWheel(StringView map_name, StringView action_name, Float scale) {
        return input_manager_ && input_manager_->bindMouseWheel(map_name, action_name, scale);
    }
    Bool Input::SetBindingInteraction(StringView map_name, StringView action_name,
                                       InputInteraction interaction, Float hold_seconds) {
        return input_manager_ && input_manager_->setBindingInteraction(map_name, action_name, interaction, hold_seconds);
    }
    Bool Input::LoadActionAsset(const String& absolute_path) {
        return input_manager_ && input_manager_->loadActionAsset(absolute_path);
    }

    Bool Input::SetBindingTapParams(StringView map_name, StringView action_name, Size_t binding_index,
                                    Int tap_count, Float tap_window_seconds) {
        return input_manager_ && input_manager_->setBindingTapParams(map_name, action_name, binding_index,
                                                                     tap_count, tap_window_seconds);
    }
    Bool Input::SetBindingRepeatParams(StringView map_name, StringView action_name, Size_t binding_index,
                                       Float repeat_delay, Float repeat_rate) {
        return input_manager_ && input_manager_->setBindingRepeatParams(map_name, action_name, binding_index,
                                                                        repeat_delay, repeat_rate);
    }
    Bool Input::SetBindingProcessor(StringView map_name, StringView action_name, Size_t binding_index,
                                    const InputProcessor& processor) {
        return input_manager_ && input_manager_->setBindingProcessor(map_name, action_name, binding_index, processor);
    }
    Bool Input::BindGamepadButton(StringView map_name, StringView action_name, GamepadButtonCode button,
                                  InputDeviceId device_id, Float scale) {
        return input_manager_ && input_manager_->bindGamepadButton(map_name, action_name, button, device_id, scale);
    }
    Bool Input::BindGamepadAxis(StringView map_name, StringView action_name, GamepadAxisCode axis,
                                InputDeviceId device_id, Float scale) {
        return input_manager_ && input_manager_->bindGamepadAxis(map_name, action_name, axis, device_id, scale);
    }
    Bool Input::BindGamepadStick(StringView map_name, StringView action_name, GamepadAxisCode stick_axis,
                                 InputDeviceId device_id, Float scale) {
        return input_manager_ && input_manager_->bindGamepadStick(map_name, action_name, stick_axis, device_id, scale);
    }
    Bool Input::BindComposite(StringView map_name, StringView action_name,
                              const DynamicArray<InputBinding>& parts, InputDeviceId device_id) {
        return input_manager_ && input_manager_->bindComposite(map_name, action_name, parts, device_id);
    }
    Bool Input::SetBindingOverride(StringView map_name, StringView action_name, Size_t binding_index,
                                   const InputBinding& binding) {
        return input_manager_ && input_manager_->setBindingOverride(map_name, action_name, binding_index, binding);
    }
    Bool Input::ClearBindingOverride(StringView map_name, StringView action_name, Size_t binding_index) {
        return input_manager_ && input_manager_->clearBindingOverride(map_name, action_name, binding_index);
    }
    Bool Input::GetBinding(StringView map_name, StringView action_name, Size_t binding_index, InputBinding& out) {
        return input_manager_ && input_manager_->getBinding(map_name, action_name, binding_index, out);
    }
    Bool Input::BeginRebindSession(StringView map_name, StringView action_name, Size_t binding_index) {
        return input_manager_ && input_manager_->beginRebindSession(map_name, action_name, binding_index);
    }
    void Input::CancelRebindSession() {
        if (input_manager_) input_manager_->cancelRebindSession();
    }
    Bool Input::IsRebindSessionActive() {
        return input_manager_ && input_manager_->isRebindSessionActive();
    }
    Bool Input::LoadConfigOverrides(const FsPath& project_path, const FsPath& user_path) {
        return input_manager_ && input_manager_->loadConfigOverrides(project_path, user_path);
    }
    Bool Input::SaveUserConfigOverrides(const FsPath& user_path) {
        return input_manager_ && input_manager_->saveUserConfigOverrides(user_path);
    }

    InputActionId Input::FindActionId(StringView map_name, StringView action_name) {
        return input_manager_ ? input_manager_->findActionId(map_name, action_name) : kInvalidInputActionId;
    }
    InputActionId Input::FindActionId(StringView qualified_name) {
        return input_manager_ ? input_manager_->findActionId(qualified_name) : kInvalidInputActionId;
    }

    Bool Input::IsActionDown(StringView action_name) {
        return input_manager_ && input_manager_->isActionDown(action_name);
    }
    Bool Input::WasActionPressed(StringView action_name) {
        return input_manager_ && input_manager_->wasActionPressed(action_name);
    }
    Bool Input::WasActionReleased(StringView action_name) {
        return input_manager_ && input_manager_->wasActionReleased(action_name);
    }
    Float Input::GetActionAxis(StringView action_name) {
        return input_manager_ ? input_manager_->getActionAxis(action_name) : 0.0f;
    }
    Vector2f Input::GetActionVector2(StringView action_name) {
        return input_manager_ ? input_manager_->getActionVector2(action_name) : Vector2f(0.0f);
    }
    Vector2f Input::GetMousePosition() {
        return input_manager_ ? input_manager_->getMousePosition() : Vector2f(0.0f);
    }
    Vector2f Input::GetMouseDelta() {
        return input_manager_ ? input_manager_->getMouseDelta() : Vector2f(0.0f);
    }
    Vector2f Input::GetMouseWheel() {
        return input_manager_ ? input_manager_->getMouseWheel() : Vector2f(0.0f);
    }

    Bool Input::IsGamepadConnected(InputDeviceId device_id) {
        return input_manager_ && input_manager_->isGamepadConnected(device_id);
    }
    Bool Input::IsGamepadButtonDown(InputDeviceId device_id, GamepadButtonCode button) {
        return input_manager_ && input_manager_->isGamepadButtonDown(device_id, button);
    }
    Bool Input::IsGamepadButtonPressed(InputDeviceId device_id, GamepadButtonCode button) {
        return input_manager_ && input_manager_->isGamepadButtonPressed(device_id, button);
    }
    Bool Input::IsGamepadButtonReleased(InputDeviceId device_id, GamepadButtonCode button) {
        return input_manager_ && input_manager_->isGamepadButtonReleased(device_id, button);
    }
    Float Input::GetGamepadAxis(InputDeviceId device_id, GamepadAxisCode axis) {
        return input_manager_ ? input_manager_->getGamepadAxis(device_id, axis) : 0.0f;
    }

    Bool Input::IsActionDown(InputActionId action_id) {
        return input_manager_ && input_manager_->isActionDown(action_id);
    }
    Bool Input::WasActionPressed(InputActionId action_id) {
        return input_manager_ && input_manager_->wasActionPressed(action_id);
    }
    Bool Input::WasActionReleased(InputActionId action_id) {
        return input_manager_ && input_manager_->wasActionReleased(action_id);
    }
    Float Input::GetActionAxis(InputActionId action_id) {
        return input_manager_ ? input_manager_->getActionAxis(action_id) : 0.0f;
    }
    Vector2f Input::GetActionVector2(InputActionId action_id) {
        return input_manager_ ? input_manager_->getActionVector2(action_id) : Vector2f(0.0f);
    }

    InputSubscriptionId Input::Subscribe(StringView action_name, InputActionPhase phase,
                                          InputActionListener listener) {
        return input_manager_ ? input_manager_->subscribe(action_name, phase, std::move(listener)) : 0;
    }
    InputSubscriptionId Input::Subscribe(InputActionId action_id, InputActionPhase phase,
                                          InputActionListener listener) {
        return input_manager_ ? input_manager_->subscribe(action_id, phase, std::move(listener)) : 0;
    }
    void Input::Unsubscribe(InputSubscriptionId subscription_id) {
        if (input_manager_) input_manager_->unsubscribe(subscription_id);
    }

} // namespace dodoe
