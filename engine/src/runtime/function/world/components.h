//
// Created by GreenMuffin on 2025/11/16.
//

#ifndef DODOE_COMPONENTS_H
#define DODOE_COMPONENTS_H

#include "dopch.h"

#include "box2d/box2d.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"
#include "runtime/core/utils/common.h"

#include "runtime/function/animation/animation.h"
#include "runtime/function/render/camera/camera.h"

#include "runtime/resource/resource_type.h"

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
        identifier id;
        std::string tag;

        const std::string& get_tag() { return tag; }
        void set_tag(const std::string& in_tag) { tag = in_tag; id = string2hash(tag); } 

        TagComponent() : id("default"_hs), tag("default") { }
        TagComponent(const std::string& tag) : id(string2hash(tag)), tag(tag) { }
    };

    struct TransformComponent {
        Vector3f position{ 0.0f, 0.0f, 0.0f };
        Vector3f rotation{ 0.0f, 0.0f, 0.0f };
        Vector3f scale{ 1.0f, 1.0f, 1.0f };
    };

    struct SpriteRendererComponent {
        identifier texture_id{ 0 };
        bool flip{ false };
        Vector2f pivot{0.0f, 0.0f};
        float depth_{0.0f};
        Color color{ };
        SpriteRendererComponent() = default;
    };

    struct ModelRendererComponent {
        identifier model_id{ 0 };
        Color color{ };
        ModelRendererComponent() = default;
    };

    struct MeshRendererComponent {
        identifier mesh_id;
        
    };

    struct ScriptComponent {
        std::string class_name;
        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& /*sc*/) = default;
        ScriptComponent& operator=(const ScriptComponent&) = default;
    };

    struct CameraComponent {
        CameraType type{ CameraType::Orthographic };
        float zoom{ 1.0f };
        Color background{};
        bool dirty{ true };
        bool has_synced{ false };
        Vector3f last_synced_position{ 0.0f, 0.0f, 0.0f };
        float last_synced_rotation{ 0.0f };
        float last_synced_zoom{ 1.0f };
    };

    struct Rigidbody2dComponent {
        enum class BodyType {
            Static = 0,
            Dynamic = 1,
            Kinematic = 2
        };

        BodyType type{ BodyType::Static };
        float gravity_scale{1.0f};
        bool fixed_rotation{ false };
        b2BodyId body_id{ b2_nullBodyId };

        void set_linear_velocity(const Vector2f& velocity) {
            if (B2_IS_NON_NULL(body_id)) {
                b2Body_SetLinearVelocity(body_id, {velocity.x, velocity.y});
            }
        }

        void apply_force_to_center(const Vector2f& force, bool wake) {
            if (B2_IS_NON_NULL(body_id)) {
                b2Body_ApplyForceToCenter(body_id, {force.x, force.y}, wake);
            }
        }

        void apply_linear_impulse_to_center(const Vector2f& impulse, bool wake) {
            if (B2_IS_NON_NULL(body_id)) {
                b2Body_ApplyLinearImpulseToCenter(body_id, {impulse.x, impulse.y}, wake);
            }
        }
    };

    struct BoxCollider2dComponent {
        Vector2f offset{ 0.0f,0.0f };
        Vector2f size{ 10.0f, 10.0f };

        float density{ 1.0f };
        float friction{ 0.5f };
        float restitution{ 0.0f };
        float restitution_threshold{ 0.5f };

        BoxCollider2dComponent() = default;
    };

    struct Animation2dComponent {
        std::unordered_map<identifier, Ref<AnimClip2d>> anim_clip_umap{};
        identifier cur_anim_id{0};
        size_t cur_frame_id{0};
        float cur_time_duration{0.0f};
        float speed{1.0f};

        void add_anim_clip(AnimClip2dRes res) {
            anim_clip_umap.emplace(res.id, res.clip);
            cur_anim_id = res.id;
        }
    };

    struct Animator2dComponent {

    };


} // dodoe

#endif //DODOE_COMPONENTS_H
