//
// Created by GreenMuffin on 2026/3/x.
//

#include "lua_register_detail.h"

#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/core/utils/common.h"

#include "runtime/function/world/systems/physics2d_system.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/world.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "box2d/box2d.h"

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

    dodoe::Color table_to_color(const sol::table& data, const dodoe::Color default_value) {
        return dodoe::Color{
            get_float_field(data, "r", default_value.r),
            get_float_field(data, "g", default_value.g),
            get_float_field(data, "b", default_value.b),
            get_float_field(data, "a", default_value.a)
        };
    }

    entt::meta_data find_meta_field(const entt::meta_type& type, const std::string& key) {
        if (const auto direct = type.data(entt::hashed_string{ key.c_str() }.value()); direct) {
            return direct;
        }

        for (auto range = type.data(); const auto& data : range | std::views::values) {
            const auto* name_ptr = static_cast<const std::string*>(data.custom());
            if (name_ptr != nullptr && *name_ptr == key) {
                return data;
            }
        }

        return {};
    }

    bool assign_meta_field_from_lua(entt::meta_any& any, const entt::meta_data& field, const sol::object& value) {
        if (!value.valid()) {
            return false;
        }

        const auto field_type = field.type().info();

        if (field_type == entt::type_id<bool>()) {
            if (value.get_type() == sol::type::boolean) {
                return field.set(entt::meta_handle{ any }, value.as<bool>());
            }
            return false;
        }

        if (field_type == entt::type_id<int>()) {
            if (value.get_type() == sol::type::number) {
                return field.set(entt::meta_handle{ any }, value.as<int>());
            }
            return false;
        }

        if (field_type == entt::type_id<float>()) {
            if (value.get_type() == sol::type::number) {
                return field.set(entt::meta_handle{ any }, value.as<float>());
            }
            return false;
        }

        if (field_type == entt::type_id<size_t>()) {
            if (value.get_type() == sol::type::number) {
                return field.set(entt::meta_handle{ any }, static_cast<size_t>(value.as<uint32_t>()));
            }
            return false;
        }

        if (field_type == entt::type_id<dodoe::identifier>()) {
            if (value.get_type() == sol::type::number) {
                return field.set(entt::meta_handle{ any }, static_cast<dodoe::identifier>(value.as<uint32_t>()));
            }
            return false;
        }

        if (field_type == entt::type_id<std::string>()) {
            if (value.get_type() == sol::type::string) {
                return field.set(entt::meta_handle{ any }, value.as<std::string>());
            }
            return false;
        }

        if (field_type == entt::type_id<dodoe::Rigidbody2dComponent::BodyType>()) {
            if (value.get_type() == sol::type::number) {
                return field.set(entt::meta_handle{ any }, static_cast<dodoe::Rigidbody2dComponent::BodyType>(value.as<int>()));
            }
            return false;
        }

        if (field_type == entt::type_id<dodoe::Vector2f>()) {
            if (value.get_type() == sol::type::table) {
                const auto current = field.get(entt::meta_handle{ any }).cast<dodoe::Vector2f>();
                return field.set(entt::meta_handle{ any }, table_to_vec2(value.as<sol::table>(), current));
            }
            return false;
        }

        if (field_type == entt::type_id<dodoe::Vector3f>()) {
            if (value.get_type() == sol::type::table) {
                const auto current = field.get(entt::meta_handle{ any }).cast<dodoe::Vector3f>();
                return field.set(entt::meta_handle{ any }, table_to_vec3(value.as<sol::table>(), current));
            }
            return false;
        }

        if (field_type == entt::type_id<dodoe::Color>()) {
            if (value.get_type() == sol::type::table) {
                const auto current = field.get(entt::meta_handle{ any }).cast<dodoe::Color>();
                return field.set(entt::meta_handle{ any }, table_to_color(value.as<sol::table>(), current));
            }
            return false;
        }

        return false;
    }

    template <typename T>
    bool set_component_by_reflection(dodoe::Entity& self, const sol::table& data) {
        auto& component = self.addOrReplaceComponent<T>();
        const entt::meta_type type = entt::resolve<T>();
        if (!type) {
            return false;
        }

        entt::meta_any any = type.from_void(&component);
        for (const auto& [key_obj, value_obj] : data) {
            if (key_obj.get_type() != sol::type::string) {
                continue;
            }

            const std::string key = key_obj.as<std::string>();
            const entt::meta_data field = find_meta_field(type, key);
            if (!field) {
                continue;
            }

            assign_meta_field_from_lua(any, field, value_obj);
        }

        return true;
    }

    sol::table copy_table(sol::state_view lua, const sol::table& source) {
        sol::table out = lua.create_table();
        for (const auto& [key, value] : source) {
            out[key] = value;
        }
        return out;
    }

    template <typename T, typename SetFunc>
    void register_component_registry(const std::string& type_name, SetFunc&& set_func) {
        ComponentRegistry registry{};
        registry.add_func = [](sol::state_view lua_state, dodoe::Entity& self) -> sol::object {
            if (self.hasComponent<T>()) {
                return sol::make_object(lua_state, std::ref(self.getComponent<T>()));
            }
            return sol::make_object(lua_state, std::ref(self.addComponent<T>()));
        };
        registry.get_func = [](sol::state_view lua_state, dodoe::Entity& self) -> sol::object {
            if (!self.hasComponent<T>()) {
                return sol::make_object(lua_state, sol::nil);
            }
            return sol::make_object(lua_state, std::ref(self.getComponent<T>()));
        };
        registry.has_func = [](dodoe::Entity& self) -> bool {
            return self.hasComponent<T>();
        };
        registry.remove_func = [](dodoe::Entity& self) {
            if (self.hasComponent<T>()) {
                self.removeComponent<T>();
            }
        };
        registry.set_func = std::forward<SetFunc>(set_func);
        s_component_registry[type_name] = std::move(registry);
    }

    template <typename T>
    void register_component_registry(const std::string& type_name) {
        register_component_registry<T>(type_name, [](dodoe::Entity& self, const sol::table& data) -> bool {
            return set_component_by_reflection<T>(self, data);
        });
    }

    sol::object meta_any_to_lua(sol::state_view lua, entt::meta_any value) {
        if (!value) {
            return sol::make_object(lua, sol::nil);
        }

        const auto type_info = value.type().info();
        if (type_info == entt::type_id<bool>()) {
            return sol::make_object(lua, value.cast<bool>());
        }
        if (type_info == entt::type_id<int>()) {
            return sol::make_object(lua, value.cast<int>());
        }
        if (type_info == entt::type_id<float>()) {
            return sol::make_object(lua, value.cast<float>());
        }
        if (type_info == entt::type_id<size_t>()) {
            return sol::make_object(lua, static_cast<uint32_t>(value.cast<size_t>()));
        }
        if (type_info == entt::type_id<dodoe::identifier>()) {
            return sol::make_object(lua, static_cast<uint32_t>(value.cast<dodoe::identifier>()));
        }
        if (type_info == entt::type_id<std::string>()) {
            return sol::make_object(lua, value.cast<std::string>());
        }
        if (type_info == entt::type_id<dodoe::Vector2f>()) {
            return sol::make_object(lua, vec2_to_table(lua, value.cast<dodoe::Vector2f>()));
        }
        if (type_info == entt::type_id<dodoe::Vector3f>()) {
            return sol::make_object(lua, vec3_to_table(lua, value.cast<dodoe::Vector3f>()));
        }
        if (type_info == entt::type_id<dodoe::Color>()) {
            const auto c = value.cast<dodoe::Color>();
            sol::table t = lua.create_table();
            t["r"] = c.r;
            t["g"] = c.g;
            t["b"] = c.b;
            t["a"] = c.a;
            return sol::make_object(lua, t);
        }
        if (type_info == entt::type_id<dodoe::Rigidbody2dComponent::BodyType>()) {
            return sol::make_object(lua, static_cast<int>(value.cast<dodoe::Rigidbody2dComponent::BodyType>()));
        }

        return sol::make_object(lua, sol::nil);
    }

    template <typename T>
    void bind_reflected_fields(sol::table& type_table) {
        const entt::meta_type meta_type = entt::resolve<T>();
        if (!meta_type) {
            return;
        }

        for (auto range = meta_type.data(); const auto& [field_id, field] : range) {
            const auto* field_name_ptr = static_cast<const std::string*>(field.custom());
            if (field_name_ptr == nullptr || field_name_ptr->empty()) {
                continue;
            }

            const std::string field_name = *field_name_ptr;
            type_table.set(field_name, sol::property(
                [field_id](T& self, sol::this_state lua_state) -> sol::object {
                    sol::state_view lua(lua_state);
                    entt::meta_any any = entt::resolve<T>().from_void(&self);
                    const entt::meta_data data = entt::resolve<T>().data(field_id);
                    if (!data) {
                        return sol::make_object(lua, sol::nil);
                    }
                    return meta_any_to_lua(lua, data.get(entt::meta_handle{ any }));
                },
                [field_id](T& self, const sol::object& value) {
                    entt::meta_any any = entt::resolve<T>().from_void(&self);
                    const entt::meta_data data = entt::resolve<T>().data(field_id);
                    if (!data) {
                        return;
                    }
                    assign_meta_field_from_lua(any, data, value);
                }
            ));
        }
    }

    template <typename T, typename... Ctors>
    void register_reflected_usertype(sol::state& lua, sol::table& dodoe_table, const char* type_name,
                                     sol::constructors<Ctors...> ctors) {
        dodoe_table.new_usertype<T>(type_name, ctors);
        sol::table type_table = dodoe_table[type_name];
        bind_reflected_fields<T>(type_table);
        type_table["__type_name"] = std::string(type_name);
        (void)lua;
    }

}

namespace dodoe::lua_register_detail {

    void register_entity(sol::state& lua, sol::table& dodoe_table) {
        dodoe_table.new_enum<Rigidbody2dComponent::BodyType>(
            "RigidbodyBodyType", {
                {"static", Rigidbody2dComponent::BodyType::Static},
                {"dynamic", Rigidbody2dComponent::BodyType::Dynamic},
                {"kinematic", Rigidbody2dComponent::BodyType::Kinematic}
            }
        );

        register_reflected_usertype<TransformComponent>(lua, dodoe_table, "TransformComponent", sol::constructors<TransformComponent()>());
        register_reflected_usertype<TagComponent>(lua, dodoe_table, "TagComponent", sol::constructors<TagComponent(), TagComponent(const std::string&)>());

        dodoe_table.new_usertype<Rigidbody2dComponent>("Rigidbody2dComponent",
            sol::constructors<Rigidbody2dComponent()>(),
            "body_type", sol::property(
                [](Rigidbody2dComponent& c) { return static_cast<int>(c.type); },
                [](Rigidbody2dComponent& c, const int value) { c.type = static_cast<Rigidbody2dComponent::BodyType>(value); }
            ),
            "setLinearVelocity", &Rigidbody2dComponent::setLinearVelocity,
            "applyForceToCenter", &Rigidbody2dComponent::applyForceToCenter,
            "applyLinearImpulseToCenter", &Rigidbody2dComponent::applyLinearImpulseToCenter
        );

        {
            sol::table type_table = dodoe_table["Rigidbody2dComponent"];
            bind_reflected_fields<Rigidbody2dComponent>(type_table);
            type_table["__type_name"] = "Rigidbody2dComponent";
        }

        register_reflected_usertype<BoxCollider2dComponent>(lua, dodoe_table, "BoxCollider2dComponent", sol::constructors<BoxCollider2dComponent()>());
        register_reflected_usertype<SpriteRendererComponent>(lua, dodoe_table, "SpriteRendererComponent", sol::constructors<SpriteRendererComponent()>());
        dodoe_table.new_usertype<Animation2dComponent>("Animation2dComponent",
            sol::constructors<Animation2dComponent()>(),
            "addAnimClip", &Animation2dComponent::addClip
        );

        {
            sol::table type_table = dodoe_table["Animation2dComponent"];
            bind_reflected_fields<Animation2dComponent>(type_table);
            type_table["__type_name"] = "Animation2dComponent";
        }

        s_component_registry.clear();

        register_component_registry<TagComponent>("TagComponent");
        register_component_registry<TransformComponent>("TransformComponent");
        register_component_registry<Animation2dComponent>("Animation2dComponent");

        register_component_registry<Rigidbody2dComponent>("Rigidbody2dComponent", [](Entity& self, const sol::table& data) -> bool {
            sol::state_view lua = data.lua_state();
            sol::table patch = copy_table(lua, data);
            if (const sol::object body_type = data["body_type"]; body_type.valid()) {
                patch["type"] = body_type;
            }
            return set_component_by_reflection<Rigidbody2dComponent>(self, patch);
        });

        register_component_registry<BoxCollider2dComponent>("BoxCollider2dComponent", [](Entity& self, const sol::table& data) -> bool {
            sol::state_view lua = data.lua_state();
            sol::table patch = copy_table(lua, data);
            if (const sol::object old_key = data["restitutionThreshold"]; old_key.valid()) {
                patch["restitution_threshold"] = old_key;
            }
            return set_component_by_reflection<BoxCollider2dComponent>(self, patch);
        });

        register_component_registry<SpriteRendererComponent>("SpriteRendererComponent", [](Entity& self, const sol::table& data) -> bool {
            sol::state_view lua = data.lua_state();
            sol::table patch = copy_table(lua, data);

            if (const sol::object texture = data["texture"]; texture.valid() && texture.get_type() == sol::type::string) {
                const std::string texture_name = texture.as<std::string>();
                if (!texture_name.empty()) {
                    patch["texture_id"] = static_cast<identifier>(string2hash(texture_name));
                }
            }

            if (const sol::object depth = data["depth"]; depth.valid()) {
                patch["depth_"] = depth;
            }

            return set_component_by_reflection<SpriteRendererComponent>(self, patch);
        });

        dodoe_table.new_usertype<Entity>("Entity",
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
