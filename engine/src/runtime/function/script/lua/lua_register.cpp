#include "lua_register.h"
#include "lua_register_detail.h"

namespace dodoe {

    void LuaRegister::register_all(sol::state& lua) {
        sol::table dodoe_table = lua["dodoe"];
        if (!dodoe_table.valid()) {
            dodoe_table = lua.create_named_table("dodoe");
        }

        lua_register_detail::register_log_time(lua, dodoe_table);
        lua_register_detail::register_resource_manager(lua, dodoe_table);
        lua_register_detail::register_math_types(lua, dodoe_table);
        lua_register_detail::register_components_entity(lua, dodoe_table);
        lua_register_detail::register_world_system(lua, dodoe_table);
    }

} // dodoe
