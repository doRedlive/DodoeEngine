// do@Redlive

#pragma once

#include "runtime/dopch.h"

#include "runtime/function/input/input_types.h"
#include "runtime/core/utils/json.h"

namespace dodoe {

    inline InputBindingType InputBindingTypeFromName(const String& name) {
        if (name == "mouse_button") return InputBindingType::MouseButton;
        if (name == "mouse_delta") return InputBindingType::MouseDelta;
        if (name == "mouse_wheel") return InputBindingType::MouseWheel;
        if (name == "gamepad_button") return InputBindingType::GamepadButton;
        if (name == "gamepad_axis") return InputBindingType::GamepadAxis;
        if (name == "composite") return InputBindingType::Composite;
        return InputBindingType::Key;
    }

    inline String InputBindingTypeName(InputBindingType type) {
        switch (type) {
        case InputBindingType::MouseButton: return "mouse_button";
        case InputBindingType::MouseDelta: return "mouse_delta";
        case InputBindingType::MouseWheel: return "mouse_wheel";
        case InputBindingType::GamepadButton: return "gamepad_button";
        case InputBindingType::GamepadAxis: return "gamepad_axis";
        case InputBindingType::Composite: return "composite";
        default: return "key";
        }
    }

    inline InputInteraction InputInteractionFromName(const String& name) {
        if (name == "hold") return InputInteraction::Hold;
        if (name == "tap") return InputInteraction::Tap;
        if (name == "multi_tap") return InputInteraction::MultiTap;
        if (name == "chord") return InputInteraction::Chord;
        if (name == "toggle") return InputInteraction::Toggle;
        if (name == "repeat") return InputInteraction::Repeat;
        return InputInteraction::Press;
    }

    inline String InputInteractionName(InputInteraction interaction) {
        switch (interaction) {
        case InputInteraction::Hold: return "hold";
        case InputInteraction::Tap: return "tap";
        case InputInteraction::MultiTap: return "multi_tap";
        case InputInteraction::Chord: return "chord";
        case InputInteraction::Toggle: return "toggle";
        case InputInteraction::Repeat: return "repeat";
        default: return "press";
        }
    }

    inline InputProcessorType InputProcessorTypeFromName(const String& name) {
        if (name == "normalize") return InputProcessorType::Normalize;
        if (name == "scale") return InputProcessorType::Scale;
        if (name == "invert") return InputProcessorType::Invert;
        if (name == "clamp") return InputProcessorType::Clamp;
        return InputProcessorType::DeadZone;
    }

    inline String InputProcessorTypeName(InputProcessorType type) {
        switch (type) {
        case InputProcessorType::Normalize: return "normalize";
        case InputProcessorType::Scale: return "scale";
        case InputProcessorType::Invert: return "invert";
        case InputProcessorType::Clamp: return "clamp";
        default: return "dead_zone";
        }
    }

    inline InputBinding ParseInputBinding(const Json& j) {
        InputBinding binding;
        binding.type = InputBindingTypeFromName(j.value("type", String("key")));
        binding.device_id = static_cast<InputDeviceId>(j.value("device_id", 0));
        binding.key = static_cast<KeyCode>(j.value("key", static_cast<int>(KeyCode::Space)));
        binding.mouse = static_cast<MouseCode>(j.value("button", static_cast<int>(MouseCode::None)));
        binding.gamepad_button = static_cast<GamepadButtonCode>(j.value("gamepad_button", 0));
        binding.gamepad_axis = static_cast<GamepadAxisCode>(j.value("gamepad_axis", 0));
        binding.scale = j.value("scale", 1.0f);
        binding.hold_seconds = j.value("hold_seconds", 0.5f);
        binding.dead_zone = j.value("dead_zone", 0.0f);
        binding.tap_count = j.value("tap_count", 2);
        binding.tap_window_seconds = j.value("tap_window", 0.4f);
        binding.repeat_delay = j.value("repeat_delay", 0.4f);
        binding.repeat_rate = j.value("repeat_rate", 0.1f);
        binding.interaction = InputInteractionFromName(j.value("interaction", String("press")));

        if (j.contains("vector_scale") && j["vector_scale"].is_array() && j["vector_scale"].size() >= 2) {
            binding.vector_scale = Vector2f(j["vector_scale"][0].get<Float>(), j["vector_scale"][1].get<Float>());
        }
        if (j.contains("processors") && j["processors"].is_array()) {
            for (const auto& processor_json : j["processors"]) {
                InputProcessor processor;
                processor.type = InputProcessorTypeFromName(processor_json.value("type", String("dead_zone")));
                processor.a = processor_json.value("a", 0.0f);
                processor.b = processor_json.value("b", 1.0f);
                binding.processors.push_back(processor);
            }
        }
        if (binding.type == InputBindingType::Composite && j.contains("composite") && j["composite"].is_array()) {
            for (const auto& part_json : j["composite"]) {
                binding.composite_parts.push_back(ParseInputBinding(part_json));
            }
        }
        return binding;
    }

    inline Json InputBindingToJson(const InputBinding& binding) {
        Json j;
        j["type"] = InputBindingTypeName(binding.type);
        if (binding.device_id != kInvalidInputDeviceId) j["device_id"] = binding.device_id;
        if (binding.type == InputBindingType::Key) j["key"] = static_cast<int>(binding.key);
        else if (binding.type == InputBindingType::MouseButton) j["button"] = static_cast<int>(binding.mouse);
        else if (binding.type == InputBindingType::GamepadButton) j["gamepad_button"] = static_cast<int>(binding.gamepad_button);
        else if (binding.type == InputBindingType::GamepadAxis) j["gamepad_axis"] = static_cast<int>(binding.gamepad_axis);
        j["scale"] = binding.scale;
        j["interaction"] = InputInteractionName(binding.interaction);
        j["hold_seconds"] = binding.hold_seconds;
        if (binding.dead_zone != 0.0f) j["dead_zone"] = binding.dead_zone;
        if (binding.vector_scale != Vector2f(1.0f, 0.0f)) j["vector_scale"] = {binding.vector_scale.x, binding.vector_scale.y};
        if (binding.interaction == InputInteraction::MultiTap) {
            j["tap_count"] = binding.tap_count;
            j["tap_window"] = binding.tap_window_seconds;
        }
        if (binding.interaction == InputInteraction::Repeat) {
            j["repeat_delay"] = binding.repeat_delay;
            j["repeat_rate"] = binding.repeat_rate;
        }
        if (!binding.processors.empty()) {
            j["processors"] = Json::array();
            for (const auto& processor : binding.processors) {
                Json pj;
                pj["type"] = InputProcessorTypeName(processor.type);
                pj["a"] = processor.a;
                pj["b"] = processor.b;
                j["processors"].push_back(std::move(pj));
            }
        }
        if (binding.type == InputBindingType::Composite) {
            j["composite"] = Json::array();
            for (const auto& part : binding.composite_parts) {
                j["composite"].push_back(InputBindingToJson(part));
            }
        }
        return j;
    }

} // namespace dodoe
