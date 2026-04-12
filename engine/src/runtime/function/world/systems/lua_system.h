#ifndef DODOE_LUA_SYSTEM_H
#define DODOE_LUA_SYSTEM_H

#include "dopch.h"

#include "system.h"

#include "sol/sol.hpp"

namespace dodoe {

    class LuaSystem final : public System {
        sol::table table_{};
        sol::protected_function start_{};
        sol::protected_function update_{};
        sol::protected_function finalize_{};
    public:
        explicit LuaSystem(const sol::table& table) : table_(table) {
            start_ = getFun(table_, "doStart");
            update_ = getFun(table_, "doUpdate");
            finalize_ = getFun(table_, "doFinalize");
        }

        ~LuaSystem() override = default;

        void start(Registry& reg) override {
            callStart(reg);
        }

        void update(Registry& reg, const float dt) override {
            callUpdate(reg, dt);
        }

        void finalize(Registry& reg) override {
            callFinalize(reg);
        }

    private:
        static sol::protected_function getFun(const sol::table& t, const char* a) {
            if (sol::object obj = t[a]; obj.valid() && obj.get_type() == sol::type::function) {
                return obj.as<sol::protected_function>();
            }
            return {};
        }

        void callStart(Registry& reg) {
            if (!start_.valid()) {
                return;
            }

            sol::protected_function pf = start_;
            sol::protected_function_result result = pf(table_, std::ref(reg));
            if (result.valid()) {
                return;
            }

            sol::protected_function_result result2 = pf(std::ref(reg));
            if (!result2.valid()) {
                sol::error err = result2;
                DoError("LuaSystem Start failed: {}", err.what());
            }
        }

        void callUpdate(Registry& reg, const float dt) {
            if (!update_.valid()) {
                return;
            }

            sol::protected_function pf = update_;
            sol::protected_function_result result = pf(table_, std::ref(reg), dt);
            if (result.valid()) {
                return;
            }

            sol::protected_function_result result2 = pf(std::ref(reg), dt);
            if (!result2.valid()) {
                sol::error err = result2;
                DoError("LuaSystem Update failed: {}", err.what());
            }
        }

        void callFinalize(Registry& reg) {
            if (!finalize_.valid()) {
                return;
            }

            sol::protected_function pf = finalize_;
            sol::protected_function_result result = pf(table_, std::ref(reg));
            if (result.valid()) {
                return;
            }

            sol::protected_function_result result2 = pf(std::ref(reg));
            if (!result2.valid()) {
                sol::error err = result2;
                DoError("LuaSystem Finalize failed: {}", err.what());
            }
        }
    };

} // dodoe

#endif//DODOE_LUA_SYSTEM_H
