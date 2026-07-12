// do@Redlive

#ifdef DODOE_PYTHON_ENABLED

#include "py_bindings.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/input/input.h"
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

    py::class_<Input>(m, "Input")
        .def_static("is_key_pressed", &Input::IsKeyPressed)
        .def_static("is_mouse_button_pressed", &Input::IsMouseButtonPressed)
        .def_static("get_mouse_position", &Input::GetMousePosition)
        .def_static("get_mouse_x", &Input::get_mouse_x)
        .def_static("get_mouse_y", &Input::get_mouse_y);
}

} // namespace dodoe::py_bindings

#endif // DODOE_PYTHON_ENABLED
