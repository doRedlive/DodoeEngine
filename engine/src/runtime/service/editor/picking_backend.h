// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    class Scene;
    class Entity;

    class DODOE_API PickingBackend {
    public:
        static Entity RaycastNearest(Scene& scene,
                                     const Vector3f& origin,
                                     const Vector3f& dir);

        static Entity ReadObjectIdBuffer(Scene& scene, int screenX, int screenY);

        static DynamicArray<Entity> RaycastAll(Scene& scene,
                                               const Vector3f& origin,
                                               const Vector3f& dir);
    };

} // namespace dodoe
