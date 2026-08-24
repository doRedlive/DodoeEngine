// do@Redlive

#include "input_manager.h"

#include <cmath>
#include <limits>

#include "runtime/core/utils/json.h"

#include "runtime/function/input/input.h"
#include "runtime/function/input/input_serialization.h"
#include "runtime/function/input/backend/input_backend_glfw.h"
#include "runtime/function/input/backend/input_backend_null.h"
#include "runtime/function/input/backend/input_backend_qt.h"
#include "runtime/core/event/event_system.h"

namespace dodoe {

    namespace {
        constexpr Size_t kMouseButtonCount = 8;

        std::pair<StringView, StringView> splitActionName(StringView action_name) {
            const Size_t separator = action_name.find('/');
            if (separator == StringView::npos) return {{}, action_name};
            return {action_name.substr(0, separator), action_name.substr(separator + 1)};
        }

        Float axisFromValue(const InputActionValue& value) {
            if (const auto* axis = std::get_if<Float>(&value)) return *axis;
            if (const auto* button = std::get_if<Bool>(&value)) return *button ? 1.0f : 0.0f;
            return 0.0f;
        }

        Vector2f vectorFromValue(const InputActionValue& value) {
            if (const auto* vector = std::get_if<Vector2f>(&value)) return *vector;
            return Vector2f(axisFromValue(value), 0.0f);
        }

        Size_t gamepadSlot(InputDeviceId device_id) {
            return device_id >= kGamepadDeviceIdBase ? device_id - kGamepadDeviceIdBase : 0;
        }

        UInt32 controlKey(const InputBinding& binding) {
            switch (binding.type) {
            case InputBindingType::Key: return static_cast<UInt32>(binding.key);
            case InputBindingType::MouseButton: return (1u << 16) | static_cast<UInt32>(binding.mouse);
            case InputBindingType::GamepadButton:
                return (2u << 16) | (static_cast<UInt32>(gamepadSlot(binding.device_id)) << 8) |
                       static_cast<UInt32>(binding.gamepad_button);
            case InputBindingType::GamepadAxis:
                return (3u << 16) | (static_cast<UInt32>(gamepadSlot(binding.device_id)) << 8) |
                       static_cast<UInt32>(binding.gamepad_axis);
            default: return 0xFFFFFFFFu;
            }
        }

    }

    Bool InputManager::initialize(const InputManagerInitInfo& init_info) {
        m_has_focus_ = !init_info.host_mode;
        if (!init_info.host_mode && init_info.native_window) {
            m_backend_ = new InputBackendGlfw();
            m_backend_->initialize(m_raw_state_, init_info.native_window);
        } else {
            m_backend_ = new InputBackendQt();
            m_backend_->initialize(m_raw_state_, init_info.native_window);
        }

        EventSystem::Subscribe<WindowLostFocusEvent, &InputManager::on_window_lost_focus_>(this);

        Input::initialize(this);
        return true;
    }

    void InputManager::beginFrame() {
        for (auto& [key, state] : m_raw_state_.keys) {
            (void)key;
            state.pressed_this_frame = false;
            state.released_this_frame = false;
        }
        for (auto& state : m_raw_state_.mouse_buttons) {
            state.pressed_this_frame = false;
            state.released_this_frame = false;
        }
        m_raw_state_.mouse_delta = Vector2f(0.0f, 0.0f);
        m_raw_state_.mouse_wheel = Vector2f(0.0f, 0.0f);
        for (auto& pad : m_raw_state_.gamepads) {
            for (auto& state : pad.buttons) {
                state.pressed_this_frame = false;
                state.released_this_frame = false;
            }
        }

        for (auto& [map_name, map] : action_maps_) {
            (void)map_name;
            for (auto& [action_name, action] : map.actions) {
                (void)action_name;
                action.state.pressed_this_frame = false;
                action.state.released_this_frame = false;
                if (action.state.phase == InputActionPhase::Canceled) {
                    action.state.phase = InputActionPhase::Waiting;
                }
            }
        }
    }

    void InputManager::update(Float delta_time) {
        if (m_backend_) m_backend_->poll(m_raw_state_);
        updateRebindSession_();

        DynamicArray<ActionMap*> maps;
        maps.reserve(context_stack_.size());
        for (auto it = context_stack_.rbegin(); it != context_stack_.rend(); ++it) {
            const auto map_it = action_maps_.find(*it);
            if (map_it != action_maps_.end() && map_it->second.enabled) maps.push_back(&map_it->second);
        }

        std::unordered_set<UInt32> consumed_controls;
        Bool pointer_consumed = false;

        for (ActionMap* map_ptr : maps) {
            ActionMap& map = *map_ptr;

            for (auto& [action_name, action] : map.actions) {
                (void)action_name;
                Float scalar_value = 0.0f;
                Vector2f vector_value(0.0f, 0.0f);
                Bool button_down = false;
                Bool started = false, performed = false, canceled = false, released_any = false;
                DynamicArray<UInt32> active_controls;
                Bool active_pointer = false;

                Bool has_chord = false;
                for (const auto& binding : action.bindings) {
                    if (binding.interaction == InputInteraction::Chord) {
                        has_chord = true;
                        break;
                    }
                }
                Bool chord_ready = false;
                if (has_chord) {
                    chord_ready = true;
                    for (const auto& binding : action.bindings) {
                        if (!readControl_(binding).down) {
                            chord_ready = false;
                            break;
                        }
                    }
                }

                for (Size_t binding_index = 0; binding_index < action.bindings.size(); ++binding_index) {
                    const InputBinding& binding = action.bindings[binding_index];
                    const Bool consumed = isBindingConsumed_(binding, consumed_controls, pointer_consumed);
                    const ControlSnapshot snapshot = readControl_(binding);

                    if (map.consume_input && !consumed &&
                        (snapshot.down || snapshot.pressed || snapshot.released)) {
                        const UInt32 key = controlKey(binding);
                        if (key != 0xFFFFFFFFu) active_controls.push_back(key);
                        else active_pointer = true;
                    }

                    switch (action.value_type) {
                    case InputActionValueType::Button: {
                        const BindingResult result = evaluateButtonBinding_(
                            binding, snapshot, chord_ready, delta_time, action.binding_states[binding_index]);
                        if (!consumed) {
                            button_down = button_down || result.effective_down;
                            started = started || result.started;
                            performed = performed || result.performed;
                            canceled = canceled || result.canceled;
                            released_any = released_any || result.released;
                        }
                        break;
                    }
                    case InputActionValueType::Axis1D:
                        if (!consumed) scalar_value += processValue_(binding, snapshot.value);
                        break;
                    case InputActionValueType::Axis2D:
                        if (!consumed) vector_value += processVector_(binding, readVector2_(binding, snapshot));
                        break;
                    }
                }

                if (map.consume_input) {
                    for (const UInt32 key : active_controls) consumed_controls.insert(key);
                    if (active_pointer) pointer_consumed = true;
                }

                const Bool was_down = action.state.is_down;
                Bool is_down = false;
                InputActionValue value = false;
                switch (action.value_type) {
                case InputActionValueType::Button:
                    is_down = button_down;
                    value = button_down;
                    action.state.pressed_this_frame = started;
                    action.state.released_this_frame = released_any;
                    break;
                case InputActionValueType::Axis1D:
                    value = scalar_value;
                    is_down = std::abs(scalar_value) > 0.0001f;
                    break;
                case InputActionValueType::Axis2D:
                    value = vector_value;
                    is_down = glm::length(vector_value) > 0.0001f;
                    break;
                }

                action.state.value = value;
                action.state.is_down = is_down;

                if (action.value_type == InputActionValueType::Button) {
                    if (started) {
                        action.state.phase = InputActionPhase::Started;
                        emit_(map, action, InputActionPhase::Started, value);
                    }
                    if (performed) {
                        action.state.phase = InputActionPhase::Performed;
                        emit_(map, action, InputActionPhase::Performed, value);
                    }
                    if (canceled) {
                        action.state.phase = InputActionPhase::Canceled;
                        emit_(map, action, InputActionPhase::Canceled, value);
                    }
                    if (!started && !performed && !canceled) {
                        action.state.phase = is_down ? InputActionPhase::Performed : InputActionPhase::Waiting;
                    }
                } else if (!was_down && is_down) {
                    action.state.phase = InputActionPhase::Started;
                    emit_(map, action, InputActionPhase::Started, value);
                    action.state.phase = InputActionPhase::Performed;
                    emit_(map, action, InputActionPhase::Performed, value);
                } else if (is_down) {
                    action.state.phase = InputActionPhase::Performed;
                    emit_(map, action, InputActionPhase::Performed, value);
                } else if (was_down && !is_down) {
                    action.state.phase = InputActionPhase::Canceled;
                    action.state.released_this_frame = true;
                    emit_(map, action, InputActionPhase::Canceled, value);
                } else {
                    action.state.phase = InputActionPhase::Waiting;
                }
            }
        }
    }

    Bool InputManager::registerActionMap(const String& map_name, Int priority) {
        if (map_name.empty() || action_maps_.contains(map_name)) return false;
        ActionMap map;
        map.name = map_name;
        map.priority = priority;
        action_maps_.emplace(map_name, std::move(map));
        return true;
    }

    Bool InputManager::unregisterActionMap(StringView map_name) {
        const String name(map_name);
        const auto it = action_maps_.find(name);
        if (it == action_maps_.end()) return false;
        cancelMapActions_(it->second);
        for (const auto& [action_name, action] : it->second.actions) {
            (void)action_name;
            action_by_id_.erase(action.id);
        }
        context_stack_.erase(std::remove(context_stack_.begin(), context_stack_.end(), name), context_stack_.end());
        action_maps_.erase(it);
        return true;
    }

    Bool InputManager::setActionMapEnabled(StringView map_name, Bool enabled) {
        auto it = action_maps_.find(String(map_name));
        if (it == action_maps_.end()) return false;
        if (it->second.enabled && !enabled) cancelMapActions_(it->second);
        it->second.enabled = enabled;
        return true;
    }

    Bool InputManager::setActionMapConsume(StringView map_name, Bool consume) {
        auto it = action_maps_.find(String(map_name));
        if (it == action_maps_.end()) return false;
        it->second.consume_input = consume;
        return true;
    }

    Bool InputManager::pushInputContext(StringView map_name) {
        const String name(map_name);
        if (!action_maps_.contains(name)) return false;
        context_stack_.erase(std::remove(context_stack_.begin(), context_stack_.end(), name), context_stack_.end());
        context_stack_.push_back(name);
        return true;
    }

    Bool InputManager::popInputContext(StringView map_name) {
        const String name(map_name);
        const auto it = std::find(context_stack_.begin(), context_stack_.end(), name);
        if (it == context_stack_.end()) return false;
        const auto map_it = action_maps_.find(name);
        if (map_it != action_maps_.end()) cancelMapActions_(map_it->second);
        context_stack_.erase(it);
        return true;
    }

    Bool InputManager::registerAction(StringView map_name, const String& action_name,
                                      InputActionValueType value_type) {
        auto it = action_maps_.find(String(map_name));
        if (it == action_maps_.end() || action_name.empty() || it->second.actions.contains(action_name)) return false;

        ActionDefinition action;
        action.id = next_action_id_++;
        action.name = action_name;
        action.map_name = it->first;
        action.value_type = value_type;
        action.state.value_type = value_type;
        action.state.value = value_type == InputActionValueType::Button ? InputActionValue(false) :
            (value_type == InputActionValueType::Axis1D ? InputActionValue(0.0f) :
                InputActionValue(Vector2f(0.0f)));
        auto& stored = it->second.actions.emplace(action_name, std::move(action)).first->second;
        action_by_id_[stored.id] = &stored;
        return true;
    }

    Bool InputManager::bindKey(StringView map_name, StringView action_name, KeyCode key, Float scale) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::Key)) return false;
        action->bindings.push_back(InputBinding{InputBindingType::Key, key, MouseCode::None, scale,
                                                Vector2f(scale, 0.0f)});
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::bindKey2D(StringView map_name, StringView action_name, KeyCode key, Vector2f scale) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::Key)) return false;
        action->bindings.push_back(InputBinding{InputBindingType::Key, key, MouseCode::None, 1.0f, scale});
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::bindMouseButton(StringView map_name, StringView action_name, MouseCode button, Float scale) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::MouseButton)) return false;
        action->bindings.push_back(InputBinding{InputBindingType::MouseButton, KeyCode::Space, button, scale,
                                                Vector2f(scale, 0.0f)});
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::bindMouseDelta(StringView map_name, StringView action_name, Float scale) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::MouseDelta)) return false;
        action->bindings.push_back(InputBinding{InputBindingType::MouseDelta, KeyCode::Space, MouseCode::None, scale,
                                                Vector2f(scale)});
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::bindMouseWheel(StringView map_name, StringView action_name, Float scale) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::MouseWheel)) return false;
        action->bindings.push_back(InputBinding{InputBindingType::MouseWheel, KeyCode::Space, MouseCode::None, scale,
                                                Vector2f(scale)});
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::bindGamepadButton(StringView map_name, StringView action_name, GamepadButtonCode button,
                                         InputDeviceId device_id, Float scale) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::GamepadButton)) return false;
        InputBinding binding;
        binding.type = InputBindingType::GamepadButton;
        binding.gamepad_button = button;
        binding.device_id = device_id;
        binding.scale = scale;
        binding.vector_scale = Vector2f(scale, 0.0f);
        action->bindings.push_back(binding);
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::bindGamepadAxis(StringView map_name, StringView action_name, GamepadAxisCode axis,
                                       InputDeviceId device_id, Float scale) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::GamepadAxis)) return false;
        InputBinding binding;
        binding.type = InputBindingType::GamepadAxis;
        binding.gamepad_axis = axis;
        binding.device_id = device_id;
        binding.scale = scale;
        action->bindings.push_back(binding);
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::bindGamepadStick(StringView map_name, StringView action_name, GamepadAxisCode stick_axis,
                                        InputDeviceId device_id, Float scale) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::GamepadAxis)) return false;
        if (stick_axis != GamepadAxisCode::LeftX && stick_axis != GamepadAxisCode::RightX) return false;
        InputBinding binding;
        binding.type = InputBindingType::GamepadAxis;
        binding.gamepad_axis = stick_axis;
        binding.device_id = device_id;
        binding.scale = scale;
        binding.vector_scale = Vector2f(scale);
        action->bindings.push_back(binding);
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::bindComposite(StringView map_name, StringView action_name,
                                     const DynamicArray<InputBinding>& parts, InputDeviceId device_id) {
        auto* action = findAction_(map_name, action_name);
        if (!action || !isTypeCompatible_(action->value_type, InputBindingType::Composite) || parts.empty())
            return false;
        InputBinding binding;
        binding.type = InputBindingType::Composite;
        binding.device_id = device_id;
        binding.composite_parts = parts;
        action->bindings.push_back(binding);
        action->binding_states.emplace_back();
        return true;
    }

    Bool InputManager::setBindingInteraction(StringView map_name, StringView action_name,
                                              InputInteraction interaction, Float hold_seconds) {
        auto* action = findAction_(map_name, action_name);
        if (!action || action->bindings.empty()) return false;
        for (auto& binding : action->bindings) {
            binding.interaction = interaction;
            binding.hold_seconds = hold_seconds;
        }
        for (auto& binding_state : action->binding_states) {
            binding_state = BindingState{};
        }
        return true;
    }

    Bool InputManager::setBindingTapParams(StringView map_name, StringView action_name, Size_t binding_index,
                                           Int tap_count, Float tap_window_seconds) {
        auto* action = findAction_(map_name, action_name);
        if (!action || binding_index >= action->bindings.size()) return false;
        action->bindings[binding_index].tap_count = tap_count;
        action->bindings[binding_index].tap_window_seconds = tap_window_seconds;
        action->binding_states[binding_index] = BindingState{};
        return true;
    }

    Bool InputManager::setBindingRepeatParams(StringView map_name, StringView action_name, Size_t binding_index,
                                              Float repeat_delay, Float repeat_rate) {
        auto* action = findAction_(map_name, action_name);
        if (!action || binding_index >= action->bindings.size()) return false;
        action->bindings[binding_index].repeat_delay = repeat_delay;
        action->bindings[binding_index].repeat_rate = repeat_rate;
        action->binding_states[binding_index] = BindingState{};
        return true;
    }

    Bool InputManager::setBindingProcessor(StringView map_name, StringView action_name, Size_t binding_index,
                                           const InputProcessor& processor) {
        auto* action = findAction_(map_name, action_name);
        if (!action || binding_index >= action->bindings.size()) return false;
        action->bindings[binding_index].processors.push_back(processor);
        return true;
    }

    InputActionId InputManager::findActionId(StringView map_name, StringView action_name) const {
        const auto* action = findAction_(map_name, action_name);
        return action ? action->id : kInvalidInputActionId;
    }

    InputActionId InputManager::findActionId(StringView qualified_name) const {
        const auto [map_name, action_name] = splitActionName(qualified_name);
        if (!map_name.empty()) {
            const auto* action = findAction_(map_name, action_name);
            return action ? action->id : kInvalidInputActionId;
        }
        const auto* action = findAction_(action_name);
        return action ? action->id : kInvalidInputActionId;
    }

    Bool InputManager::isGamepadConnected(InputDeviceId device_id) const {
        const Size_t slot = gamepadSlot(device_id);
        return slot < kMaxGamepads && m_raw_state_.gamepads[slot].connected;
    }

    Bool InputManager::isGamepadButtonDown(InputDeviceId device_id, GamepadButtonCode button) const {
        const Size_t slot = gamepadSlot(device_id);
        const Size_t index = static_cast<Size_t>(button);
        if (slot >= kMaxGamepads || index >= kGamepadButtonCount) return false;
        return m_raw_state_.gamepads[slot].connected && m_raw_state_.gamepads[slot].buttons[index].down;
    }

    Bool InputManager::isGamepadButtonPressed(InputDeviceId device_id, GamepadButtonCode button) const {
        const Size_t slot = gamepadSlot(device_id);
        const Size_t index = static_cast<Size_t>(button);
        if (slot >= kMaxGamepads || index >= kGamepadButtonCount) return false;
        return m_raw_state_.gamepads[slot].connected && m_raw_state_.gamepads[slot].buttons[index].pressed_this_frame;
    }

    Bool InputManager::isGamepadButtonReleased(InputDeviceId device_id, GamepadButtonCode button) const {
        const Size_t slot = gamepadSlot(device_id);
        const Size_t index = static_cast<Size_t>(button);
        if (slot >= kMaxGamepads || index >= kGamepadButtonCount) return false;
        return m_raw_state_.gamepads[slot].connected && m_raw_state_.gamepads[slot].buttons[index].released_this_frame;
    }

    Float InputManager::getGamepadAxis(InputDeviceId device_id, GamepadAxisCode axis) const {
        const Size_t slot = gamepadSlot(device_id);
        const Size_t index = static_cast<Size_t>(axis);
        if (slot >= kMaxGamepads || index >= kGamepadAxisCount) return 0.0f;
        const GamepadState& pad = m_raw_state_.gamepads[slot];
        if (!pad.connected) return 0.0f;
        const Float value = pad.axes[index];
        return std::abs(value) < pad.dead_zone ? 0.0f : value;
    }

    Bool InputManager::isActionDown(StringView action_name) const {
        const auto* action = findAction_(action_name);
        return action ? action->state.is_down : false;
    }

    Bool InputManager::wasActionPressed(StringView action_name) const {
        const auto* action = findAction_(action_name);
        return action ? action->state.pressed_this_frame : false;
    }

    Bool InputManager::wasActionReleased(StringView action_name) const {
        const auto* action = findAction_(action_name);
        return action ? action->state.released_this_frame : false;
    }

    Float InputManager::getActionAxis(StringView action_name) const {
        const auto* action = findAction_(action_name);
        return action ? axisFromValue(action->state.value) : 0.0f;
    }

    Vector2f InputManager::getActionVector2(StringView action_name) const {
        const auto* action = findAction_(action_name);
        return action ? vectorFromValue(action->state.value) : Vector2f(0.0f);
    }

    Bool InputManager::isActionDown(InputActionId action_id) const {
        const auto* action = findAction_(action_id);
        return action ? action->state.is_down : false;
    }

    Bool InputManager::wasActionPressed(InputActionId action_id) const {
        const auto* action = findAction_(action_id);
        return action ? action->state.pressed_this_frame : false;
    }

    Bool InputManager::wasActionReleased(InputActionId action_id) const {
        const auto* action = findAction_(action_id);
        return action ? action->state.released_this_frame : false;
    }

    Float InputManager::getActionAxis(InputActionId action_id) const {
        const auto* action = findAction_(action_id);
        return action ? axisFromValue(action->state.value) : 0.0f;
    }

    Vector2f InputManager::getActionVector2(InputActionId action_id) const {
        const auto* action = findAction_(action_id);
        return action ? vectorFromValue(action->state.value) : Vector2f(0.0f);
    }

    InputSubscriptionId InputManager::subscribe(StringView action_name, InputActionPhase phase,
                                                  InputActionListener listener) {
        if (action_name.empty() || !listener) return 0;
        const InputSubscriptionId id = next_subscription_id_++;
        subscriptions_.push_back(Subscription{id, String(action_name), phase, std::move(listener)});
        return id;
    }

    InputSubscriptionId InputManager::subscribe(InputActionId action_id, InputActionPhase phase,
                                                  InputActionListener listener) {
        const auto* action = findAction_(action_id);
        if (!action) return 0;
        return subscribe(action->map_name + "/" + action->name, phase, std::move(listener));
    }

    void InputManager::unsubscribe(InputSubscriptionId subscription_id) {
        subscriptions_.erase(std::remove_if(subscriptions_.begin(), subscriptions_.end(),
            [subscription_id](const Subscription& subscription) { return subscription.id == subscription_id; }),
            subscriptions_.end());
    }

    Bool InputManager::setBindingOverride(StringView map_name, StringView action_name, Size_t binding_index,
                                          const InputBinding& binding) {
        auto* action = findAction_(map_name, action_name);
        if (!action || binding_index >= action->bindings.size()) return false;
        action->bindings[binding_index] = binding;
        action->binding_states[binding_index] = BindingState{};
        const String map(map_name);
        const String action_key(action_name);
        const auto it = std::find_if(overrides_.begin(), overrides_.end(),
            [&](const InputBindingOverride& override) {
                return override.map_name == map && override.action_name == action_key &&
                       override.binding_index == binding_index;
            });
        if (it != overrides_.end()) {
            it->binding = binding;
        } else {
            overrides_.push_back(InputBindingOverride{map, action_key, binding_index, binding});
        }
        return true;
    }

    Bool InputManager::clearBindingOverride(StringView map_name, StringView action_name, Size_t binding_index) {
        const String map(map_name);
        const String action_key(action_name);
        const auto it = std::find_if(overrides_.begin(), overrides_.end(),
            [&](const InputBindingOverride& override) {
                return override.map_name == map && override.action_name == action_key &&
                       override.binding_index == binding_index;
            });
        if (it == overrides_.end()) return false;
        overrides_.erase(it);
        return true;
    }

    Bool InputManager::getBinding(StringView map_name, StringView action_name, Size_t binding_index,
                                  InputBinding& out) const {
        const auto* action = findAction_(map_name, action_name);
        if (!action || binding_index >= action->bindings.size()) return false;
        out = action->bindings[binding_index];
        return true;
    }

    void InputManager::applyBindingOverrides() {
        for (const auto& override : overrides_) {
            auto* action = findAction_(override.map_name, override.action_name);
            if (!action || override.binding_index >= action->bindings.size()) continue;
            action->bindings[override.binding_index] = override.binding;
            action->binding_states[override.binding_index] = BindingState{};
        }
    }

    Bool InputManager::loadConfigOverrides(const FsPath& project_path, const FsPath& user_path) {
        overrides_.clear();
        for (const FsPath& path : {project_path, user_path}) {
            std::ifstream file(path);
            if (!file.is_open()) continue;
            Json json;
            try {
                std::stringstream buffer;
                buffer << file.rdbuf();
                json = Json::parse(buffer.str());
            } catch (const Json::exception&) {
                continue;
            }
            if (!json.contains("overrides") || !json["overrides"].is_array()) continue;
            for (const auto& override_json : json["overrides"]) {
                InputBindingOverride override;
                override.map_name = override_json.value("map", String{});
                override.action_name = override_json.value("action", String{});
                override.binding_index = override_json.value("index", 0);
                if (override_json.contains("binding")) override.binding = ParseInputBinding(override_json["binding"]);
                if (!override.map_name.empty() && !override.action_name.empty()) overrides_.push_back(override);
            }
        }
        applyBindingOverrides();
        return true;
    }

    Bool InputManager::saveUserConfigOverrides(const FsPath& user_path) const {
        std::ofstream file(user_path);
        if (!file.is_open()) return false;
        Json root;
        root["overrides"] = Json::array();
        for (const auto& override : overrides_) {
            Json override_json;
            override_json["map"] = override.map_name;
            override_json["action"] = override.action_name;
            override_json["index"] = override.binding_index;
            override_json["binding"] = InputBindingToJson(override.binding);
            root["overrides"].push_back(std::move(override_json));
        }
        file << root.dump(4);
        return true;
    }

    Bool InputManager::beginRebindSession(StringView map_name, StringView action_name, Size_t binding_index) {
        auto* action = findAction_(map_name, action_name);
        if (!action || binding_index >= action->bindings.size()) return false;
        m_rebind_session_.active = true;
        m_rebind_session_.map_name = String(map_name);
        m_rebind_session_.action_name = String(action_name);
        m_rebind_session_.binding_index = binding_index;
        return true;
    }

    void InputManager::cancelRebindSession() {
        m_rebind_session_.active = false;
        m_rebind_session_.map_name.clear();
        m_rebind_session_.action_name.clear();
        m_rebind_session_.binding_index = 0;
    }

    void InputManager::updateRebindSession_() {
        if (!m_rebind_session_.active) return;
        auto* action = findAction_(m_rebind_session_.map_name, m_rebind_session_.action_name);
        if (!action || m_rebind_session_.binding_index >= action->bindings.size()) {
            cancelRebindSession();
            return;
        }
        InputBinding captured;
        if (!captureNextControl_(captured)) return;

        InputBinding& binding = action->bindings[m_rebind_session_.binding_index];
        const Vector2f vector_scale = binding.vector_scale;
        const InputInteraction interaction = binding.interaction;
        const Float scale = binding.scale;
        binding.type = captured.type;
        binding.key = captured.key;
        binding.mouse = captured.mouse;
        binding.gamepad_button = captured.gamepad_button;
        binding.gamepad_axis = captured.gamepad_axis;
        binding.device_id = captured.device_id;
        binding.scale = scale;
        binding.vector_scale = vector_scale;
        binding.interaction = interaction;
        action->binding_states[m_rebind_session_.binding_index] = BindingState{};
        setBindingOverride(m_rebind_session_.map_name, m_rebind_session_.action_name,
                           m_rebind_session_.binding_index, binding);
        cancelRebindSession();
    }

    Bool InputManager::captureNextControl_(InputBinding& out) {
        for (const auto& [key, state] : m_raw_state_.keys) {
            if (state.pressed_this_frame) {
                out.type = InputBindingType::Key;
                out.key = key;
                out.device_id = kKeyboardDeviceId;
                return true;
            }
        }
        for (Size_t index = 0; index < m_raw_state_.mouse_buttons.size(); ++index) {
            if (m_raw_state_.mouse_buttons[index].pressed_this_frame) {
                out.type = InputBindingType::MouseButton;
                out.mouse = static_cast<MouseCode>(index);
                out.device_id = kMouseDeviceId;
                return true;
            }
        }
        for (Size_t slot = 0; slot < kMaxGamepads; ++slot) {
            const GamepadState& pad = m_raw_state_.gamepads[slot];
            if (!pad.connected) continue;
            for (Size_t index = 0; index < kGamepadButtonCount; ++index) {
                if (pad.buttons[index].pressed_this_frame) {
                    out.type = InputBindingType::GamepadButton;
                    out.gamepad_button = static_cast<GamepadButtonCode>(index);
                    out.device_id = kGamepadDeviceIdBase + slot;
                    return true;
                }
            }
            for (Size_t index = 0; index < kGamepadAxisCount; ++index) {
                if (std::abs(pad.axes[index]) > 0.5f) {
                    out.type = InputBindingType::GamepadAxis;
                    out.gamepad_axis = static_cast<GamepadAxisCode>(index);
                    out.device_id = kGamepadDeviceIdBase + slot;
                    return true;
                }
            }
        }
        return false;
    }

    void InputManager::shutdown() {
        if (m_backend_) {
            m_backend_->shutdown();
            delete m_backend_;
            m_backend_ = nullptr;
        }
        m_raw_state_.keys.clear();
        action_maps_.clear();
        action_by_id_.clear();
        context_stack_.clear();
        subscriptions_.clear();
        overrides_.clear();

        EventSystem::Unsubscribe<WindowLostFocusEvent, &InputManager::on_window_lost_focus_>(this);

        Input::shutdown();
    }

    void InputManager::on_window_lost_focus_(const WindowLostFocusEvent& event) {
        (void)event;
        for (auto& [key, state] : m_raw_state_.keys) {
            (void)key;
            state.down = false;
            state.released_this_frame = true;
        }
        for (auto& state : m_raw_state_.mouse_buttons) {
            state.down = false;
            state.released_this_frame = true;
        }
    }

    const InputManager::ActionDefinition* InputManager::findAction_(StringView action_name) const {
        const auto [map_name, local_name] = splitActionName(action_name);
        if (!map_name.empty()) {
            const auto map_it = action_maps_.find(String(map_name));
            if (map_it == action_maps_.end()) return nullptr;
            const auto action_it = map_it->second.actions.find(String(local_name));
            return action_it == map_it->second.actions.end() ? nullptr : &action_it->second;
        }
        for (auto it = context_stack_.rbegin(); it != context_stack_.rend(); ++it) {
            const auto map_it = action_maps_.find(*it);
            if (map_it == action_maps_.end() || !map_it->second.enabled) continue;
            const auto action_it = map_it->second.actions.find(String(action_name));
            if (action_it != map_it->second.actions.end()) return &action_it->second;
        }
        return nullptr;
    }

    InputManager::ActionDefinition* InputManager::findAction_(StringView map_name, StringView action_name) {
        auto map_it = action_maps_.find(String(map_name));
        if (map_it == action_maps_.end()) return nullptr;
        auto action_it = map_it->second.actions.find(String(action_name));
        return action_it == map_it->second.actions.end() ? nullptr : &action_it->second;
    }

    const InputManager::ActionDefinition* InputManager::findAction_(StringView map_name,
                                                                    StringView action_name) const {
        const auto map_it = action_maps_.find(String(map_name));
        if (map_it == action_maps_.end()) return nullptr;
        const auto action_it = map_it->second.actions.find(String(action_name));
        return action_it == map_it->second.actions.end() ? nullptr : &action_it->second;
    }

    const InputManager::ActionDefinition* InputManager::findAction_(InputActionId action_id) const {
        const auto it = action_by_id_.find(action_id);
        return it != action_by_id_.end() ? it->second : nullptr;
    }

    InputManager::ControlSnapshot InputManager::readControl_(const InputBinding& binding) const {
        switch (binding.type) {
        case InputBindingType::Key: {
            const auto it = m_raw_state_.keys.find(binding.key);
            if (it == m_raw_state_.keys.end()) return {};
            return {it->second.down, it->second.pressed_this_frame, it->second.released_this_frame,
                    it->second.down ? binding.scale : 0.0f};
        }
        case InputBindingType::MouseButton: {
            const Size_t index = static_cast<Size_t>(binding.mouse);
            if (index >= kMouseButtonCount) return {};
            const InputButtonState& state = m_raw_state_.mouse_buttons[index];
            return {state.down, state.pressed_this_frame, state.released_this_frame,
                    state.down ? binding.scale : 0.0f};
        }
        case InputBindingType::MouseDelta: {
            const Float value = m_raw_state_.mouse_delta.x * binding.scale;
            return {std::abs(value) > 0.0001f, false, false, value};
        }
        case InputBindingType::MouseWheel: {
            const Float value = m_raw_state_.mouse_wheel.y * binding.scale;
            return {std::abs(value) > 0.0001f, false, false, value};
        }
        case InputBindingType::GamepadButton: {
            const Size_t slot = gamepadSlot(binding.device_id);
            const Size_t index = static_cast<Size_t>(binding.gamepad_button);
            if (slot >= kMaxGamepads || index >= kGamepadButtonCount) return {};
            const GamepadState& pad = m_raw_state_.gamepads[slot];
            if (!pad.connected) return {};
            const InputButtonState& state = pad.buttons[index];
            return {state.down, state.pressed_this_frame, state.released_this_frame,
                    state.down ? binding.scale : 0.0f};
        }
        case InputBindingType::GamepadAxis: {
            const Size_t slot = gamepadSlot(binding.device_id);
            const Size_t index = static_cast<Size_t>(binding.gamepad_axis);
            if (slot >= kMaxGamepads || index >= kGamepadAxisCount) return {};
            const GamepadState& pad = m_raw_state_.gamepads[slot];
            if (!pad.connected) return {};
            const Float raw = pad.axes[index];
            const Float dead_zone = binding.dead_zone > 0.0f ? binding.dead_zone : pad.dead_zone;
            const Float value = std::abs(raw) < dead_zone ? 0.0f : raw * binding.scale;
            return {std::abs(value) > 0.0001f, false, false, value};
        }
        default:
            return {};
        }
    }

    Vector2f InputManager::readStick_(const InputBinding& binding) const {
        const Size_t slot = gamepadSlot(binding.device_id);
        if (slot >= kMaxGamepads) return Vector2f(0.0f);
        const GamepadState& pad = m_raw_state_.gamepads[slot];
        if (!pad.connected) return Vector2f(0.0f);
        const auto axis = binding.gamepad_axis;
        Vector2f value(0.0f);
        if (axis == GamepadAxisCode::LeftX || axis == GamepadAxisCode::LeftY) {
            value = Vector2f(pad.axes[0], pad.axes[1]);
        } else if (axis == GamepadAxisCode::RightX || axis == GamepadAxisCode::RightY) {
            value = Vector2f(pad.axes[2], pad.axes[3]);
        } else {
            value = Vector2f(pad.axes[static_cast<Size_t>(axis)], 0.0f);
        }
        const Float dead_zone = binding.dead_zone > 0.0f ? binding.dead_zone : pad.dead_zone;
        return glm::length(value) < dead_zone ? Vector2f(0.0f) : value;
    }

    Vector2f InputManager::readVector2_(const InputBinding& binding, const ControlSnapshot& snapshot) const {
        switch (binding.type) {
        case InputBindingType::Key:
        case InputBindingType::GamepadButton:
            return snapshot.down ? binding.vector_scale * binding.scale : Vector2f(0.0f);
        case InputBindingType::GamepadAxis:
            return readStick_(binding) * binding.scale;
        case InputBindingType::MouseDelta:
            return m_raw_state_.mouse_delta * binding.scale;
        case InputBindingType::MouseWheel:
            return m_raw_state_.mouse_wheel * binding.scale;
        case InputBindingType::Composite: {
            Vector2f result(0.0f);
            for (const auto& part : binding.composite_parts) {
                const ControlSnapshot part_snapshot = readControl_(part);
                if (part_snapshot.down) result += part.vector_scale * part.scale;
            }
            return result;
        }
        default:
            return Vector2f(0.0f);
        }
    }

    Float InputManager::processValue_(const InputBinding& binding, Float value) const {
        Float result = value;
        for (const auto& processor : binding.processors) {
            switch (processor.type) {
            case InputProcessorType::DeadZone:
                if (std::abs(result) < processor.a) result = 0.0f;
                break;
            case InputProcessorType::Normalize:
                if (result > processor.b) result = processor.b;
                if (result < -processor.b) result = -processor.b;
                if (result != 0.0f) result /= processor.b;
                break;
            case InputProcessorType::Scale:
                result *= processor.a;
                break;
            case InputProcessorType::Invert:
                result = -result;
                break;
            case InputProcessorType::Clamp:
                result = glm::clamp(result, processor.a, processor.b);
                break;
            }
        }
        return result;
    }

    Vector2f InputManager::processVector_(const InputBinding& binding, Vector2f value) const {
        Vector2f result = value;
        for (const auto& processor : binding.processors) {
            switch (processor.type) {
            case InputProcessorType::DeadZone:
                if (glm::length(result) < processor.a) result = Vector2f(0.0f);
                break;
            case InputProcessorType::Normalize: {
                const Float length = glm::length(result);
                if (length > processor.b) result *= processor.b / length;
                if (length > 0.0f) result /= processor.b;
                break;
            }
            case InputProcessorType::Scale:
                result *= processor.a;
                break;
            case InputProcessorType::Invert:
                result = -result;
                break;
            case InputProcessorType::Clamp: {
                const Float length = glm::length(result);
                if (length > processor.b) result *= processor.b / length;
                if (length < processor.a) result = Vector2f(0.0f);
                break;
            }
            }
        }
        return result;
    }

    Bool InputManager::isBindingConsumed_(const InputBinding& binding,
                                          const std::unordered_set<UInt32>& controls,
                                          Bool pointer) const {
        switch (binding.type) {
        case InputBindingType::Key:
        case InputBindingType::MouseButton:
        case InputBindingType::GamepadButton:
        case InputBindingType::GamepadAxis:
            return controls.contains(controlKey(binding));
        default:
            return pointer;
        }
    }

    Bool InputManager::isTypeCompatible_(InputActionValueType action_type, InputBindingType binding_type) const {
        switch (action_type) {
        case InputActionValueType::Button:
            return binding_type == InputBindingType::Key ||
                   binding_type == InputBindingType::MouseButton ||
                   binding_type == InputBindingType::GamepadButton;
        case InputActionValueType::Axis1D:
            return true;
        case InputActionValueType::Axis2D:
            return binding_type != InputBindingType::MouseButton;
        }
        return false;
    }

    InputManager::BindingResult InputManager::evaluateButtonBinding_(const InputBinding& binding,
                                                                     const ControlSnapshot& snapshot,
                                                                     Bool chord_ready,
                                                                     Float delta_time,
                                                                     BindingState& binding_state) const {
        const Bool was_down = binding_state.is_down;
        const Bool is_down = snapshot.down;
        const Float elapsed = binding_state.held_seconds;
        binding_state.is_down = is_down;

        if (is_down) binding_state.held_seconds += delta_time;

        BindingResult result;
        result.effective_down = is_down;
        switch (binding.interaction) {
        case InputInteraction::Press:
            if (!was_down && is_down) {
                result.started = true;
                result.performed = true;
                binding_state.phase = InputActionPhase::Performed;
            } else if (was_down && !is_down) {
                result.canceled = true;
                result.released = true;
                binding_state.held_seconds = 0.0f;
                binding_state.phase = InputActionPhase::Canceled;
            } else if (!is_down) {
                binding_state.phase = InputActionPhase::Waiting;
            }
            break;
        case InputInteraction::Hold:
            if (!was_down && is_down) {
                result.started = true;
                binding_state.phase = InputActionPhase::Started;
            } else if (is_down && binding_state.phase != InputActionPhase::Performed &&
                       binding_state.held_seconds >= binding.hold_seconds) {
                result.performed = true;
                binding_state.phase = InputActionPhase::Performed;
            } else if (was_down && !is_down) {
                result.canceled = true;
                result.released = true;
                binding_state.held_seconds = 0.0f;
                binding_state.phase = InputActionPhase::Canceled;
            } else if (!is_down) {
                binding_state.phase = InputActionPhase::Waiting;
            }
            break;
        case InputInteraction::Tap:
            if (!was_down && is_down) {
                result.started = true;
                binding_state.phase = InputActionPhase::Started;
            } else if (is_down && binding_state.phase == InputActionPhase::Started &&
                       binding_state.held_seconds > binding.hold_seconds) {
                result.canceled = true;
                binding_state.phase = InputActionPhase::Canceled;
            } else if (was_down && !is_down) {
                result.released = true;
                if (binding_state.phase == InputActionPhase::Started && elapsed <= binding.hold_seconds) {
                    result.performed = true;
                    binding_state.phase = InputActionPhase::Performed;
                } else {
                    binding_state.phase = InputActionPhase::Canceled;
                }
                binding_state.held_seconds = 0.0f;
            } else if (!is_down) {
                binding_state.phase = InputActionPhase::Waiting;
            }
            break;
        case InputInteraction::MultiTap:
            if (!was_down && is_down) {
                binding_state.held_seconds = 0.0f;
                binding_state.tap_timer = 0.0f;
                binding_state.taps_in_window++;
                if (binding_state.taps_in_window >= binding.tap_count) {
                    binding_state.taps_in_window = 0;
                    result.started = true;
                    result.performed = true;
                    binding_state.phase = InputActionPhase::Performed;
                } else {
                    binding_state.phase = InputActionPhase::Started;
                }
            } else if (was_down && !is_down) {
                result.released = true;
                binding_state.held_seconds = 0.0f;
            } else if (!is_down) {
                binding_state.tap_timer += delta_time;
                if (binding_state.taps_in_window > 0 && binding_state.tap_timer > binding.tap_window_seconds) {
                    binding_state.taps_in_window = 0;
                    binding_state.phase = InputActionPhase::Waiting;
                }
            }
            break;
        case InputInteraction::Chord:
            if (!was_down && is_down) {
                result.started = true;
                binding_state.phase = InputActionPhase::Started;
            }
            if (is_down && binding_state.phase == InputActionPhase::Started && chord_ready) {
                result.performed = true;
                binding_state.phase = InputActionPhase::Performed;
            } else if (was_down && !is_down) {
                result.canceled = true;
                result.released = true;
                binding_state.held_seconds = 0.0f;
                binding_state.phase = InputActionPhase::Canceled;
            } else if (!is_down) {
                binding_state.phase = InputActionPhase::Waiting;
            }
            break;
        case InputInteraction::Toggle:
            if (!was_down && is_down) {
                binding_state.toggle_on = !binding_state.toggle_on;
                result.effective_down = binding_state.toggle_on;
                if (binding_state.toggle_on) {
                    result.started = true;
                    result.performed = true;
                    binding_state.phase = InputActionPhase::Performed;
                } else {
                    result.canceled = true;
                    binding_state.phase = InputActionPhase::Canceled;
                }
            }
            break;
        case InputInteraction::Repeat:
            if (!was_down && is_down) {
                result.started = true;
                result.performed = true;
                binding_state.phase = InputActionPhase::Performed;
                binding_state.repeat_timer = 0.0f;
            } else if (is_down) {
                binding_state.repeat_timer += delta_time;
                if (binding_state.phase != InputActionPhase::Performed) {
                    if (binding_state.repeat_timer >= binding.repeat_delay) {
                        result.performed = true;
                        binding_state.phase = InputActionPhase::Performed;
                    }
                } else if (binding_state.repeat_timer >= binding.repeat_delay + binding.repeat_rate) {
                    result.performed = true;
                    binding_state.repeat_timer = binding.repeat_delay;
                }
            } else if (was_down && !is_down) {
                result.canceled = true;
                result.released = true;
                binding_state.held_seconds = 0.0f;
                binding_state.repeat_timer = 0.0f;
                binding_state.phase = InputActionPhase::Canceled;
            } else if (!is_down) {
                binding_state.phase = InputActionPhase::Waiting;
            }
            break;
        }
        return result;
    }

    void InputManager::cancelMapActions_(ActionMap& map) {
        for (auto& [action_name, action] : map.actions) {
            (void)action_name;
            action.state.is_down = false;
            action.state.released_this_frame = true;
            const InputActionPhase old_phase = action.state.phase;
            if (old_phase == InputActionPhase::Started || old_phase == InputActionPhase::Performed) {
                action.state.phase = InputActionPhase::Canceled;
                emit_(map, action, InputActionPhase::Canceled, action.state.value);
            } else {
                action.state.phase = InputActionPhase::Waiting;
            }
            for (auto& binding_state : action.binding_states) {
                binding_state.is_down = false;
                binding_state.held_seconds = 0.0f;
                binding_state.phase = InputActionPhase::Waiting;
                binding_state.taps_in_window = 0;
                binding_state.tap_timer = 0.0f;
                binding_state.toggle_on = false;
                binding_state.repeat_timer = 0.0f;
            }
        }
    }

    Bool InputManager::loadActionAsset(const String& absolute_path) {
        std::ifstream file(absolute_path.c_str());
        if (!file.is_open()) return false;

        Json json;
        try {
            std::stringstream buffer;
            buffer << file.rdbuf();
            json = Json::parse(buffer.str());
        } catch (const Json::exception&) {
            return false;
        }
        if (!json.contains("maps") || !json["maps"].is_array()) return false;

        auto read_type = [](const Json& value) {
            if (value.is_number_integer()) return static_cast<InputActionValueType>(value.get<int>());
            const String name = value.get<String>();
            if (name == "Axis1D") return InputActionValueType::Axis1D;
            if (name == "Axis2D") return InputActionValueType::Axis2D;
            return InputActionValueType::Button;
        };

        struct LoadedMap {
            String name{};
            Int priority{0};
            Bool enabled{true};
        };
        DynamicArray<LoadedMap> loaded_maps;

        for (const auto& map_json : json["maps"]) {
            if (!map_json.contains("name")) continue;
            const String map_name = map_json["name"].get<String>();
            const Bool enabled = map_json.value("enabled", true);
            if (action_maps_.contains(map_name)) unregisterActionMap(map_name);
            registerActionMap(map_name, map_json.value("priority", 0));
            setActionMapEnabled(map_name, enabled);
            setActionMapConsume(map_name, map_json.value("consume", false));
            loaded_maps.push_back({map_name, map_json.value("priority", 0), enabled});
            if (!map_json.contains("actions") || !map_json["actions"].is_array()) continue;
            for (const auto& action_json : map_json["actions"]) {
                if (!action_json.contains("name")) continue;
                const String action_name = action_json["name"].get<String>();
                const auto type = action_json.contains("type") ? read_type(action_json["type"])
                                                                 : InputActionValueType::Button;
                if (!registerAction(map_name, action_name, type)) continue;
                auto* action = findAction_(map_name, action_name);
                if (!action) continue;
                if (!action_json.contains("bindings") || !action_json["bindings"].is_array()) continue;
                for (const auto& binding_json : action_json["bindings"]) {
                    InputBinding binding = ParseInputBinding(binding_json);
                    if (!isTypeCompatible_(type, binding.type)) continue;
                    action->bindings.push_back(binding);
                    action->binding_states.emplace_back();
                }
            }
        }

        std::stable_sort(loaded_maps.begin(), loaded_maps.end(),
                         [](const LoadedMap& lhs, const LoadedMap& rhs) { return lhs.priority < rhs.priority; });
        for (const auto& loaded_map : loaded_maps) {
            if (loaded_map.enabled) pushInputContext(loaded_map.name);
        }
        applyBindingOverrides();
        return true;
    }

    void InputManager::emit_(const ActionMap& map, const ActionDefinition& action,
                             InputActionPhase phase, const InputActionValue& value) {
        const String qualified_name = map.name + "/" + action.name;
        DynamicArray<InputActionListener> callbacks;
        for (const auto& subscription : subscriptions_) {
            if (subscription.phase != phase ||
                (subscription.action_name != action.name && subscription.action_name != qualified_name)) continue;
            callbacks.push_back(subscription.listener);
        }
        const InputActionEvent event{action.id, map.name, action.name, phase, value};
        for (auto& callback : callbacks) callback(event);
    }

} // namespace dodoe
