#include "lua_register_detail.h"

#include "runtime/core/world/entity.h"
#include "runtime/core/world/components.h"

namespace {

    struct ComponentRegistry {
        std::function<sol::object(sol::state_view, dodoe::Entity&)> add_func;
        std::function<sol::object(sol::state_view, dodoe::Entity&)> get_func;
        std::function<bool(dodoe::Entity&)> has_func;
        std::function<void(dodoe::Entity&)> remove_func;
        std::function<bool(dodoe::Entity&, const sol::table&)> set_func;
    };

    std::unordered_map<std::string, ComponentRegistry> s_component_registry{};

    bool get_bool_field(const sol::table& data, const char* key, const bool default_value) {
        sol::object obj = data[key];
        if (!obj.valid()) {
            return default_value;
        }
        if (obj.get_type() == sol::type::boolean) {
            return obj.as<bool>();
        }
        return default_value;
    }

    float get_float_field(const sol::table& data, const char* key, const float default_value) {
        sol::object obj = data[key];
        if (!obj.valid()) {
            return default_value;
        }
        if (obj.get_type() == sol::type::number) {
            return obj.as<float>();
        }
        return default_value;
    }

    std::string get_string_field(const sol::table& data, const char* key, const std::string& default_value = "") {
        sol::object obj = data[key];
        if (!obj.valid()) {
            return default_value;
        }
        if (obj.get_type() == sol::type::string) {
            return obj.as<std::string>();
        }
        return default_value;
    }

    dodoe::Vector2f table_to_vec2(const sol::table& data, const dodoe::Vector2f default_value) {
        return dodoe::Vector2f{
            get_float_field(data, "x", default_value.x),
            get_float_field(data, "y", default_value.y)
        };
    }

    dodoe::Vector3f table_to_vec3(const sol::table& data, const dodoe::Vector3f default_value) {
        return dodoe::Vector3f{
            get_float_field(data, "x", default_value.x),
            get_float_field(data, "y", default_value.y),
            get_float_field(data, "z", default_value.z)
        };
    }

    sol::table vec2_to_table(sol::state_view lua, const dodoe::Vector2f& value) {
        auto t = lua.create_table();
        t["x"] = value.x;
        t["y"] = value.y;
        return t;
    }

    sol::table vec3_to_table(sol::state_view lua, const dodoe::Vector3f& value) {
        auto t = lua.create_table();
        t["x"] = value.x;
        t["y"] = value.y;
        t["z"] = value.z;
        return t;
    }

    std::optional<std::string> get_type_name(const sol::table& comp_type) {
        sol::optional<std::string> type_name = comp_type["__type_name"];
        if (!type_name) {
            return std::nullopt;
        }
        return *type_name;
    }

    const ComponentRegistry* find_component_registry(const std::string& type_name) {
        const auto it = s_component_registry.find(type_name);
        if (it == s_component_registry.end()) {
            return nullptr;
        }
        return &it->second;
    }

}

namespace dodoe::lua_register_detail {

    void register_components_entity(sol::state& lua, sol::table& dodoe_table) {
        dodoe_table.new_enum<Rigidbody2dComponent::BodyType>(
            "rigidbodyBodyType",
            {
                {"Static", Rigidbody2dComponent::BodyType::Static},
                {"Dynamic", Rigidbody2dComponent::BodyType::Dynamic},
                {"Kinematic", Rigidbody2dComponent::BodyType::Kinematic}
            }
        );

        lua.new_usertype<TransformComponent>("TransformComponent",
            sol::constructors<TransformComponent()>(),
            "position", &TransformComponent::position,
            "rotation", &TransformComponent::rotation,
            "scale", &TransformComponent::scale
        );
        lua.new_usertype<TagComponent>("TagComponent",
            sol::constructors<TagComponent(), TagComponent(const std::string&)>(),
            "tag", &TagComponent::tag
        );
        lua.new_usertype<ScriptComponent>("ScriptComponent",
            sol::constructors<ScriptComponent()>(),
            "className", sol::property(
                [](ScriptComponent& c) -> std::string& { return c.class_name; },
                [](ScriptComponent& c, const std::string& value) { c.class_name = value; })
        );
        lua.new_usertype<Rigidbody2dComponent>("Rigidbody2dComponent",
            sol::constructors<Rigidbody2dComponent()>(),
            "bodyType", sol::property(
                [](Rigidbody2dComponent& c) { return c.type; },
                [](Rigidbody2dComponent& c, const Rigidbody2dComponent::BodyType value) { c.type = value; }),
            "fixedRotation", &Rigidbody2dComponent::fixed_rotation
        );
        lua.new_usertype<BoxCollider2dComponent>("BoxCollider2dComponent",
            sol::constructors<BoxCollider2dComponent()>(),
            "offset", &BoxCollider2dComponent::offset,
            "size", &BoxCollider2dComponent::size,
            "density", &BoxCollider2dComponent::density,
            "friction", &BoxCollider2dComponent::friction,
            "restitution", &BoxCollider2dComponent::restitution,
            "restitutionThreshold", &BoxCollider2dComponent::restitution_threshold
        );
        dodoe_table["TagComponent"]["__type_name"] = "TagComponent";
        dodoe_table["TransformComponent"]["__type_name"] = "TransformComponent";
        dodoe_table["ScriptComponent"]["__type_name"] = "ScriptComponent";
        dodoe_table["Rigidbody2dComponent"]["__type_name"] = "Rigidbody2dComponent";
        dodoe_table["BoxCollider2dComponent"]["__type_name"] = "BoxCollider2dComponent";

        s_component_registry.clear();

        {
            ComponentRegistry registry{};
            registry.add_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (self.has_component<TagComponent>()) {
                    return sol::make_object(lua_state, std::ref(self.get_component<TagComponent>()));
                }
                return sol::make_object(lua_state, std::ref(self.add_component<TagComponent>()));
            };
            registry.get_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (!self.has_component<TagComponent>()) return sol::make_object(lua_state, sol::nil);
                return sol::make_object(lua_state, std::ref(self.get_component<TagComponent>()));
            };
            registry.has_func = [](Entity& self) -> bool { return self.has_component<TagComponent>(); };
            registry.remove_func = [](Entity& self) { if (self.has_component<TagComponent>()) self.remove_component<TagComponent>(); };
            registry.set_func = [](Entity& self, const sol::table& data) -> bool {
                auto& c = self.add_or_replace_component<TagComponent>();
                c.tag = get_string_field(data, "tag", c.tag);
                return true;
            };
            s_component_registry["TagComponent"] = std::move(registry);
        }
        {
            ComponentRegistry registry{};
            registry.add_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (self.has_component<TransformComponent>()) {
                    return sol::make_object(lua_state, std::ref(self.get_component<TransformComponent>()));
                }
                return sol::make_object(lua_state, std::ref(self.add_component<TransformComponent>()));
            };
            registry.get_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (!self.has_component<TransformComponent>()) return sol::make_object(lua_state, sol::nil);
                return sol::make_object(lua_state, std::ref(self.get_component<TransformComponent>()));
            };
            registry.has_func = [](Entity& self) -> bool { return self.has_component<TransformComponent>(); };
            registry.remove_func = [](Entity& self) { if (self.has_component<TransformComponent>()) self.remove_component<TransformComponent>(); };
            registry.set_func = [](Entity& self, const sol::table& data) -> bool {
                auto& c = self.add_or_replace_component<TransformComponent>();
                if (sol::object pos_obj = data["position"]; pos_obj.valid() && pos_obj.get_type() == sol::type::table) {
                    c.position = table_to_vec3(pos_obj.as<sol::table>(), c.position);
                }
                if (sol::object rot_obj = data["rotation"]; rot_obj.valid() && rot_obj.get_type() == sol::type::table) {
                    c.rotation = table_to_vec3(rot_obj.as<sol::table>(), c.rotation);
                }
                if (sol::object scale_obj = data["scale"]; scale_obj.valid() && scale_obj.get_type() == sol::type::table) {
                    c.scale = table_to_vec3(scale_obj.as<sol::table>(), c.scale);
                }
                return true;
            };
            s_component_registry["TransformComponent"] = std::move(registry);
        }
        {
            ComponentRegistry registry{};
            registry.add_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (self.has_component<ScriptComponent>()) {
                    return sol::make_object(lua_state, std::ref(self.get_component<ScriptComponent>()));
                }
                return sol::make_object(lua_state, std::ref(self.add_component<ScriptComponent>()));
            };
            registry.get_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (!self.has_component<ScriptComponent>()) return sol::make_object(lua_state, sol::nil);
                return sol::make_object(lua_state, std::ref(self.get_component<ScriptComponent>()));
            };
            registry.has_func = [](Entity& self) -> bool { return self.has_component<ScriptComponent>(); };
            registry.remove_func = [](Entity& self) { if (self.has_component<ScriptComponent>()) self.remove_component<ScriptComponent>(); };
            registry.set_func = [](Entity& self, const sol::table& data) -> bool {
                auto& c = self.add_or_replace_component<ScriptComponent>();
                c.class_name = get_string_field(data, "className", c.class_name);
                return true;
            };
            s_component_registry["ScriptComponent"] = std::move(registry);
        }
        {
            ComponentRegistry registry{};
            registry.add_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (self.has_component<Rigidbody2dComponent>()) {
                    return sol::make_object(lua_state, std::ref(self.get_component<Rigidbody2dComponent>()));
                }
                return sol::make_object(lua_state, std::ref(self.add_component<Rigidbody2dComponent>()));
            };
            registry.get_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (!self.has_component<Rigidbody2dComponent>()) return sol::make_object(lua_state, sol::nil);
                return sol::make_object(lua_state, std::ref(self.get_component<Rigidbody2dComponent>()));
            };
            registry.has_func = [](Entity& self) -> bool { return self.has_component<Rigidbody2dComponent>(); };
            registry.remove_func = [](Entity& self) { if (self.has_component<Rigidbody2dComponent>()) self.remove_component<Rigidbody2dComponent>(); };
            registry.set_func = [](Entity& self, const sol::table& data) -> bool {
                auto& c = self.add_or_replace_component<Rigidbody2dComponent>();
                if (sol::object body_type_obj = data["bodyType"]; body_type_obj.valid() && body_type_obj.get_type() == sol::type::number) {
                    c.type = static_cast<Rigidbody2dComponent::BodyType>(body_type_obj.as<int>());
                }
                c.fixed_rotation = get_bool_field(data, "fixedRotation", c.fixed_rotation);
                return true;
            };
            s_component_registry["Rigidbody2dComponent"] = std::move(registry);
        }
        {
            ComponentRegistry registry{};
            registry.add_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (self.has_component<BoxCollider2dComponent>()) {
                    return sol::make_object(lua_state, std::ref(self.get_component<BoxCollider2dComponent>()));
                }
                return sol::make_object(lua_state, std::ref(self.add_component<BoxCollider2dComponent>()));
            };
            registry.get_func = [](sol::state_view lua_state, Entity& self) -> sol::object {
                if (!self.has_component<BoxCollider2dComponent>()) return sol::make_object(lua_state, sol::nil);
                return sol::make_object(lua_state, std::ref(self.get_component<BoxCollider2dComponent>()));
            };
            registry.has_func = [](Entity& self) -> bool { return self.has_component<BoxCollider2dComponent>(); };
            registry.remove_func = [](Entity& self) { if (self.has_component<BoxCollider2dComponent>()) self.remove_component<BoxCollider2dComponent>(); };
            registry.set_func = [](Entity& self, const sol::table& data) -> bool {
                auto& c = self.add_or_replace_component<BoxCollider2dComponent>();
                if (sol::object offset_obj = data["offset"]; offset_obj.valid() && offset_obj.get_type() == sol::type::table) {
                    c.offset = table_to_vec2(offset_obj.as<sol::table>(), c.offset);
                }
                if (sol::object size_obj = data["size"]; size_obj.valid() && size_obj.get_type() == sol::type::table) {
                    c.size = table_to_vec2(size_obj.as<sol::table>(), c.size);
                }
                c.density = get_float_field(data, "density", c.density);
                c.friction = get_float_field(data, "friction", c.friction);
                c.restitution = get_float_field(data, "restitution", c.restitution);
                c.restitution_threshold = get_float_field(data, "restitutionThreshold", c.restitution_threshold);
                return true;
            };
            s_component_registry["BoxCollider2dComponent"] = std::move(registry);
        }

        lua.new_usertype<Entity>("Entity",
            sol::constructors<Entity()>(),
            "valid", &Entity::valid,
            "name", &Entity::name,
            "addComponent", sol::overload(
                [&lua](Entity& self, sol::table comp_type) -> sol::object {
                    auto type_name = get_type_name(comp_type);
                    if (!type_name) {
                        throw sol::error("invalid component type: missing __type_name");
                    }
                    const ComponentRegistry* reg = find_component_registry(type_name.value());
                    if (!reg) {
                        throw sol::error("unregistered component type: " + type_name.value());
                    }
                    return reg->add_func(lua, self);
                },
                [&lua](Entity& self, const std::string& type_name) -> sol::object {
                    const ComponentRegistry* reg = find_component_registry(type_name);
                    if (!reg) {
                        throw sol::error("unregistered component type: " + type_name);
                    }
                    return reg->add_func(lua, self);
                }
            ),
            "getComponent", sol::overload(
                [&lua](Entity& self, sol::table comp_type) -> sol::object {
                    auto type_name = get_type_name(comp_type);
                    if (!type_name) {
                        return sol::make_object(lua, sol::nil);
                    }
                    const ComponentRegistry* reg = find_component_registry(type_name.value());
                    if (!reg) {
                        return sol::make_object(lua, sol::nil);
                    }
                    return reg->get_func(lua, self);
                },
                [&lua](Entity& self, const std::string& type_name) -> sol::object {
                    const ComponentRegistry* reg = find_component_registry(type_name);
                    if (!reg) {
                        return sol::make_object(lua, sol::nil);
                    }
                    return reg->get_func(lua, self);
                }
            ),
            "hasComponent", sol::overload(
                [](Entity& self, sol::table comp_type) -> bool {
                    auto type_name = get_type_name(comp_type);
                    if (!type_name) {
                        return false;
                    }
                    const ComponentRegistry* reg = find_component_registry(type_name.value());
                    if (!reg) {
                        return false;
                    }
                    return reg->has_func(self);
                },
                [](Entity& self, const std::string& type_name) -> bool {
                    const ComponentRegistry* reg = find_component_registry(type_name);
                    if (!reg) {
                        return false;
                    }
                    return reg->has_func(self);
                }
            ),
            "removeComponent", sol::overload(
                [](Entity& self, sol::table comp_type) {
                    auto type_name = get_type_name(comp_type);
                    if (!type_name) {
                        return;
                    }
                    const ComponentRegistry* reg = find_component_registry(type_name.value());
                    if (!reg) {
                        return;
                    }
                    reg->remove_func(self);
                },
                [](Entity& self, const std::string& type_name) {
                    const ComponentRegistry* reg = find_component_registry(type_name);
                    if (!reg) {
                        return;
                    }
                    reg->remove_func(self);
                }
            ),
            "setComponent", sol::overload(
                [](Entity& self, sol::table comp_type, const sol::table& data) -> bool {
                    auto type_name = get_type_name(comp_type);
                    if (!type_name) {
                        return false;
                    }
                    const ComponentRegistry* reg = find_component_registry(type_name.value());
                    if (!reg) {
                        return false;
                    }
                    return reg->set_func(self, data);
                },
                [](Entity& self, const std::string& type_name, const sol::table& data) -> bool {
                    const ComponentRegistry* reg = find_component_registry(type_name);
                    if (!reg) {
                        return false;
                    }
                    return reg->set_func(self, data);
                }
            )
        );
    }

} // dodoe::lua_register_detail
