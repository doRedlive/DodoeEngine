// do@Redlive

#include "prefab.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/types/prefab_asset.h"
#include "runtime/resource/resource_manager.h"

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<Prefab>> s_prefab_cache{};

    }

    Prefab* Prefab::Create(const ObjectID& ref, const String& path) {
        DO_PROFILE_SCOPE_CATEGORY("Prefab::Create", "asset");
        if (!ref.isValid() || path.empty()) {
            DO_ERROR("Prefab::Create: invalid object reference or empty path");
            return nullptr;
        }

        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("Prefab::Create: AssetManager unavailable");
            return nullptr;
        }

        PrefabAsset* asset = asset_manager->loadAssetSync<PrefabAsset>(ref.asset_id);
        if (!asset) {
            DO_ERROR("Prefab::Create: failed to load PrefabAsset for '{}'", path);
            return nullptr;
        }

        auto prefab = create_scope<Prefab>(ref);
        Prefab* raw = prefab.get();
        raw->setPath(path);
        raw->setSceneRes(asset->getSceneRes());
        s_prefab_cache.emplace(raw->getInstanceID(), std::move(prefab));
        return raw;
    }

    void Prefab::Shutdown() {
        s_prefab_cache.clear();
    }

} // dodoe
