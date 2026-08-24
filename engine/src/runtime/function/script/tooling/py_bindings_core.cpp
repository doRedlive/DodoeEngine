// do@Redlive

#ifdef DODOE_PYTHON_ENABLED

#include "py_bindings.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/input/input.h"
#include "runtime/function/input/input_serialization.h"
#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

#include <pybind11/operators.h>

namespace dodoe::py_bindings {
namespace {

    TimeSystem* GetTimeSystem() {
        return dodoe::GetTimeSystem();
    }

#define DO_PY_VEC2(T, S) \
    py::class_<T>(m, #T) \
        .def(py::init<>()) \
        .def(py::init<S>()) \
        .def(py::init<S, S>()) \
        .def_property("x", [](const T& v) { return v.x; }, [](T& v, S val) { v.x = val; }) \
        .def_property("y", [](const T& v) { return v.y; }, [](T& v, S val) { v.y = val; }) \
        .def(py::self + py::self) \
        .def(py::self - py::self) \
        .def(py::self * S()) \
        .def(S() * py::self) \
        .def(py::self / S()) \
        .def("__repr__", [](const T& v) { \
            return #T "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; \
        })

#define DO_PY_VEC3(T, S) \
    py::class_<T>(m, #T) \
        .def(py::init<>()) \
        .def(py::init<S>()) \
        .def(py::init<S, S, S>()) \
        .def_property("x", [](const T& v) { return v.x; }, [](T& v, S val) { v.x = val; }) \
        .def_property("y", [](const T& v) { return v.y; }, [](T& v, S val) { v.y = val; }) \
        .def_property("z", [](const T& v) { return v.z; }, [](T& v, S val) { v.z = val; }) \
        .def(py::self + py::self) \
        .def(py::self - py::self) \
        .def(py::self * S()) \
        .def(S() * py::self) \
        .def(py::self / S()) \
        .def("__repr__", [](const T& v) { \
            return #T "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")"; \
        })

#define DO_PY_VEC4(T, S) \
    py::class_<T>(m, #T) \
        .def(py::init<>()) \
        .def(py::init<S>()) \
        .def(py::init<S, S, S, S>()) \
        .def_property("x", [](const T& v) { return v.x; }, [](T& v, S val) { v.x = val; }) \
        .def_property("y", [](const T& v) { return v.y; }, [](T& v, S val) { v.y = val; }) \
        .def_property("z", [](const T& v) { return v.z; }, [](T& v, S val) { v.z = val; }) \
        .def_property("w", [](const T& v) { return v.w; }, [](T& v, S val) { v.w = val; }) \
        .def(py::self + py::self) \
        .def(py::self - py::self) \
        .def(py::self * S()) \
        .def(S() * py::self) \
        .def(py::self / S()) \
        .def("__repr__", [](const T& v) { \
            return #T "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " \
                + std::to_string(v.z) + ", " + std::to_string(v.w) + ")"; \
        })

#define DO_PY_TIME_GET(NAME, RET, METHOD, DEF) \
    time_module.def(#NAME, []() -> RET { \
        auto* ts = GetTimeSystem(); \
        return ts ? ts->METHOD() : DEF; \
    })
#define DO_PY_TIME_SET(NAME, TYPE, METHOD) \
    time_module.def(#NAME, [](TYPE v) { \
        auto* ts = GetTimeSystem(); \
        if (ts) ts->METHOD(v); \
    })

} // anonymous namespace

void RegisterCore(py::module_& m) {
    m.def("log_info", [](const String& msg) { LOG_INFO("Py: {}", msg); });
    m.def("log_warn", [](const String& msg) { LOG_WARN("Py: {}", msg); });
    m.def("log_error", [](const String& msg) { LOG_ERROR("Py: {}", msg); });

    py::module_ time_module = m.def_submodule("Time");
    DO_PY_TIME_GET(get_delta_time, Float, getDeltaTime, 0.0f);
    DO_PY_TIME_GET(get_current_time, Float, current_time, 0.0f);
    DO_PY_TIME_GET(get_unscaled_delta_time, Float, get_unscaled_delta_time, 0.0f);
    DO_PY_TIME_GET(get_fps, Int, get_fps, 0);
    DO_PY_TIME_GET(get_time_scale, Float, get_time_scale, 1.0f);
    DO_PY_TIME_SET(set_time_scale, Float, set_time_scale);
    DO_PY_TIME_GET(get_target_fps, Int, get_target_fps, -1);
    DO_PY_TIME_SET(set_target_fps, Int, set_target_fps);

#undef DO_PY_TIME_GET
#undef DO_PY_TIME_SET

    DO_PY_VEC2(Vector2f, float);
    DO_PY_VEC2(Vector2i, int);
    DO_PY_VEC3(Vector3f, float);
    DO_PY_VEC3(Vector3i, int);
    DO_PY_VEC4(Vector4f, float);
    DO_PY_VEC4(Vector4i, int);

#undef DO_PY_VEC2
#undef DO_PY_VEC3
#undef DO_PY_VEC4

    py::class_<Matrix3f>(m, "Matrix3f")
        .def(py::init<>())
        .def(py::init<float>());
    py::class_<Matrix4f>(m, "Matrix4f")
        .def(py::init<>())
        .def(py::init<float>());

    py::enum_<KeyCode>(m, "KeyCode")
        .value("A", KeyCode::A).value("B", KeyCode::B).value("C", KeyCode::C).value("D", KeyCode::D)
        .value("E", KeyCode::E).value("F", KeyCode::F).value("G", KeyCode::G).value("H", KeyCode::H)
        .value("I", KeyCode::I).value("J", KeyCode::J).value("K", KeyCode::K).value("L", KeyCode::L)
        .value("M", KeyCode::M).value("N", KeyCode::N).value("O", KeyCode::O).value("P", KeyCode::P)
        .value("Q", KeyCode::Q).value("R", KeyCode::R).value("S", KeyCode::S).value("T", KeyCode::T)
        .value("U", KeyCode::U).value("V", KeyCode::V).value("W", KeyCode::W).value("X", KeyCode::X)
        .value("Y", KeyCode::Y).value("Z", KeyCode::Z)
        .value("D0", KeyCode::D0).value("D1", KeyCode::D1).value("D2", KeyCode::D2).value("D3", KeyCode::D3)
        .value("D4", KeyCode::D4).value("D5", KeyCode::D5).value("D6", KeyCode::D6).value("D7", KeyCode::D7)
        .value("D8", KeyCode::D8).value("D9", KeyCode::D9)
        .value("Enter", KeyCode::Enter).value("Escape", KeyCode::Escape).value("Backspace", KeyCode::Backspace)
        .value("Tab", KeyCode::Tab).value("Space", KeyCode::Space)
        .value("Minus", KeyCode::Minus).value("Equal", KeyCode::Equal)
        .value("LeftBracket", KeyCode::LeftBracket).value("RightBracket", KeyCode::RightBracket)
        .value("Backslash", KeyCode::Backslash).value("Semicolon", KeyCode::Semicolon)
        .value("Apostrophe", KeyCode::Apostrophe).value("GraveAccent", KeyCode::GraveAccent)
        .value("Comma", KeyCode::Comma).value("Period", KeyCode::Period).value("Slash", KeyCode::Slash)
        .value("CapsLock", KeyCode::CapsLock)
        .value("F1", KeyCode::F1).value("F2", KeyCode::F2).value("F3", KeyCode::F3).value("F4", KeyCode::F4)
        .value("F5", KeyCode::F5).value("F6", KeyCode::F6).value("F7", KeyCode::F7).value("F8", KeyCode::F8)
        .value("F9", KeyCode::F9).value("F10", KeyCode::F10).value("F11", KeyCode::F11).value("F12", KeyCode::F12)
        .value("PrintScreen", KeyCode::PrintScreen).value("ScrollLock", KeyCode::ScrollLock)
        .value("Pause", KeyCode::Pause).value("Insert", KeyCode::Insert).value("Home", KeyCode::Home)
        .value("PageUp", KeyCode::PageUp).value("Delete", KeyCode::Delete).value("End", KeyCode::End)
        .value("PageDown", KeyCode::PageDown)
        .value("Right", KeyCode::Right).value("Left", KeyCode::Left).value("Down", KeyCode::Down).value("Up", KeyCode::Up)
        .value("LeftControl", KeyCode::LeftControl).value("LeftShift", KeyCode::LeftShift).value("LeftAlt", KeyCode::LeftAlt)
        .value("RightControl", KeyCode::RightControl).value("RightShift", KeyCode::RightShift).value("RightAlt", KeyCode::RightAlt)
        .export_values();

    py::enum_<MouseCode>(m, "MouseCode")
        .value("None", MouseCode::None)
        .value("Button0", MouseCode::Button0).value("Button1", MouseCode::Button1)
        .value("Button2", MouseCode::Button2).value("Button3", MouseCode::Button3)
        .value("Button4", MouseCode::Button4).value("Button5", MouseCode::Button5)
        .value("Button6", MouseCode::Button6).value("Button7", MouseCode::Button7)
        .value("ButtonLeft", MouseCode::ButtonLeft)
        .value("ButtonMiddle", MouseCode::ButtonMiddle)
        .value("ButtonRight", MouseCode::ButtonRight)
        .export_values();

    py::enum_<InputActionValueType>(m, "InputActionValueType")
        .value("Button", InputActionValueType::Button)
        .value("Axis1D", InputActionValueType::Axis1D)
        .value("Axis2D", InputActionValueType::Axis2D)
        .export_values();

    py::enum_<InputActionPhase>(m, "InputActionPhase")
        .value("Waiting", InputActionPhase::Waiting)
        .value("Started", InputActionPhase::Started)
        .value("Performed", InputActionPhase::Performed)
        .value("Canceled", InputActionPhase::Canceled)
        .export_values();

    py::enum_<InputInteraction>(m, "InputInteraction")
        .value("Press", InputInteraction::Press)
        .value("Hold", InputInteraction::Hold)
        .value("Tap", InputInteraction::Tap)
        .value("MultiTap", InputInteraction::MultiTap)
        .value("Chord", InputInteraction::Chord)
        .value("Toggle", InputInteraction::Toggle)
        .value("Repeat", InputInteraction::Repeat)
        .export_values();

    py::enum_<GamepadButtonCode>(m, "GamepadButtonCode")
        .value("A", GamepadButtonCode::A).value("B", GamepadButtonCode::B)
        .value("X", GamepadButtonCode::X).value("Y", GamepadButtonCode::Y)
        .value("LB", GamepadButtonCode::LB).value("RB", GamepadButtonCode::RB)
        .value("Back", GamepadButtonCode::Back).value("Start", GamepadButtonCode::Start)
        .value("Guide", GamepadButtonCode::Guide)
        .value("LeftStick", GamepadButtonCode::LeftStick)
        .value("RightStick", GamepadButtonCode::RightStick)
        .value("DpadUp", GamepadButtonCode::DpadUp).value("DpadRight", GamepadButtonCode::DpadRight)
        .value("DpadDown", GamepadButtonCode::DpadDown).value("DpadLeft", GamepadButtonCode::DpadLeft)
        .export_values();

    py::enum_<GamepadAxisCode>(m, "GamepadAxisCode")
        .value("LeftX", GamepadAxisCode::LeftX).value("LeftY", GamepadAxisCode::LeftY)
        .value("RightX", GamepadAxisCode::RightX).value("RightY", GamepadAxisCode::RightY)
        .value("LeftTrigger", GamepadAxisCode::LeftTrigger)
        .value("RightTrigger", GamepadAxisCode::RightTrigger)
        .export_values();

    py::enum_<InputProcessorType>(m, "InputProcessorType")
        .value("DeadZone", InputProcessorType::DeadZone)
        .value("Normalize", InputProcessorType::Normalize)
        .value("Scale", InputProcessorType::Scale)
        .value("Invert", InputProcessorType::Invert)
        .value("Clamp", InputProcessorType::Clamp)
        .export_values();

    py::class_<Input>(m, "Input")
        .def_static("register_action_map", &Input::RegisterActionMap, py::arg("map_name"), py::arg("priority") = 0)
        .def_static("unregister_action_map", &Input::UnregisterActionMap)
        .def_static("set_action_map_enabled", &Input::SetActionMapEnabled)
        .def_static("set_action_map_consume", &Input::SetActionMapConsume)
        .def_static("push_input_context", &Input::PushInputContext)
        .def_static("pop_input_context", &Input::PopInputContext)
        .def_static("register_action", &Input::RegisterAction)
        .def_static("bind_key", &Input::BindKey, py::arg("map_name"), py::arg("action_name"), py::arg("key"), py::arg("scale") = 1.0f)
        .def_static("bind_key_2d", &Input::BindKey2D)
        .def_static("bind_mouse_button", &Input::BindMouseButton)
        .def_static("bind_mouse_delta", &Input::BindMouseDelta)
        .def_static("bind_mouse_wheel", &Input::BindMouseWheel)
        .def_static("bind_gamepad_button", &Input::BindGamepadButton,
                    py::arg("map_name"), py::arg("action_name"), py::arg("button"),
                    py::arg("device_id") = 0, py::arg("scale") = 1.0f)
        .def_static("bind_gamepad_axis", &Input::BindGamepadAxis,
                    py::arg("map_name"), py::arg("action_name"), py::arg("axis"),
                    py::arg("device_id") = 0, py::arg("scale") = 1.0f)
        .def_static("bind_gamepad_stick", &Input::BindGamepadStick,
                    py::arg("map_name"), py::arg("action_name"), py::arg("stick_axis"),
                    py::arg("device_id") = 0, py::arg("scale") = 1.0f)
        .def_static("bind_composite",
                    [](StringView map_name, StringView action_name, const std::string& parts_json, InputDeviceId device_id) {
                        DynamicArray<InputBinding> parts;
                        try {
                            const Json j = Json::parse(parts_json);
                            if (j.is_array()) {
                                for (const auto& part : j) parts.push_back(ParseInputBinding(part));
                            }
                        } catch (const Json::exception&) {
                        }
                        return Input::BindComposite(map_name, action_name, parts, device_id);
                    }, py::arg("map_name"), py::arg("action_name"), py::arg("parts_json"), py::arg("device_id") = 0)
        .def_static("set_binding_interaction", &Input::SetBindingInteraction,
                    py::arg("map_name"), py::arg("action_name"), py::arg("interaction"),
                    py::arg("hold_seconds") = 0.5f)
        .def_static("set_binding_tap_params", &Input::SetBindingTapParams,
                    py::arg("map_name"), py::arg("action_name"), py::arg("binding_index"),
                    py::arg("tap_count"), py::arg("tap_window_seconds"))
        .def_static("set_binding_repeat_params", &Input::SetBindingRepeatParams,
                    py::arg("map_name"), py::arg("action_name"), py::arg("binding_index"),
                    py::arg("repeat_delay"), py::arg("repeat_rate"))
        .def_static("set_binding_processor",
                    [](StringView map_name, StringView action_name, Size_t binding_index,
                       InputProcessorType type, Float a, Float b) {
                        InputProcessor processor;
                        processor.type = type;
                        processor.a = a;
                        processor.b = b;
                        return Input::SetBindingProcessor(map_name, action_name, binding_index, processor);
                    }, py::arg("map_name"), py::arg("action_name"), py::arg("binding_index"),
                    py::arg("type"), py::arg("a") = 0.0f, py::arg("b") = 1.0f)
        .def_static("load_action_asset", &Input::LoadActionAsset)
        .def_static("load_config_overrides",
                    [](const std::string& project_path, const std::string& user_path) {
                        return Input::LoadConfigOverrides(FsPath(project_path), FsPath(user_path));
                    })
        .def_static("save_user_config_overrides",
                    [](const std::string& user_path) {
                        return Input::SaveUserConfigOverrides(FsPath(user_path));
                    })
        .def_static("find_action_id", (InputActionId(*)(StringView, StringView)) &Input::FindActionId)
        .def_static("find_action_id_qualified", (InputActionId(*)(StringView)) &Input::FindActionId)
        .def_static("is_action_down", &Input::IsActionDown)
        .def_static("was_action_pressed", &Input::WasActionPressed)
        .def_static("was_action_released", &Input::WasActionReleased)
        .def_static("get_action_axis", &Input::GetActionAxis)
        .def_static("get_action_vector2", &Input::GetActionVector2)
        .def_static("is_action_down_by_id", (Bool(*)(InputActionId)) &Input::IsActionDown)
        .def_static("was_action_pressed_by_id", (Bool(*)(InputActionId)) &Input::WasActionPressed)
        .def_static("was_action_released_by_id", (Bool(*)(InputActionId)) &Input::WasActionReleased)
        .def_static("get_action_axis_by_id", (Float(*)(InputActionId)) &Input::GetActionAxis)
        .def_static("get_action_vector2_by_id", (Vector2f(*)(InputActionId)) &Input::GetActionVector2)
        .def_static("get_mouse_position", &Input::GetMousePosition)
        .def_static("get_mouse_delta", &Input::GetMouseDelta)
        .def_static("get_mouse_wheel", &Input::GetMouseWheel)
        .def_static("is_gamepad_connected", &Input::IsGamepadConnected)
        .def_static("is_gamepad_button_down", &Input::IsGamepadButtonDown)
        .def_static("is_gamepad_button_pressed", &Input::IsGamepadButtonPressed)
        .def_static("is_gamepad_button_released", &Input::IsGamepadButtonReleased)
        .def_static("get_gamepad_axis", &Input::GetGamepadAxis)
        .def_static("begin_rebind_session", &Input::BeginRebindSession)
        .def_static("cancel_rebind_session", &Input::CancelRebindSession)
        .def_static("is_rebind_session_active", &Input::IsRebindSessionActive)
        .def_static("set_binding_override",
                    [](StringView map_name, StringView action_name, Size_t binding_index, const std::string& binding_json) {
                        try {
                            return Input::SetBindingOverride(map_name, action_name, binding_index,
                                                            ParseInputBinding(Json::parse(binding_json)));
                        } catch (const Json::exception&) {
                            return false;
                        }
                    })
        .def_static("clear_binding_override", &Input::ClearBindingOverride);
}

} // namespace dodoe::py_bindings

#endif // DODOE_PYTHON_ENABLED
