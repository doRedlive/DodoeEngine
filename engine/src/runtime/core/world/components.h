//
// Created by GreenMuffin on 2025/11/16.
//

#ifndef DODOE_COMPONENTS_H
#define DODOE_COMPONENTS_H

#include "dopch.h"

#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"

#include "runtime/function/render/camera/camera.h"

#include "glm/glm.hpp"
#include "entt/entt.hpp"

namespace dodoe {

    using namespace entt::literals;

    struct IDComponent {
        Uuid id{};
        std::string name{};

        IDComponent() = default;
        IDComponent(const Uuid& in_id, std::string in_name) : id(in_id), name(std::move(in_name)) {}
        IDComponent(const IDComponent&) = default;
    };

    struct TagComponent {
        entt::id_type id;
        std::string tag;

        TagComponent() : id("default"_hs), tag("default") { }
        TagComponent(const std::string& tag) : id(entt::hashed_string{tag.c_str()}.value()), tag(tag) { }
    };

    struct TransformComponent {
        Vector3f position{ 0.0f, 0.0f, 0.0f };
        Vector3f rotation{ 0.0f, 0.0f, 0.0f };
        Vector3f scale{ 1.0f, 1.0f, 1.0f };
    };

    struct SpriteRendererComponent {
        std::string texture_path;
        // std::string shader_name;
        bool flip{ false };
        Vector2f pivot{0.0f, 0.0f};
        float depth_{0.0f};
        Color color{ };
        SpriteRendererComponent() = default;
        SpriteRendererComponent(const std::string& t_name, const std::string& /*s_name*/) : texture_path(t_name) { }
    };

    struct ScriptComponent {
        std::string class_name;
        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& /*sc*/) = default;
        ScriptComponent& operator=(const ScriptComponent&) = default;
    };

    struct Rigidbody2dComponent {
        enum class BodyType { Static = 0, Dynamic, Kinematic };
        BodyType type{ BodyType::Static };
        bool fixed_rotation{ false };
        void* runtime_body{ nullptr };
        Rigidbody2dComponent() = default;
    };

    struct CameraComponent {
        CameraType type;
        float zoom;
    };

    struct BoxCollider2dComponent {
        Vector2f offset{ 0.0f,0.0f };
        Vector2f size{ 0.0f, 0.0f };

        float density{ 1.0f };
        float friction{ 0.5f };
        float restitution{ 0.0f };
        float restitution_threshold{ 0.5f };

        void* runtime_fixture{ nullptr };

        BoxCollider2dComponent() = default;
    };

    template <typename... Component>
    struct ComponentGroup {
    };

    using AllComponents =
        ComponentGroup<TransformComponent, SpriteRendererComponent, ScriptComponent,
        Rigidbody2dComponent, BoxCollider2dComponent>;


} // dodoe

#endif //DODOE_COMPONENTS_H
