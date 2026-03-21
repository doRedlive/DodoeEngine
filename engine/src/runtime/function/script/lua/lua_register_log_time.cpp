#include "lua_register_detail.h"

#include "runtime/core/application.h"
#include "runtime/core/system_context.h"
#include "runtime/function/time/time_system.h"

namespace dodoe::lua_register_detail {

    void register_log_time(sol::state& lua, sol::table& dodoe_table) {
        (void)lua;

        dodoe_table.set_function("logInfo", [](const std::string& msg) {
            DoInfo("Lua: {}", msg);
        });
        dodoe_table.set_function("logWarn", [](const std::string& msg) {
            DoWarn("Lua: {}", msg);
        });
        dodoe_table.set_function("logError", [](const std::string& msg) {
            DoError("Lua: {}", msg);
        });
        dodoe_table.set_function("deltaTime", []() -> float {
            if (!Application::self().context().time_system) {
                return 0.0f;
            }
            return Application::self().context().time_system->get_delta_time();
        });
        dodoe_table.set_function("getFps", []() -> int {
            if (!Application::self().context().time_system) {
                return 0;
            }
            return Application::self().context().time_system->get_fps();
        });
    }

} // dodoe::lua_register_detail
