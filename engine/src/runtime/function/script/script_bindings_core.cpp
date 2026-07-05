// do@Redlive

#ifdef DODOE_PYTHON_ENABLED

#include "script_bindings.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/input/input.h"
#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

#include <pybind11/operators.h>

namespace dodoe::script_bindings {
namespace {

    TimeSystem* GetTimeSystem() {
        return dodoe::GetTimeSystem();
    }

    void BindVec2f(py::module_& m) {
        py::class_<Vector2f>(m, "Vector2f")
            .def(py::init<>())
            .def(py::init<float>())
            .def(py::init<float, float>())
            .def_property("x",
                [](const Vector2f& v) { return v.x; },
                [](Vector2f& v, float val) { v.x = val; })
            .def_property("y",
                [](const Vector2f& v) { return v.y; },
                [](Vector2f& v, float val) { v.y = val; })
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * float())
            .def(float() * py::self)
            .def(py::self / float())
            .def("__repr__", [](const Vector2f& v) {
                return "Vector2f(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
            });
    }

    void BindVec2i(py::module_& m) {
        py::class_<Vector2i>(m, "Vector2i")
            .def(py::init<>())
            .def(py::init<int>())
            .def(py::init<int, int>())
            .def_property("x",
                [](const Vector2i& v) { return v.x; },
                [](Vector2i& v, int val) { v.x = val; })
            .def_property("y",
                [](const Vector2i& v) { return v.y; },
                [](Vector2i& v, int val) { v.y = val; })
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * int())
            .def(int() * py::self)
            .def(py::self / int())
            .def("__repr__", [](const Vector2i& v) {
                return "Vector2i(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
            });
    }

    void BindVec3f(py::module_& m) {
        py::class_<Vector3f>(m, "Vector3f")
            .def(py::init<>())
            .def(py::init<float>())
            .def(py::init<float, float, float>())
            .def_property("x",
                [](const Vector3f& v) { return v.x; },
                [](Vector3f& v, float val) { v.x = val; })
            .def_property("y",
                [](const Vector3f& v) { return v.y; },
                [](Vector3f& v, float val) { v.y = val; })
            .def_property("z",
                [](const Vector3f& v) { return v.z; },
                [](Vector3f& v, float val) { v.z = val; })
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * float())
            .def(float() * py::self)
            .def(py::self / float())
            .def("__repr__", [](const Vector3f& v) {
                return "Vector3f(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
            });
    }

    void BindVec3i(py::module_& m) {
        py::class_<Vector3i>(m, "Vector3i")
            .def(py::init<>())
            .def(py::init<int>())
            .def(py::init<int, int, int>())
            .def_property("x",
                [](const Vector3i& v) { return v.x; },
                [](Vector3i& v, int val) { v.x = val; })
            .def_property("y",
                [](const Vector3i& v) { return v.y; },
                [](Vector3i& v, int val) { v.y = val; })
            .def_property("z",
                [](const Vector3i& v) { return v.z; },
                [](Vector3i& v, int val) { v.z = val; })
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * int())
            .def(int() * py::self)
            .def(py::self / int())
            .def("__repr__", [](const Vector3i& v) {
                return "Vector3i(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
            });
    }

    void BindVec4f(py::module_& m) {
        py::class_<Vector4f>(m, "Vector4f")
            .def(py::init<>())
            .def(py::init<float>())
            .def(py::init<float, float, float, float>())
            .def_property("x",
                [](const Vector4f& v) { return v.x; },
                [](Vector4f& v, float val) { v.x = val; })
            .def_property("y",
                [](const Vector4f& v) { return v.y; },
                [](Vector4f& v, float val) { v.y = val; })
            .def_property("z",
                [](const Vector4f& v) { return v.z; },
                [](Vector4f& v, float val) { v.z = val; })
            .def_property("w",
                [](const Vector4f& v) { return v.w; },
                [](Vector4f& v, float val) { v.w = val; })
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * float())
            .def(float() * py::self)
            .def(py::self / float())
            .def("__repr__", [](const Vector4f& v) {
                return "Vector4f(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", "
                    + std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
            });
    }

    void BindVec4i(py::module_& m) {
        py::class_<Vector4i>(m, "Vector4i")
            .def(py::init<>())
            .def(py::init<int>())
            .def(py::init<int, int, int, int>())
            .def_property("x",
                [](const Vector4i& v) { return v.x; },
                [](Vector4i& v, int val) { v.x = val; })
            .def_property("y",
                [](const Vector4i& v) { return v.y; },
                [](Vector4i& v, int val) { v.y = val; })
            .def_property("z",
                [](const Vector4i& v) { return v.z; },
                [](Vector4i& v, int val) { v.z = val; })
            .def_property("w",
                [](const Vector4i& v) { return v.w; },
                [](Vector4i& v, int val) { v.w = val; })
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * int())
            .def(int() * py::self)
            .def(py::self / int())
            .def("__repr__", [](const Vector4i& v) {
                return "Vector4i(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", "
                    + std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
            });
    }

    void BindKeyCode(py::module_& m) {
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
    }

    void BindMouseCode(py::module_& m) {
        py::enum_<MouseCode>(m, "MouseCode")
            .value("None", MouseCode::None)
            .value("Button0", MouseCode::Button0)
            .value("Button1", MouseCode::Button1)
            .value("Button2", MouseCode::Button2)
            .value("Button3", MouseCode::Button3)
            .value("Button4", MouseCode::Button4)
            .value("Button5", MouseCode::Button5)
            .value("Button6", MouseCode::Button6)
            .value("Button7", MouseCode::Button7)
            .value("ButtonLeft", MouseCode::ButtonLeft)
            .value("ButtonMiddle", MouseCode::ButtonMiddle)
            .value("ButtonRight", MouseCode::ButtonRight)
            .export_values();
    }

} // anonymous namespace

void RegisterCore(py::module_& m) {
    // --- Logging ---
    m.def("log_info", [](const String& msg) {
        LOG_INFO("Py: {}", msg);
    });
    m.def("log_warn", [](const String& msg) {
        LOG_WARN("Py: {}", msg);
    });
    m.def("log_error", [](const String& msg) {
        LOG_ERROR("Py: {}", msg);
    });

    // --- FPS ---
    m.def("get_fps", []() -> Int {
        auto* ts = GetTimeSystem();
        return ts ? ts->get_fps() : 0;
    });

    // --- Time submodule ---
    py::module_ time_module = m.def_submodule("Time");
    time_module.def("get_delta_time", []() -> Float {
        auto* ts = GetTimeSystem();
        return ts ? ts->getDeltaTime() : 0.0f;
    });
    time_module.def("get_current_time", []() -> Float {
        auto* ts = GetTimeSystem();
        return ts ? ts->current_time() : 0.0f;
    });
    time_module.def("get_unscaled_delta_time", []() -> Float {
        auto* ts = GetTimeSystem();
        return ts ? ts->get_unscaled_delta_time() : 0.0f;
    });
    time_module.def("get_fps", []() -> Int {
        auto* ts = GetTimeSystem();
        return ts ? ts->get_fps() : 0;
    });
    time_module.def("get_time_scale", []() -> Float {
        auto* ts = GetTimeSystem();
        return ts ? ts->get_time_scale() : 1.0f;
    });
    time_module.def("set_time_scale", [](Float value) {
        auto* ts = GetTimeSystem();
        if (ts) { ts->set_time_scale(value); }
    });
    time_module.def("get_target_fps", []() -> Int {
        auto* ts = GetTimeSystem();
        return ts ? ts->get_target_fps() : -1;
    });
    time_module.def("set_target_fps", [](Int value) {
        auto* ts = GetTimeSystem();
        if (ts) { ts->set_target_fps(value); }
    });

    // --- Vectors ---
    BindVec2f(m);
    BindVec2i(m);
    BindVec3f(m);
    BindVec3i(m);
    BindVec4f(m);
    BindVec4i(m);

    // --- Matrix ---
    py::class_<Matrix3f>(m, "Matrix3f")
        .def(py::init<>())
        .def(py::init<float>());
    py::class_<Matrix4f>(m, "Matrix4f")
        .def(py::init<>())
        .def(py::init<float>());

    // --- KeyCode / MouseCode ---
    BindKeyCode(m);
    BindMouseCode(m);

    // --- Input ---
    py::class_<Input>(m, "Input")
        .def_static("is_key_pressed", &Input::IsKeyPressed)
        .def_static("is_mouse_button_pressed", &Input::IsMouseButtonPressed)
        .def_static("get_mouse_position", &Input::GetMousePosition)
        .def_static("get_mouse_x", &Input::get_mouse_x)
        .def_static("get_mouse_y", &Input::get_mouse_y);
}

} // dodoe::script_bindings

#endif // DODOE_PYTHON_ENABLED
