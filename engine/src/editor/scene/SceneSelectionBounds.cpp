// do@Redlive
#include "dopch.h"
#include "SceneSelectionBounds.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/components/sprite_renderer_component.h"
#include "runtime/function/world/components/mesh_renderer_component.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include <array>

using namespace dodoe;
namespace cakery {
namespace {
Matrix4f localTransform(const TransformComponent& t) {
    Matrix4f result = Math::Translate(Matrix4f(1.0f), t.position);
    result = Math::Rotate(result, Math::Radians(t.rotation.x), Vector3f(1, 0, 0));
    result = Math::Rotate(result, Math::Radians(t.rotation.y), Vector3f(0, 1, 0));
    result = Math::Rotate(result, Math::Radians(t.rotation.z), Vector3f(0, 0, 1));
    return Math::Scale(result, t.scale);
}
}

Matrix4f sceneEntityParentTransform(Entity entity)
{
    // Sprites currently use the flat XY transform in SpriteRendererSystem.
    if (!entity || entity.hasComponent<SpriteRendererComponent>()) return Matrix4f(1.0f);
    std::array<const TransformComponent*, 256> chain{};
    Size_t count = 0;
    while (entity && entity.hasComponent<HierarchyComponent>() && count < chain.size()) {
        entity = entity.getComponent<HierarchyComponent>().parent;
        if (!entity || !entity.hasComponent<TransformComponent>()) break;
        chain[count++] = &entity.getComponent<TransformComponent>();
    }
    Matrix4f result(1.0f);
    while (count > 0) result = result * localTransform(*chain[--count]);
    return result;
}

Matrix4f sceneEntityTransform(Entity entity)
{
    if (!entity || !entity.hasComponent<TransformComponent>()) return Matrix4f(1.0f);
    const auto& t = entity.getComponent<TransformComponent>();
    if (entity.hasComponent<SpriteRendererComponent>()) {
        Matrix4f result = Math::Translate(Matrix4f(1.0f), Vector3f(t.position.x, t.position.y, 0.0f));
        result = Math::Rotate(result, Math::Radians(t.rotation.z), Vector3f(0, 0, 1));
        return Math::Scale(result, Vector3f(t.scale.x, t.scale.y, 1.0f));
    }
    return sceneEntityParentTransform(entity) * localTransform(t);
}

SceneSelectionBounds sceneSelectionBounds(Entity entity)
{
    SceneSelectionBounds result;
    if (!entity || !entity.hasComponent<TransformComponent>()) return result;
    result.localToWorld = sceneEntityTransform(entity);
    if (entity.hasComponent<SpriteRendererComponent>()) {
        const auto* sprite = entity.getComponent<SpriteRendererComponent>().sprite.get();
        if (sprite && String(sprite->getObjectTypeName()) != "Sprite") sprite = nullptr;
        const Vector2f size = sprite ? sprite->getNaturalSize() : Vector2f(1.0f);
        result.minimum = Vector3f(-size.x * 0.5f, -size.y * 0.5f, 0.0f);
        result.maximum = -result.minimum;
        result.planar = result.hasGeometry = true;
    } else if (entity.hasComponent<MeshRendererComponent>()) {
        if (const auto* mesh = entity.getComponent<MeshRendererComponent>().mesh.get()) {
            result.minimum = mesh->getBoundsMin();
            result.maximum = mesh->getBoundsMax();
            result.hasGeometry = true;
        }
    }
    return result;
}

} // namespace cakery
