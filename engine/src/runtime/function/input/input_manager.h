// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/event/event.h"
#include "runtime/core/memory/managed.h"
#include "runtime/function/input/input_types.h"

namespace dodoe {

    class InputBackend;

    struct InputManagerInitInfo {
        void* native_window{nullptr};
        Bool host_mode{false};
    };

    class DODOE_API InputManager : public Managed<InputManager, InputManagerInitInfo> {
        friend class Input;
        friend class Managed<InputManager, InputManagerInitInfo>;

    public:
        void beginFrame();
        void update(Float delta_time);

        Bool registerActionMap(const String& map_name, Int priority = 0);
        Bool unregisterActionMap(StringView map_name);
        Bool setActionMapEnabled(StringView map_name, Bool enabled);
        Bool setActionMapConsume(StringView map_name, Bool consume);
        Bool pushInputContext(StringView map_name);
        Bool popInputContext(StringView map_name);
        Bool registerAction(StringView map_name, const String& action_name,
                            InputActionValueType value_type);
        Bool bindKey(StringView map_name, StringView action_name, KeyCode key, Float scale = 1.0f);
        Bool bindKey2D(StringView map_name, StringView action_name, KeyCode key, Vector2f scale);
        Bool bindMouseButton(StringView map_name, StringView action_name, MouseCode button, Float scale = 1.0f);
        Bool bindMouseDelta(StringView map_name, StringView action_name, Float scale = 1.0f);
        Bool bindMouseWheel(StringView map_name, StringView action_name, Float scale = 1.0f);
        Bool bindGamepadButton(StringView map_name, StringView action_name, GamepadButtonCode button,
                               InputDeviceId device_id = kInvalidInputDeviceId, Float scale = 1.0f);
        Bool bindGamepadAxis(StringView map_name, StringView action_name, GamepadAxisCode axis,
                             InputDeviceId device_id = kInvalidInputDeviceId, Float scale = 1.0f);
        Bool bindGamepadStick(StringView map_name, StringView action_name, GamepadAxisCode stick_axis,
                              InputDeviceId device_id = kInvalidInputDeviceId, Float scale = 1.0f);
        Bool bindComposite(StringView map_name, StringView action_name,
                           const DynamicArray<InputBinding>& parts,
                           InputDeviceId device_id = kInvalidInputDeviceId);
        Bool setBindingInteraction(StringView map_name, StringView action_name,
                                   InputInteraction interaction, Float hold_seconds = 0.5f);
        Bool setBindingTapParams(StringView map_name, StringView action_name, Size_t binding_index,
                                 Int tap_count, Float tap_window_seconds);
        Bool setBindingRepeatParams(StringView map_name, StringView action_name, Size_t binding_index,
                                    Float repeat_delay, Float repeat_rate);
        Bool setBindingProcessor(StringView map_name, StringView action_name, Size_t binding_index,
                                 const InputProcessor& processor);
        Bool loadActionAsset(const String& absolute_path);

        [[nodiscard]] InputActionId findActionId(StringView map_name, StringView action_name) const;
        [[nodiscard]] InputActionId findActionId(StringView qualified_name) const;

        [[nodiscard]] const Vector2f& getMousePosition() const { return m_raw_state_.mouse_position; }
        [[nodiscard]] const Vector2f& getMouseDelta() const { return m_raw_state_.mouse_delta; }
        [[nodiscard]] const Vector2f& getMouseWheel() const { return m_raw_state_.mouse_wheel; }

        [[nodiscard]] Bool isGamepadConnected(InputDeviceId device_id) const;
        [[nodiscard]] Bool isGamepadButtonDown(InputDeviceId device_id, GamepadButtonCode button) const;
        [[nodiscard]] Bool isGamepadButtonPressed(InputDeviceId device_id, GamepadButtonCode button) const;
        [[nodiscard]] Bool isGamepadButtonReleased(InputDeviceId device_id, GamepadButtonCode button) const;
        [[nodiscard]] Float getGamepadAxis(InputDeviceId device_id, GamepadAxisCode axis) const;

        Bool isActionDown(StringView action_name) const;
        Bool wasActionPressed(StringView action_name) const;
        Bool wasActionReleased(StringView action_name) const;
        Float getActionAxis(StringView action_name) const;
        Vector2f getActionVector2(StringView action_name) const;

        Bool isActionDown(InputActionId action_id) const;
        Bool wasActionPressed(InputActionId action_id) const;
        Bool wasActionReleased(InputActionId action_id) const;
        Float getActionAxis(InputActionId action_id) const;
        Vector2f getActionVector2(InputActionId action_id) const;

        InputSubscriptionId subscribe(StringView action_name, InputActionPhase phase,
                                      InputActionListener listener);
        InputSubscriptionId subscribe(InputActionId action_id, InputActionPhase phase,
                                      InputActionListener listener);
        void unsubscribe(InputSubscriptionId subscription_id);

        Bool setBindingOverride(StringView map_name, StringView action_name, Size_t binding_index,
                                const InputBinding& binding);
        Bool clearBindingOverride(StringView map_name, StringView action_name, Size_t binding_index);
        Bool getBinding(StringView map_name, StringView action_name, Size_t binding_index,
                        InputBinding& out) const;
        void applyBindingOverrides();
        Bool loadConfigOverrides(const FsPath& project_path, const FsPath& user_path);
        Bool saveUserConfigOverrides(const FsPath& user_path) const;

        Bool beginRebindSession(StringView map_name, StringView action_name, Size_t binding_index);
        void cancelRebindSession();
        [[nodiscard]] Bool isRebindSessionActive() const { return m_rebind_session_.active; }

    private:
        struct BindingState {
            Bool is_down{false};
            Float held_seconds{0.0f};
            InputActionPhase phase{InputActionPhase::Waiting};
            Int taps_in_window{0};
            Float tap_timer{0.0f};
            Bool toggle_on{false};
            Float repeat_timer{0.0f};
        };

        struct ActionDefinition {
            InputActionId id{kInvalidInputActionId};
            String name{};
            String map_name{};
            InputActionValueType value_type{InputActionValueType::Button};
            DynamicArray<InputBinding> bindings{};
            DynamicArray<BindingState> binding_states{};
            InputActionState state{};
        };

        struct ActionMap {
            String name{};
            Int priority{0};
            Bool enabled{true};
            Bool consume_input{false};
            std::unordered_map<String, ActionDefinition> actions{};
        };

        struct ControlSnapshot {
            Bool down{false};
            Bool pressed{false};
            Bool released{false};
            Float value{0.0f};
        };

        struct BindingResult {
            Bool effective_down{false};
            Bool started{false};
            Bool performed{false};
            Bool canceled{false};
            Bool released{false};
        };

        struct Subscription {
            InputSubscriptionId id{0};
            String action_name{};
            InputActionPhase phase{InputActionPhase::Waiting};
            InputActionListener listener{};
        };

        struct RebindSession {
            Bool active{false};
            String map_name{};
            String action_name{};
            Size_t binding_index{0};
        };

        [[nodiscard]] Bool initialize(const InputManagerInitInfo& init_info);
        void shutdown();

        InputRawState m_raw_state_{};
        InputBackend* m_backend_{nullptr};
        Bool m_has_focus_{true};

        std::unordered_map<String, ActionMap> action_maps_{};
        std::unordered_map<InputActionId, ActionDefinition*> action_by_id_{};
        InputActionId next_action_id_{1};
        DynamicArray<String> context_stack_{};
        DynamicArray<Subscription> subscriptions_{};
        InputSubscriptionId next_subscription_id_{1};
        DynamicArray<InputBindingOverride> overrides_{};
        RebindSession m_rebind_session_{};

        void on_window_lost_focus_(const WindowLostFocusEvent& event);
        void updateRebindSession_();
        [[nodiscard]] Bool captureNextControl_(InputBinding& out);

        [[nodiscard]] const ActionDefinition* findAction_(StringView action_name) const;
        [[nodiscard]] ActionDefinition* findAction_(StringView map_name, StringView action_name);
        [[nodiscard]] const ActionDefinition* findAction_(StringView map_name, StringView action_name) const;
        [[nodiscard]] const ActionDefinition* findAction_(InputActionId action_id) const;
        [[nodiscard]] ControlSnapshot readControl_(const InputBinding& binding) const;
        [[nodiscard]] Vector2f readStick_(const InputBinding& binding) const;
        [[nodiscard]] Vector2f readVector2_(const InputBinding& binding, const ControlSnapshot& snapshot) const;
        [[nodiscard]] Float processValue_(const InputBinding& binding, Float value) const;
        [[nodiscard]] Vector2f processVector_(const InputBinding& binding, Vector2f value) const;
        [[nodiscard]] Bool isBindingConsumed_(const InputBinding& binding,
                                              const std::unordered_set<UInt32>& controls,
                                              Bool pointer) const;
        [[nodiscard]] Bool isTypeCompatible_(InputActionValueType action_type,
                                             InputBindingType binding_type) const;
        [[nodiscard]] BindingResult evaluateButtonBinding_(const InputBinding& binding,
                                                           const ControlSnapshot& snapshot,
                                                           Bool chord_ready,
                                                           Float delta_time,
                                                           BindingState& binding_state) const;
        void cancelMapActions_(ActionMap& map);
        void emit_(const ActionMap& map, const ActionDefinition& action, InputActionPhase phase,
                   const InputActionValue& value);
    };

} // namespace dodoe
