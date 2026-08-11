// do@Redlive

#include "asset_manager.h"

#include "runtime/core/project/project.h"
#include "runtime/core/utils/common.h"
#include "runtime/core/event/event_system.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/asset/importer/asset_importer.h"
#include "runtime/resource/asset/importer/import_settings_io.h"
#include "runtime/resource/asset/importer/texture_importer.h"
#include "runtime/resource/asset/importer/sprite_importer.h"
#include "runtime/resource/asset/importer/model_importer.h"

namespace dodoe {

    namespace {
        const DynamicArray<String> kImageExts = {
            ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga", ".psd", ".hdr"
        };

        const DynamicArray<String> kModelExts = {
            ".obj", ".fbx"
        };

        const String kSceneExt = ".doscn";
        const String kMaterialExt = ".domat";
        const String kAnimClipExt = ".doaniclip";
    }

    Bool AssetManager::initialize(const AssetManagerCreateInfo& info) {
        (void)info;
        return true;
    }

    void AssetManager::shutdown() {
        unloadAll();
        if (m_database && m_database->isDirty()) {
            m_database->save();
        }
        m_database.reset();
        m_assets.clear();
        m_path_to_asset_id.clear();
        for (auto& arr : m_assets_by_type) {
            arr.clear();
        }
    }

    FsPath AssetManager::getFullPath(const String& asset_url) const {
        return (m_asset_dir / FsPath(asset_url)).lexically_normal();
    }

    Scope<Asset> AssetManager::createAssetInstance(AssetType type) {
        switch (type) {
            case AssetType::Texture:        return create_scope<TextureAsset>();
            case AssetType::Sprite:          return create_scope<SpriteAsset>();
            case AssetType::Mesh:           return create_scope<MeshAsset>();
            case AssetType::Material:        return create_scope<MaterialAsset>();
            case AssetType::AnimationClip:   return create_scope<AnimationClipAsset>();
            case AssetType::Scene:           return create_scope<SceneAsset>();
            default:                         return nullptr;
        }
    }

    UUID AssetManager::registerAsset(const String& source_path, AssetType type) {
        std::unique_lock lock(m_mutex);

        auto it = m_path_to_asset_id.find(source_path);
        if (it != m_path_to_asset_id.end()) {
            return it->second;
        }

        UUID asset_id = AssetDatabase::generateUUID();

        AssetMetaData meta;
        meta.ref = ObjectID{asset_id, 0};
        meta.type = type;
        meta.source_file = FileID(source_path);
        meta.source_path = source_path;
        meta.name = FileSystem::PathToNameNoExt(source_path);

        m_path_to_asset_id[source_path] = asset_id;

        Size_t type_idx = static_cast<Size_t>(type);
        if (type_idx < static_cast<Size_t>(AssetType::Count)) {
            m_assets_by_type[type_idx].push_back(asset_id);
        }

        if (m_database) {
            m_database->setMetaData(ObjectID{asset_id, 0}, meta);
        }

        return asset_id;
    }

    Asset* AssetManager::findAsset(const UUID& asset_id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_assets.find(asset_id);
        if (it != m_assets.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    Asset* AssetManager::findAssetByPath(const String& source_path) const {
        std::shared_lock lock(m_mutex);
        auto it = m_path_to_asset_id.find(source_path);
        if (it != m_path_to_asset_id.end()) {
            return findAsset(it->second);
        }
        return nullptr;
    }

    ObjectID AssetManager::resolvePathToRef(const FileID& file_id) const {
        const String& source_path = file_id.getPath();
        if (!source_path.empty()) {
            std::shared_lock lock(m_mutex);
            const auto it = m_path_to_asset_id.find(source_path);
            if (it != m_path_to_asset_id.end()) {
                return ObjectID{it->second, 0};
            }
        }
        if (!m_asset_dir.empty() && !source_path.empty()) {
            const FsPath absolute_path = m_asset_dir / FsPath(source_path.c_str());
            ImportSettings settings;
            if (ImportSettingsIO::Load(absolute_path, settings) && settings.guid.isValid()) {
                return ObjectID{settings.guid, 0};
            }
        }
        return {};
    }

    void AssetManager::unloadAsset(const UUID& asset_id) {
        std::unique_lock lock(m_mutex);
        auto it = m_assets.find(asset_id);
        if (it != m_assets.end()) {
            it->second->unloadRuntime();
            it->second->setLoadState(AssetLoadState::Unloaded);
            m_assets.erase(it);
        }
    }

    void AssetManager::unloadAll() {
        std::unique_lock lock(m_mutex);
        for (auto& pair : m_assets) {
            pair.second->unloadRuntime();
            pair.second->setLoadState(AssetLoadState::Unloaded);
        }
        m_assets.clear();
    }

    Bool AssetManager::saveAsset(const UUID& asset_id) const {
        const Asset* asset = findAsset(asset_id);
        if (!asset) {
            return false;
        }
        if (asset->isReadOnly() || !asset->isLoaded()) {
            return false;
        }
        String abs_path((m_asset_dir / asset->getSourcePath()).string().c_str());
        return asset->saveToSource(abs_path);
    }

    Bool AssetManager::loadAssets() {
        if (m_database) {
            return true;
        }

        const auto active_project = Project::ActiveProject();
        if (!active_project) {
            DO_WARN("AssetManager::loadAssets called but no project is active");
            return false;
        }
        m_asset_dir = Project::ProjectDirectory() / active_project->config().asset_directory;

        const auto configs_dir = Project::ProjectDirectory() / "Configs";
        m_database = create_scope<AssetDatabase>(configs_dir);
        if (!m_database->load()) {
            return false;
        }

        const auto asset_ids = m_database->getAllAssetIDs();
        for (const auto& id : asset_ids) {
            AssetMetaData meta = m_database->getMetaData(id);
            m_path_to_asset_id[meta.source_path] = id.asset_id;
            Size_t type_idx = static_cast<Size_t>(meta.type);
            if (type_idx < static_cast<Size_t>(AssetType::Count)) {
                m_assets_by_type[type_idx].push_back(id.asset_id);
            }
            auto asset = createAssetInstance(meta.type);
            if (asset) {
                asset->setMetaData(meta);
                m_assets[id.asset_id] = std::move(asset);
            }
        }

        if (!asset_ids.empty()) {
            return true;
        }

        EnsureBuiltinImporters();

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(m_asset_dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                String ext = String(entry.path().extension().string().c_str());
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".meta") {
                    continue;
                }

                FsPath rel_path = std::filesystem::relative(entry.path(), m_asset_dir);
                String source_path = String(rel_path.generic_string().c_str());

                if (ext == kSceneExt) {
                    UUID asset_id = registerAsset(source_path, AssetType::Scene);
                    auto scene = create_scope<SceneAsset>();
                    scene->setObjectID(ObjectID{asset_id, 0});
                    scene->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (scene->loadFromSource(abs_path)) {
                        scene->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[asset_id] = std::move(scene);
                } else if (ext == kMaterialExt) {
                    UUID asset_id = registerAsset(source_path, AssetType::Material);
                    auto mat = create_scope<MaterialAsset>();
                    mat->setObjectID(ObjectID{asset_id, 0});
                    mat->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (mat->loadFromSource(abs_path)) {
                        mat->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[asset_id] = std::move(mat);
                } else if (ext == kAnimClipExt) {
                    UUID asset_id = registerAsset(source_path, AssetType::AnimationClip);
                    auto anim = create_scope<AnimationClipAsset>();
                    anim->setObjectID(ObjectID{asset_id, 0});
                    anim->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (anim->loadFromSource(abs_path)) {
                        anim->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[asset_id] = std::move(anim);
                } else if (std::ranges::find(kImageExts, ext) != kImageExts.end()
                           || std::ranges::find(kModelExts, ext) != kModelExts.end()) {
                    importSourceFile(entry.path(), source_path, ext);
                }
            }
        }
        catch (const std::filesystem::filesystem_error& err) {
            DO_ERROR("Traverse {} error: {}", m_asset_dir.string(), err.what());
            return false;
        }

        return true;
    }

    auto AssetManager::loadAssetsAsync() const -> std::future<void> {
        return TaskScheduler::Self().async([this]() {
            const_cast<AssetManager*>(this)->loadAssets();
        });
    }

    void AssetManager::EnsureBuiltinImporters() {
        static Bool s_initialized = false;
        if (s_initialized) {
            return;
        }

        auto& registry = ImporterRegistry::Self();
        for (const auto& ext : kImageExts) {
            registry.registerImporter(ext, create_scope<TextureImporter>());
        }
        registry.registerImporter("", create_scope<SpriteImporter>());
        for (const auto& ext : kModelExts) {
            registry.registerImporter(ext, create_scope<ModelImporter>());
        }
        s_initialized = true;
    }

    void AssetManager::importSourceFile(const FsPath& absolute_path,
                                        const String& source_path,
                                        const String& ext) {
        std::unique_lock lock(m_mutex);

        AssetImporter* default_importer = ImporterRegistry::Self().find(ext);
        if (!default_importer) {
            return;
        }

        ImportSettings settings = ImportSettingsIO::LoadOrCreate(
            absolute_path, source_path,
            String(default_importer->getName()), default_importer->getDefaultSettings());

        UUID asset_id = settings.guid;
        if (!asset_id.isValid()) {
            return;
        }

        const UInt64 mtime = ImportSettingsIO::LastWriteTimeSeconds(absolute_path);
        const UInt64 signature = ComputeImportSignature(settings.settings);

        AssetMetaData cached = m_database ? m_database->getMetaData(ObjectID{asset_id, 0}) : AssetMetaData{};
        const Bool up_to_date = cached.ref.isValid()
            && cached.source_file_mtime == mtime
            && cached.import_signature == signature;

        const auto existing_it = m_path_to_asset_id.find(source_path);
        if (existing_it != m_path_to_asset_id.end()) {
            if (up_to_date) {
                if (m_assets.find(asset_id) == m_assets.end()) {
                    Scope<Asset> asset = createAssetInstance(cached.type);
                    if (asset) {
                        asset->setObjectID(ObjectID{asset_id, 0});
                        asset->setName(FileSystem::PathToNameNoExt(source_path));
                        asset->setMetaData(cached);
                        m_assets[asset_id] = std::move(asset);
                    }
                }
                return;
            }
            if (existing_it->second != asset_id) {
                m_assets.erase(existing_it->second);
                if (m_database) {
                    m_database->removeAsset(ObjectID{existing_it->second, 0});
                }
            }
        }

        AssetImporter* importer = nullptr;
        if (!settings.importer.empty()) {
            importer = ImporterRegistry::Self().findByName(settings.importer);
        }
        if (!importer) {
            importer = default_importer;
        }

        ImportContext ctx{FileID(source_path), source_path, String(absolute_path.generic_string().c_str()),
                          settings.settings, up_to_date ? &cached : nullptr};
        Scope<Asset> asset = importer->import(ctx);
        if (!asset) {
            return;
        }

        asset->setObjectID(ObjectID{asset_id, 0});
        asset->setName(FileSystem::PathToNameNoExt(source_path));

        AssetMetaData meta = asset->getMetaData();
        meta.ref = ObjectID{asset_id, 0};
        meta.type = asset->getType();
        meta.source_file = FileID(source_path);
        meta.source_path = source_path;
        meta.source_file_mtime = mtime;
        meta.import_signature = signature;
        asset->setMetaData(meta);

        m_path_to_asset_id[source_path] = asset_id;

        const Size_t type_idx = static_cast<Size_t>(meta.type);
        if (type_idx < static_cast<Size_t>(AssetType::Count)) {
            auto& by_type = m_assets_by_type[type_idx];
            if (std::ranges::find(by_type, asset_id) == by_type.end()) {
                by_type.push_back(asset_id);
            }
        }

        m_assets[asset_id] = std::move(asset);

        if (m_database) {
            m_database->setMetaData(ObjectID{asset_id, 0}, meta);
        }
    }

    Bool AssetManager::isAssetDirty(const UUID& asset_id) const {
        const Asset* asset = findAsset(asset_id);
        if (!asset) {
            return false;
        }
        const AssetMetaData& meta = asset->getMetaData();
        if (meta.source_path.empty()) {
            return false;
        }

        const FsPath absolute_path = m_asset_dir / FsPath(meta.source_path.c_str());
        if (ImportSettingsIO::LastWriteTimeSeconds(absolute_path) != meta.source_file_mtime) {
            return true;
        }

        ImportSettings settings;
        if (ImportSettingsIO::Load(absolute_path, settings)) {
            return ComputeImportSignature(settings.settings) != meta.import_signature;
        }
        return false;
    }

    Bool AssetManager::reimportAsset(const UUID& asset_id) {
        const Asset* asset = findAsset(asset_id);
        if (!asset) {
            return false;
        }
        const String& source_path = asset->getSourcePath();
        if (source_path.empty()) {
            return false;
        }

        const FsPath absolute_path = m_asset_dir / FsPath(source_path);
        String ext = String(absolute_path.extension().string().c_str());
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        const bool is_importer_asset = std::ranges::find(kImageExts, ext) != kImageExts.end()
            || std::ranges::find(kModelExts, ext) != kModelExts.end();
        if (!is_importer_asset) {
            return false;
        }

        importSourceFile(absolute_path, source_path, ext);
        EventSystem::Publish<AssetReimportedEvent>(AssetReimportedEvent{asset_id, source_path});
        return true;
    }

    Bool AssetManager::refreshAssets() {
        if (m_asset_dir.empty()) {
            return false;
        }

        EnsureBuiltinImporters();

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(m_asset_dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                String ext = String(entry.path().extension().string().c_str());
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".meta") {
                    continue;
                }

                if (std::ranges::find(kImageExts, ext) == kImageExts.end()
                    && std::ranges::find(kModelExts, ext) == kModelExts.end()) {
                    continue;
                }

                FsPath rel_path = std::filesystem::relative(entry.path(), m_asset_dir);
                String source_path = String(rel_path.generic_string().c_str());
                importSourceFile(entry.path(), source_path, ext);
            }
        }
        catch (const std::filesystem::filesystem_error& err) {
            DO_ERROR("Refresh {} error: {}", m_asset_dir.string(), err.what());
            return false;
        }

        if (m_database) {
            m_database->save();
        }
        return true;
    }

    void AssetManager::discoverAssets() {
        DynamicArray<String> paths;
        FileSystem::TraverseDirectory(paths, m_asset_dir, {".asset"}, false);

        for (const auto& path : paths) {
            FsPath rel = std::filesystem::relative(path, m_asset_dir);
            String rel_str = String(rel.generic_string().c_str());

            std::unique_lock lock(m_mutex);
            if (m_path_to_asset_id.find(rel_str) != m_path_to_asset_id.end()) {
                continue;
            }

            std::ifstream file(path.c_str());
            if (!file.is_open()) {
                continue;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();

            Json json;
            try {
                json = Json::parse(buffer.str());
            } catch (const Json::exception&) {
                continue;
            }

            AssetMetaData meta;
            if (json.contains("asset_id")) {
                meta.ref.asset_id = UUID(json["asset_id"].get<UInt64>());
            } else if (json.contains("file_id")) {
                const auto& fid = json["file_id"];
                if (fid.contains("file_uuid")) {
                    meta.ref.asset_id = UUID(fid["file_uuid"].get<UInt64>());
                }
            } else {
                meta.ref = ObjectID{AssetDatabase::generateUUID(), 0};
            }
            if (json.contains("sub_object_id")) {
                meta.ref.local_id = json["sub_object_id"].get<UInt32>();
            }
            if (json.contains("type")) {
                meta.type = Asset::assetTypeFromString(json["type"].get<String>());
            }
            if (json.contains("name")) {
                meta.name = json["name"].get<String>();
            }
            if (json.contains("source_path")) {
                meta.source_path = json["source_path"].get<String>();
            }

            if (!meta.ref.isValid()) {
                continue;
            }

            m_path_to_asset_id[rel_str] = meta.ref.asset_id;

            Size_t type_idx = static_cast<Size_t>(meta.type);
            if (type_idx < static_cast<Size_t>(AssetType::Count)) {
                m_assets_by_type[type_idx].push_back(meta.ref.asset_id);
            }

            if (m_database) {
                m_database->setMetaData(meta.ref, meta);
            }
        }
    }

    Size_t AssetManager::getAssetCount() const {
        std::shared_lock lock(m_mutex);
        return m_assets.size();
    }

    Size_t AssetManager::getAssetCountOfType(AssetType type) const {
        std::shared_lock lock(m_mutex);
        Size_t type_idx = static_cast<Size_t>(type);
        if (type_idx < static_cast<Size_t>(AssetType::Count)) {
            return m_assets_by_type[type_idx].size();
        }
        return 0;
    }

    DynamicArray<UUID> AssetManager::getDependents(const UUID& asset_id) const {
        std::shared_lock lock(m_mutex);
        DynamicArray<UUID> result;
        for (const auto& [other_id, asset] : m_assets) {
            const auto& deps = asset->getMetaData().dependencies;
            for (const auto& dep : deps) {
                if (dep.asset_id == asset_id) {
                    result.push_back(other_id);
                    break;
                }
            }
        }
        return result;
    }

    DynamicArray<ObjectID> AssetManager::getDependencies(const UUID& asset_id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_assets.find(asset_id);
        if (it != m_assets.end()) {
            return it->second->getMetaData().dependencies;
        }
        return {};
    }

    String AssetManager::getAssetPath(const UUID& asset_id) const {
        std::shared_lock lock(m_mutex);
        for (const auto& [source_path, id] : m_path_to_asset_id) {
            if (id == asset_id) {
                return source_path;
            }
        }
        return {};
    }

} // dodoe
