#include "lua_register_detail.h"

#include "runtime/core/base.h"

namespace dodoe::lua_register_detail {

    void register_math_types(sol::state& lua, sol::table& dodoe_table) {
        (void)dodoe_table;

        lua.new_usertype<Vector2f>("Vec2",
            sol::constructors<Vector2f(), Vector2f(float, float)>(),
            "x", &Vector2f::x,
            "y", &Vector2f::y
        );
        lua.new_usertype<Vector3f>("Vec3",
            sol::constructors<Vector3f(), Vector3f(float, float, float)>(),
            "x", &Vector3f::x,
            "y", &Vector3f::y,
            "z", &Vector3f::z
        );
    }

} // dodoe::lua_register_detail
