// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"
#include "runtime/core/meta/reflection/reflection.h"

REFLECTION_TYPE(TransformComponent)

namespace dodoe {

    STRUCT(TransformComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(TransformComponent)

        META(Enable)
        Vector3f position{ 0.0f, 0.0f, 0.0f };
        META(Enable)
        Vector3f rotation{ 0.0f, 0.0f, 0.0f };
        META(Enable)
        Vector3f scale{ 1.0f, 1.0f, 1.0f };

        bool dirty{false};

        void setPosition(const Vector3f& in_position) { position = in_position; dirty = true; }
        void setRotation(const Vector3f& in_rotation) { rotation = in_rotation; dirty = true; }
        void setScale(const Vector3f& in_scale) { scale = in_scale; dirty = true; }

        [[nodiscard]] const Vector3f& getPosition() const { return position; }
        [[nodiscard]] const Vector3f& getRotation() const { return rotation; }
        [[nodiscard]] const Vector3f& getScale() const { return scale; }
    };

} // dodoe