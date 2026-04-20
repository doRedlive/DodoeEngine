//
// Created by GreenMuffin on 2026/3/x.
//

#include "lua_register_detail.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/input/input.h"
#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

namespace dodoe::lua_register_detail {

    void register_core(sol::state& lua, sol::table& dodoe_table) {
        (void)lua;
        dodoe_table.set_function("logInfo", [](const std::string& msg) {
            LOG_INFO("Lua: {}", msg);
        });
        dodoe_table.set_function("logWarn", [](const std::string& msg) {
            LogWarn("Lua: {}", msg);
        });
        dodoe_table.set_function("logError", [](const std::string& msg) {
            LOG_ERROR("Lua: {}", msg);
        });

        dodoe_table.set_function("getFps", []() -> int {
            if (!Application::self().context().time_system) {
                return 0;
            }
            return Application::self().context().time_system->get_fps();
        });
        dodoe_table.new_usertype<TimeSystem>("TimeSystem",
            "deltaTime", &TimeSystem::delta_time,
            "getTimeScale", &TimeSystem::get_time_scale,
            "setTimeScale", &TimeSystem::set_time_scale,
            "getTargetFps", &TimeSystem::get_target_fps,
            "setTargetFps", &TimeSystem::set_target_fps,
            "getFps", &TimeSystem::get_fps,
            "getUnscaledDeltaTime", &TimeSystem::get_unscaled_delta_time
        );
        dodoe_table.set_function("getTimeSystem", []() -> TimeSystem* {
            if (!Application::self().context().time_system) {
                return nullptr;
            }
            return Application::self().context().time_system.get();
        });
        sol::table time_table = lua.create_table();
        dodoe_table["Time"] = time_table;
        time_table.set_function("getDeltaTime", []() -> float {
            if (!Application::self().context().time_system) {
                return 0.0f;
            }
            return Application::self().context().time_system->delta_time();
        });
        time_table.set_function("getCurrentTime", []() -> float {
            if (!Application::self().context().time_system) {
                return 0.0f;
            }
            return Application::self().context().time_system->current_time();
        });
        time_table.set_function("getUnscaledDeltaTime", []() -> float {
            if (!Application::self().context().time_system) {
                return 0.0f;
            }
            return Application::self().context().time_system->get_unscaled_delta_time();
        });
        time_table.set_function("getFps", []() -> int {
            if (!Application::self().context().time_system) {
                return 0;
            }
            return Application::self().context().time_system->get_fps();
        });
        time_table.set_function("getTimeScale", []() -> float {
            if (!Application::self().context().time_system) {
                return 1.0f;
            }
            return Application::self().context().time_system->get_time_scale();
        });
        time_table.set_function("setTimeScale", [](const float value) {
            if (!Application::self().context().time_system) {
                return;
            }
            Application::self().context().time_system->set_time_scale(value);
        });
        time_table.set_function("getTargetFps", []() -> int {
            if (!Application::self().context().time_system) {
                return -1;
            }
            return Application::self().context().time_system->get_target_fps();
        });
        time_table.set_function("setTargetFps", [](const int value) {
            if (!Application::self().context().time_system) {
                return;
            }
            Application::self().context().time_system->set_target_fps(value);
        });

        dodoe_table.new_usertype<Vector2f>("Vector2f",
            sol::constructors<Vector2f(), Vector2f(float), Vector2f(float, float)>(),
            "x", &Vector2f::x,
            "y", &Vector2f::y,
            sol::meta_function::addition, [](const Vector2f& a, const Vector2f& b) { return a + b; },
            sol::meta_function::subtraction, [](const Vector2f& a, const Vector2f& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const Vector2f& v, float s) { return v * s; },
                [](float s, const Vector2f& v) { return s * v; }
            ),
            sol::meta_function::division, [](const Vector2f& v, float s) { return v / s; }
        );
        dodoe_table.new_usertype<Vector2i>("Vector2i",
            sol::constructors<Vector2i(), Vector2i(int), Vector2i(int, int)>(),
            "x", &Vector2i::x,
            "y", &Vector2i::y,
            sol::meta_function::addition, [](const Vector2i& a, const Vector2i& b) { return a + b; },
            sol::meta_function::subtraction, [](const Vector2i& a, const Vector2i& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const Vector2i& v, int s) { return v * s; },
                [](int s, const Vector2i& v) { return s * v; }
            ),
            sol::meta_function::division, [](const Vector2i& v, int s) { return v / s; }
        );
        dodoe_table.new_usertype<Vector3f>("Vector3f",
            sol::constructors<Vector3f(), Vector3f(float), Vector3f(float, float, float)>(),
            "x", &Vector3f::x,
            "y", &Vector3f::y,
            "z", &Vector3f::z,
            sol::meta_function::addition, [](const Vector3f& a, const Vector3f& b) { return a + b; },
            sol::meta_function::subtraction, [](const Vector3f& a, const Vector3f& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const Vector3f& v, float s) { return v * s; },
                [](float s, const Vector3f& v) { return s * v; }
            ),
            sol::meta_function::division, [](const Vector3f& v, float s) { return v / s; }
        );
        dodoe_table.new_usertype<Vector3i>("Vector3i",
            sol::constructors<Vector3i(), Vector3i(int), Vector3i(int, int, int)>(),
            "x", &Vector3i::x,
            "y", &Vector3i::y,
            "z", &Vector3i::z,
            sol::meta_function::addition, [](const Vector3i& a, const Vector3i& b) { return a + b; },
            sol::meta_function::subtraction, [](const Vector3i& a, const Vector3i& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const Vector3i& v, int s) { return v * s; },
                [](int s, const Vector3i& v) { return s * v; }
            ),
            sol::meta_function::division, [](const Vector3i& v, int s) { return v / s; }
        );
        dodoe_table.new_usertype<Vector4f>("Vector4f",
            sol::constructors<Vector4f(), Vector4f(float), Vector4f(float, float, float, float)>(),
            "x", &Vector4f::x,
            "y", &Vector4f::y,
            "z", &Vector4f::z,
            "w", &Vector4f::w,
            sol::meta_function::addition, [](const Vector4f& a, const Vector4f& b) { return a + b; },
            sol::meta_function::subtraction, [](const Vector4f& a, const Vector4f& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const Vector4f& v, float s) { return v * s; },
                [](float s, const Vector4f& v) { return s * v; }
            ),
            sol::meta_function::division, [](const Vector4f& v, float s) { return v / s; }
        );
        dodoe_table.new_usertype<Vector4i>("Vector4i",
            sol::constructors<Vector4i(), Vector4i(int), Vector4i(int, int, int, int)>(),
            "x", &Vector4i::x,
            "y", &Vector4i::y,
            "z", &Vector4i::z,
            "w", &Vector4i::w,
            sol::meta_function::addition, [](const Vector4i& a, const Vector4i& b) { return a + b; },
            sol::meta_function::subtraction, [](const Vector4i& a, const Vector4i& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const Vector4i& v, int s) { return v * s; },
                [](int s, const Vector4i& v) { return s * v; }
            ),
            sol::meta_function::division, [](const Vector4i& v, int s) { return v / s; }
        );
        dodoe_table.new_usertype<Matrix3f>("Matrix3f",
            sol::constructors<Matrix3f(), Matrix3f(float)>()
        );
        dodoe_table.new_usertype<Matrix4f>("Matrix4f",
            sol::constructors<Matrix4f(), Matrix4f(float)>()
        );

        dodoe_table.new_enum<KeyCode>("KeyCode", {
            {"A", KeyCode::A}, {"B", KeyCode::B}, {"C", KeyCode::C}, {"D", KeyCode::D},
            {"E", KeyCode::E}, {"F", KeyCode::F}, {"G", KeyCode::G}, {"H", KeyCode::H},
            {"I", KeyCode::I}, {"J", KeyCode::J}, {"K", KeyCode::K}, {"L", KeyCode::L},
            {"M", KeyCode::M}, {"N", KeyCode::N}, {"O", KeyCode::O}, {"P", KeyCode::P},
            {"Q", KeyCode::Q}, {"R", KeyCode::R}, {"S", KeyCode::S}, {"T", KeyCode::T},
            {"U", KeyCode::U}, {"V", KeyCode::V}, {"W", KeyCode::W}, {"X", KeyCode::X},
            {"Y", KeyCode::Y}, {"Z", KeyCode::Z},
            {"D0", KeyCode::D0}, {"D1", KeyCode::D1}, {"D2", KeyCode::D2}, {"D3", KeyCode::D3},
            {"D4", KeyCode::D4}, {"D5", KeyCode::D5}, {"D6", KeyCode::D6}, {"D7", KeyCode::D7},
            {"D8", KeyCode::D8}, {"D9", KeyCode::D9},
            {"Enter", KeyCode::Enter}, {"Escape", KeyCode::Escape}, {"Backspace", KeyCode::Backspace},
            {"Tab", KeyCode::Tab}, {"Space", KeyCode::Space},
            {"Minus", KeyCode::Minus}, {"Equal", KeyCode::Equal},
            {"LeftBracket", KeyCode::LeftBracket}, {"RightBracket", KeyCode::RightBracket},
            {"Backslash", KeyCode::Backslash}, {"Semicolon", KeyCode::Semicolon},
            {"Apostrophe", KeyCode::Apostrophe}, {"GraveAccent", KeyCode::GraveAccent},
            {"Comma", KeyCode::Comma}, {"Period", KeyCode::Period}, {"Slash", KeyCode::Slash},
            {"CapsLock", KeyCode::CapsLock},
            {"F1", KeyCode::F1}, {"F2", KeyCode::F2}, {"F3", KeyCode::F3}, {"F4", KeyCode::F4},
            {"F5", KeyCode::F5}, {"F6", KeyCode::F6}, {"F7", KeyCode::F7}, {"F8", KeyCode::F8},
            {"F9", KeyCode::F9}, {"F10", KeyCode::F10}, {"F11", KeyCode::F11}, {"F12", KeyCode::F12},
            {"PrintScreen", KeyCode::PrintScreen}, {"ScrollLock", KeyCode::ScrollLock},
            {"Pause", KeyCode::Pause}, {"Insert", KeyCode::Insert}, {"Home", KeyCode::Home},
            {"PageUp", KeyCode::PageUp}, {"Delete", KeyCode::Delete}, {"End", KeyCode::End},
            {"PageDown", KeyCode::PageDown},
            {"Right", KeyCode::Right}, {"Left", KeyCode::Left}, {"Down", KeyCode::Down}, {"Up", KeyCode::Up},
            {"LeftControl", KeyCode::LeftControl}, {"LeftShift", KeyCode::LeftShift}, {"LeftAlt", KeyCode::LeftAlt},
            {"RightControl", KeyCode::RightControl}, {"RightShift", KeyCode::RightShift}, {"RightAlt", KeyCode::RightAlt}
        });
        dodoe_table.new_enum<MouseCode>("MouseCode", {
            {"None", MouseCode::None},
            {"Button0", MouseCode::Button0},
            {"Button1", MouseCode::Button1},
            {"Button2", MouseCode::Button2},
            {"Button3", MouseCode::Button3},
            {"Button4", MouseCode::Button4},
            {"Button5", MouseCode::Button5},
            {"Button6", MouseCode::Button6},
            {"Button7", MouseCode::Button7},
            {"ButtonLeft", MouseCode::ButtonLeft},
            {"ButtonMiddle", MouseCode::ButtonMiddle},
            {"ButtonRight", MouseCode::ButtonRight}
        });
        dodoe_table.new_usertype<Input>("Input",
            "isKeyPressed", &Input::is_key_pressed,
            "isMouseButtonPressed", &Input::is_mouse_button_pressed,
            "getMousePosition", &Input::get_mouse_position,
            "getMouseX", &Input::get_mouse_x,
            "getMouseY", &Input::get_mouse_y
        );

    }

} // dodoe::lua_register_detail
