// do@Redlive

#include "picking_backend.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/transform_component.h"

#include <cmath>
#include <limits>

namespace dodoe {

    namespace {

        constexpr float kDefaultBounds = 0.5f;

        bool RayAABBIntersect(const Vector3f& origin,
                              const Vector3f& dir,
                              const Vector3f& boxMin,
                              const Vector3f& boxMax,
                              float& outT)
        {
            float tmin = -std::numeric_limits<float>::infinity();
            float tmax =  std::numeric_limits<float>::infinity();

            {
                const float d = dir.x;
                if (std::fabs(d) > 1e-8f) {
                    float t1 = (boxMin.x - origin.x) / d;
                    float t2 = (boxMax.x - origin.x) / d;
                    if (t1 > t2) std::swap(t1, t2);
                    tmin = Math::Max(tmin, t1);
                    tmax = Math::Min(tmax, t2);
                    if (tmin > tmax) return false;
                } else if (origin.x < boxMin.x || origin.x > boxMax.x) {
                    return false;
                }
            }

            {
                const float d = dir.y;
                if (std::fabs(d) > 1e-8f) {
                    float t1 = (boxMin.y - origin.y) / d;
                    float t2 = (boxMax.y - origin.y) / d;
                    if (t1 > t2) std::swap(t1, t2);
                    tmin = Math::Max(tmin, t1);
                    tmax = Math::Min(tmax, t2);
                    if (tmin > tmax) return false;
                } else if (origin.y < boxMin.y || origin.y > boxMax.y) {
                    return false;
                }
            }

            {
                const float d = dir.z;
                if (std::fabs(d) > 1e-8f) {
                    float t1 = (boxMin.z - origin.z) / d;
                    float t2 = (boxMax.z - origin.z) / d;
                    if (t1 > t2) std::swap(t1, t2);
                    tmin = Math::Max(tmin, t1);
                    tmax = Math::Min(tmax, t2);
                    if (tmin > tmax) return false;
                } else if (origin.z < boxMin.z || origin.z > boxMax.z) {
                    return false;
                }
            }

            if (tmax < 0.0f) return false;
            outT = (tmin >= 0.0f) ? tmin : tmax;
            return true;
        }

        void GetEntityBounds(Entity& entity, Vector3f& outPos, Vector3f& outHalfExt)
        {
            outPos = Vector3f(0.0f);
            outHalfExt = Vector3f(kDefaultBounds);

            if (entity.hasComponent<TransformComponent>()) {
                auto& t = entity.getComponent<TransformComponent>();
                outPos = t.position;
                outHalfExt = {
                    kDefaultBounds * std::fabs(t.scale.x),
                    kDefaultBounds * std::fabs(t.scale.y),
                    kDefaultBounds * std::fabs(t.scale.z)
                };
            }
        }

    } // namespace

    Entity PickingBackend::RaycastNearest(Scene& scene,
                                           const Vector3f& origin,
                                           const Vector3f& dir)
    {
        auto entities = scene.getEntities();
        if (entities.empty()) return {};

        entt::entity nearestHandle = entt::null;
        float nearestT = std::numeric_limits<float>::max();

        for (auto& entity : entities) {
            if (!entity.valid()) continue;

            Vector3f pos, halfExt;
            GetEntityBounds(entity, pos, halfExt);

            float t;
            if (RayAABBIntersect(origin, dir, pos - halfExt, pos + halfExt, t)) {
                if (t < nearestT) {
                    nearestT = t;
                    nearestHandle = entity.handle();
                }
            }
        }

        if (nearestHandle == entt::null) return {};
        return Entity(&scene, nearestHandle);
    }

    Entity PickingBackend::ReadObjectIdBuffer(Scene& scene, int screenX, int screenY)
    {
        (void)scene;
        (void)screenX;
        (void)screenY;
        return {};
    }

    DynamicArray<Entity> PickingBackend::RaycastAll(Scene& scene,
                                                     const Vector3f& origin,
                                                     const Vector3f& dir)
    {
        DynamicArray<Entity> results;

        auto entities = scene.getEntities();
        if (entities.empty()) return results;

        struct Hit { entt::entity handle; float t; };
        DynamicArray<Hit> hits;

        for (auto& entity : entities) {
            if (!entity.valid()) continue;

            Vector3f pos, halfExt;
            GetEntityBounds(entity, pos, halfExt);

            float t;
            if (RayAABBIntersect(origin, dir, pos - halfExt, pos + halfExt, t)) {
                hits.push_back({entity.handle(), t});
            }
        }

        std::sort(hits.begin(), hits.end(),
                  [](const Hit& a, const Hit& b) { return a.t < b.t; });

        for (auto& h : hits) {
            results.push_back(Entity(&scene, h.handle));
        }

        return results;
    }

} // namespace dodoe
