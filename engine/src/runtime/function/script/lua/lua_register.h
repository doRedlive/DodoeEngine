#ifndef DODOE_LUA_REGISTER_H
#define DODOE_LUA_REGISTER_H

#include "sol/sol.hpp"

namespace dodoe {

    class LuaRegister {
    public:
        static void register_all(sol::state& lua);
    };

} // dodoe

#endif//DODOE_LUA_REGISTER_H
