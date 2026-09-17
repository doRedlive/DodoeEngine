// do@Redlive

#include "PrefabService.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/prefab_instance_component.h"
#include "runtime/function/world/components/prefab_node_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/service/world/scene_importer.h"

#include <algorithm>
#include <vector>

namespace cakery {

dodoe::PPtr<dodoe::Prefab> ResolvePrefabRef(const std::filesystem::path& prefabPath)
{
    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) {
        return {};
    }
    std::error_code ec;
    std::filesystem::path absolute = prefabPath.is_absolute()
        ? prefabPath.lexically_normal()
        : (assetManager->getAssetDir() / prefabPath).lexically_normal();
    const dodoe::ObjectID ref = assetManager->ensureImported(
        dodoe::String(absolute.generic_string().c_str()));
    if (!ref.isValid()) {
        return {};
    }
    dodoe::PPtr<dodoe::Prefab> prefabRef(ref);
    prefabRef.setLegacyPath(dodoe::String(absolute.generic_string().c_str()));
    return prefabRef;
}

bool IsPrefabInstanceExpanded(const dodoe::Entity& marker)
{
    if (!marker.valid() || !marker.hasComponent<dodoe::PrefabInstanceComponent>()) {
        return false;
    }
    if (marker.hasComponent<dodoe::PrefabNodeComponent>()) {
        return true;
    }
    if (!marker.hasComponent<dodoe::HierarchyComponent>()) {
        return false;
    }
    return !marker.getComponent<dodoe::HierarchyComponent>().children.empty();
}

bool ExpandPrefabInstance(dodoe::Entity marker)
{
    if (!marker.valid() || !marker.hasComponent<dodoe::PrefabInstanceComponent>()) {
        return false;
    }
    if (IsPrefabInstanceExpanded(marker)) {
        return true;
    }

    auto& inst = marker.getComponent<dodoe::PrefabInstanceComponent>();
    dodoe::Entity instanceRoot = dodoe::SceneImporter::InstantiatePrefab(inst.prefab);
    if (!instanceRoot.valid()) {
        return false;
    }
    if (instanceRoot.hasComponent<dodoe::PrefabInstanceComponent>()) {
        instanceRoot.removeComponent<dodoe::PrefabInstanceComponent>();
    }

    if (!marker.hasComponent<dodoe::HierarchyComponent>()) {
        marker.addComponent<dodoe::HierarchyComponent>();
    }
    auto& markerHC = marker.getComponent<dodoe::HierarchyComponent>();
    if (!instanceRoot.hasComponent<dodoe::HierarchyComponent>()) {
        instanceRoot.addComponent<dodoe::HierarchyComponent>();
    }
    auto& rootHC = instanceRoot.getComponent<dodoe::HierarchyComponent>();
    if (rootHC.parent.valid() && rootHC.parent.hasComponent<dodoe::HierarchyComponent>()) {
        auto& oldParentHC = rootHC.parent.getComponent<dodoe::HierarchyComponent>();
        auto& siblings = oldParentHC.children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), instanceRoot), siblings.end());
        oldParentHC.child_count = static_cast<int>(siblings.size());
        oldParentHC.dirty = true;
    }
    rootHC.parent = marker;
    rootHC.parent_uuid = marker.uuid();
    rootHC.dirty = true;
    markerHC.children.push_back(instanceRoot);
    markerHC.child_count = static_cast<int>(markerHC.children.size());
    markerHC.dirty = true;

    if (marker.hasComponent<dodoe::TransformComponent>()) {
        const auto& tc = marker.getComponent<dodoe::TransformComponent>();
        inst.position = tc.getPosition();
        inst.rotation = tc.getRotation();
        inst.scale = tc.getScale();
    }
    return true;
}

void DestroySceneSubtree(dodoe::Scene& scene, dodoe::Entity root)
{
    if (!root.valid()) {
        return;
    }
    std::vector<dodoe::Entity> subtree;
    subtree.push_back(root);
    for (std::size_t i = 0; i < subtree.size(); ++i) {
        dodoe::Entity entity = subtree[i];
        if (!entity.valid() || !entity.hasComponent<dodoe::HierarchyComponent>()) {
            continue;
        }
        for (dodoe::Entity child : entity.getComponent<dodoe::HierarchyComponent>().children) {
            if (child.valid()) {
                subtree.push_back(child);
            }
        }
    }
    if (root.hasComponent<dodoe::HierarchyComponent>()) {
        auto& hc = root.getComponent<dodoe::HierarchyComponent>();
        if (hc.parent.valid() && hc.parent.hasComponent<dodoe::HierarchyComponent>()) {
            auto& parentHC = hc.parent.getComponent<dodoe::HierarchyComponent>();
            auto& siblings = parentHC.children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), root), siblings.end());
            parentHC.child_count = static_cast<int>(siblings.size());
            parentHC.dirty = true;
        }
    }
    for (auto it = subtree.rbegin(); it != subtree.rend(); ++it) {
        if (it->valid()) {
            scene.destroyEntity(*it);
        }
    }
}

} // namespace cakery
