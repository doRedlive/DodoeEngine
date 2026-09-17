// do@Redlive

#pragma once

#include "runtime/core/object/pptr.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/function/world/prefab.h"

#include <filesystem>

namespace dodoe {
    class Scene;
    class Entity;
}

namespace cakery {

dodoe::PPtr<dodoe::Prefab> ResolvePrefabRef(const std::filesystem::path& prefabPath);
bool ExpandPrefabInstance(dodoe::Entity marker);
void DestroySceneSubtree(dodoe::Scene& scene, dodoe::Entity root);
bool IsPrefabInstanceExpanded(const dodoe::Entity& marker);

} // namespace cakery
