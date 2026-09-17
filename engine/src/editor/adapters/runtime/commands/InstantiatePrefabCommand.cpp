// do@Redlive

#include "InstantiatePrefabCommand.h"

#include "adapters/runtime/services/PrefabService.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/prefab_instance_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"

#include <utility>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

float ReadComponent(const nlohmann::json& value, std::size_t index) {
    if (value.is_array() && index < value.size() && value[index].is_number()) {
        return value[index].get<float>();
    }
    return 0.0f;
}

dodoe::Vector3f ReadPosition(const nlohmann::json& value) {
    return dodoe::Vector3f(ReadComponent(value, 0), ReadComponent(value, 1), ReadComponent(value, 2));
}

} // namespace

InstantiatePrefabCommand::InstantiatePrefabCommand(std::string name, std::filesystem::path prefabPath,
                                                   nlohmann::json position)
    : m_name(std::move(name))
    , m_prefabPath(std::move(prefabPath))
    , m_position(std::move(position))
{}

bool InstantiatePrefabCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (!scene) return false;

    dodoe::Entity marker = ResolveEntity(scene, m_createdUuid);
    bool createdMarker = false;
    if (!marker.valid()) {
        const dodoe::PPtr<dodoe::Prefab> prefabRef = ResolvePrefabRef(m_prefabPath);
        if (!prefabRef.getObjectID().isValid()) return false;

        const dodoe::UUID uuid = m_createdUuid.isValid() ? m_createdUuid : dodoe::UUID::Generate();
        marker = scene->createEntity(uuid, dodoe::String(m_name.c_str()));
        if (!marker.valid()) return false;
        m_createdUuid = marker.uuid();
        createdMarker = true;

        if (!marker.hasComponent<dodoe::TransformComponent>()) {
            marker.addComponent<dodoe::TransformComponent>();
        }
        marker.getComponent<dodoe::TransformComponent>().setPosition(ReadPosition(m_position));
        marker.addComponent<dodoe::HierarchyComponent>();
        auto& inst = marker.addComponent<dodoe::PrefabInstanceComponent>();
        inst.prefab = prefabRef;
    }

    if (!IsPrefabInstanceExpanded(marker) && !ExpandPrefabInstance(marker)) {
        if (createdMarker) {
            DestroySceneSubtree(*scene, marker);
        }
        return false;
    }
    m_created = true;

    if (!model.findEntity(static_cast<std::uint64_t>(m_createdUuid))) {
        model.createEntity(m_name, static_cast<std::uint64_t>(m_createdUuid));
        nlohmann::json transformValue;
        transformValue["position"] = m_position;
        transformValue["rotation"] = nlohmann::json::array({0.0, 0.0, 0.0});
        transformValue["scale"] = nlohmann::json::array({1.0, 1.0, 1.0});
        model.addComponent(static_cast<std::uint64_t>(m_createdUuid),
                           EditorComponent{"TransformComponent", std::move(transformValue)});

        if (marker.hasComponent<dodoe::PrefabInstanceComponent>()) {
            const auto& inst = marker.getComponent<dodoe::PrefabInstanceComponent>();
            const dodoe::ObjectID ref = inst.prefab.getObjectID();
            nlohmann::json instValue;
            instValue["prefab"] = {
                {"asset_id", static_cast<std::uint64_t>(ref.asset_id)},
                {"sub_object_id", static_cast<std::uint64_t>(ref.local_id)},
            };
            instValue["position"] = m_position;
            instValue["rotation"] = nlohmann::json::array({0.0, 0.0, 0.0});
            instValue["scale"] = nlohmann::json::array({1.0, 1.0, 1.0});
            model.addComponent(static_cast<std::uint64_t>(m_createdUuid),
                               EditorComponent{"PrefabInstanceComponent", std::move(instValue)});
        }
    }
    return true;
}

void InstantiatePrefabCommand::revert(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (scene) {
        dodoe::Entity marker = ResolveEntity(scene, m_createdUuid);
        if (marker.valid()) {
            DestroySceneSubtree(*scene, marker);
        }
    }
    model.deleteEntity(static_cast<std::uint64_t>(m_createdUuid));
}

std::string InstantiatePrefabCommand::label() const
{
    return "Instantiate Prefab " + m_name;
}

} // namespace cakery
