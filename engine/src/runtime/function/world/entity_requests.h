// do@Redlive

#pragma once

#include "dopch.h"

#include "entity.h"
#include "scene.h"
#include "components/request_components.h"
#include "components/rigidbody2d_component.h"
#include "components/rigidbody_component.h"
#include "components/animator_component.h"

namespace dodoe {

    inline void setVelocity2d(Entity entity, const Vector2f& velocity) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<Rigidbody2dComponent>(entity)) return;
        reg.emplace_or_replace<SetVelocity2dRequest>(entity, SetVelocity2dRequest{ velocity });
    }

    inline void applyForce2d(Entity entity, const Vector2f& force) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<Rigidbody2dComponent>(entity)) return;
        if (reg.all_of<ApplyForce2dRequest>(entity)) {
            reg.get<ApplyForce2dRequest>(entity).force += force;
        } else {
            reg.emplace<ApplyForce2dRequest>(entity, ApplyForce2dRequest{ force });
        }
    }

    inline void applyImpulse2d(Entity entity, const Vector2f& impulse) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<Rigidbody2dComponent>(entity)) return;
        if (reg.all_of<ApplyImpulse2dRequest>(entity)) {
            reg.get<ApplyImpulse2dRequest>(entity).impulse += impulse;
        } else {
            reg.emplace<ApplyImpulse2dRequest>(entity, ApplyImpulse2dRequest{ impulse });
        }
    }

    inline void setVelocity3d(Entity entity, const Vector3f& velocity) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<RigidbodyComponent>(entity)) return;
        reg.emplace_or_replace<SetVelocityRequest>(entity, SetVelocityRequest{ velocity });
    }

    inline void applyForce3d(Entity entity, const Vector3f& force) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<RigidbodyComponent>(entity)) return;
        if (reg.all_of<ApplyForceRequest>(entity)) {
            reg.get<ApplyForceRequest>(entity).force += force;
        } else {
            reg.emplace<ApplyForceRequest>(entity, ApplyForceRequest{ force });
        }
    }

    inline void applyImpulse3d(Entity entity, const Vector3f& impulse) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<RigidbodyComponent>(entity)) return;
        if (reg.all_of<ApplyImpulseRequest>(entity)) {
            reg.get<ApplyImpulseRequest>(entity).impulse += impulse;
        } else {
            reg.emplace<ApplyImpulseRequest>(entity, ApplyImpulseRequest{ impulse });
        }
    }

    inline void teleportEntity(Entity entity, const Vector3f& position, const Quaternion& rotation) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<RigidbodyComponent>(entity)) return;
        reg.emplace_or_replace<TeleportRequest>(entity, TeleportRequest{ position, rotation });
    }

    inline void animatorPlay(Entity entity, const String& name) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<AnimatorComponent>(entity)) return;
        reg.emplace_or_replace<PlayAnimationRequest>(entity, PlayAnimationRequest{ name });
    }

    inline void animatorStop(Entity entity) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<AnimatorComponent>(entity)) return;
        reg.emplace<StopAnimationRequest>(entity);
    }

    inline void animatorResume(Entity entity) {
        if (!entity) return;
        auto* scene = entity.getScene();
        if (!scene) return;
        auto& reg = scene->registry();
        if (!reg.all_of<AnimatorComponent>(entity)) return;
        reg.emplace<ResumeAnimationRequest>(entity);
    }

} // dodoe
