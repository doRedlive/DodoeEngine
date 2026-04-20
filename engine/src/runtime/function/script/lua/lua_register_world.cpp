#include "lua_register_detail.h"

#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/registry.h"
#include "runtime/function/world/entity.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/world/systems/lua_system.h"

namespace dodoe::lua_register_detail {

    void register_world(sol::state& lua, sol::table& dodoe_table) {
        dodoe_table.new_usertype<Registry>("Registry",
            sol::no_constructor,
            "createEntity", &Registry::create,
            "destroyEntity", [](Registry& reg, const Entity& entity) { reg.destroy(entity); },
            "validEntity", [](Registry& reg, const Entity& entity) { return reg.valid(entity); },
            "clear", &Registry::clear
        );

        dodoe_table.new_usertype<Scene>("Scene",
            sol::no_constructor,
            "getName", &Scene::getName,
            "setName", &Scene::setName,
            "createEntity", [](Scene& scene, const std::string& name) { return scene.createEntity(name); },
            "destroyEntity", &Scene::destroyEntity,
            "getRegistry", [](Scene& scene) { return &scene.registry(); }
        );

        dodoe_table.new_usertype<World>("World",
            sol::no_constructor,
            "getName", &World::getName,
            "createScene", &World::createScene,
            "deleteScene", &World::deleteScene,
            "getScene", &World::getScene,
            "getCurrentScene", &World::getCurrentScene,
            "loadScene", &World::loadScene,
            "registerSystem", [](World& self, const sol::table& system_table) {
                self.registerRuntimeSystem(create_ref<LuaSystem>(system_table));
            }
        );

        dodoe_table.set_function("getWorld", []() -> World* {
            return Application::self().context().world.get();
        });
    }

} // dodoe::lua_register_detail
