//
// Created by GreenMuffin on 2026/3/x.
//
#include "lua_register.h"
#include "lua_register_detail.h"

namespace dodoe {

    void LuaRegister::register_all(sol::state& lua) {
        sol::table dodoe_table = lua.create_table();
        lua["dodoe"] = dodoe_table;

        lua_register_detail::register_core(lua, dodoe_table);
        lua_register_detail::register_resource(lua, dodoe_table);
        lua_register_detail::register_entity(lua, dodoe_table);
        lua_register_detail::register_world(lua, dodoe_table);
    }

} // dodoe
