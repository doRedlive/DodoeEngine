// do@Redlive

#include "asset_manager.h"

#include "runtime/core/project/project.h"
#include "runtime/core/utils/common.h"
#include "runtime/resource/file/file_system.h"

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
        if (m_database) {
            m_database->save();
            m_database.reset();
        }
        m_assets.clear();
        m_path_to_file_id.clear();
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
            case AssetType::Mesh:           return create_scope<MeshAsset>();
            case AssetType::Material:        return create_scope<MaterialAsset>();
            case AssetType::AnimationClip:   return create_scope<AnimationClipAsset>();
            case AssetType::Scene:           return create_scope<SceneAsset>();
            default:                         return nullptr;
        }
    }

    FileID AssetManager::registerAsset(const String& source_path, AssetType type) {
        std::unique_lock lock(m_mutex);

        auto it = m_path_to_file_id.find(source_path);
        if (it != m_path_to_file_id.end()) {
            return it->second;
        }

        UUID uuid = AssetDatabase::generateUUID();
        FileID file_id(source_path, uuid);

        AssetMetaData meta;
        meta.file_id = file_id;
        meta.type = type;
        meta.source_path = source_path;
        meta.name = FileSystem::PathToNameNoExt(source_path);

        meta.asset_path = source_path;

        m_path_to_file_id[source_path] = file_id;

        Size_t type_idx = static_cast<Size_t>(type);
        if (type_idx < static_cast<Size_t>(AssetType::Count)) {
            m_assets_by_type[type_idx].push_back(file_id);
        }

        if (m_database) {
            m_database->setMetaData(file_id, meta);
        }

        return file_id;
    }

    Asset* AssetManager::findAsset(const FileID& file_id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_assets.find(file_id);
        if (it != m_assets.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    Asset* AssetManager::findAssetByPath(const String& source_path) const {
        std::shared_lock lock(m_mutex);
        auto it = m_path_to_file_id.find(source_path);
        if (it != m_path_to_file_id.end()) {
            return findAsset(it->second);
        }
        return nullptr;
    }

    void AssetManager::unloadAsset(const FileID& file_id) {
        std::unique_lock lock(m_mutex);
        auto it = m_assets.find(file_id);
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

    Bool AssetManager::saveAsset(const FileID& file_id) const {
        const Asset* asset = findAsset(file_id);
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

        for (const auto& file_id : m_database->getAllAssetFileIDs()) {
            AssetMetaData meta = m_database->getMetaData(file_id);
            m_path_to_file_id[meta.source_path] = file_id;
            Size_t type_idx = static_cast<Size_t>(meta.type);
            if (type_idx < static_cast<Size_t>(AssetType::Count)) {
                m_assets_by_type[type_idx].push_back(file_id);
            }
        }

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(m_asset_dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const String path = String(entry.path().generic_string().c_str());
                String ext = String(entry.path().extension().string().c_str());
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                FsPath rel_path = std::filesystem::relative(entry.path(), m_asset_dir);
                String source_path = String(rel_path.generic_string().c_str());

                if (ext == kSceneExt) {
                    FileID file_id = registerAsset(source_path, AssetType::Scene);
                    auto scene = create_scope<SceneAsset>();
                    scene->setFileID(file_id);
                    scene->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (scene->loadFromSource(abs_path)) {
                        scene->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[file_id] = std::move(scene);
                } else if (ext == kMaterialExt) {
                    FileID file_id = registerAsset(source_path, AssetType::Material);
                    auto mat = create_scope<MaterialAsset>();
                    mat->setFileID(file_id);
                    mat->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (mat->loadFromSource(abs_path)) {
                        mat->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[file_id] = std::move(mat);
                } else if (ext == kAnimClipExt) {
                    FileID file_id = registerAsset(source_path, AssetType::AnimationClip);
                    auto anim = create_scope<AnimationClipAsset>();
                    anim->setFileID(file_id);
                    anim->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (anim->loadFromSource(abs_path)) {
                        anim->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[file_id] = std::move(anim);
                } else if (std::ranges::find(kImageExts, ext) != kImageExts.end()) {
                    FileID file_id = registerAsset(source_path, AssetType::Texture);
                    auto tex = create_scope<TextureAsset>();
                    tex->setFileID(file_id);
                    tex->setName(FileSystem::PathToNameNoExt(source_path));
                    m_assets[file_id] = std::move(tex);
                } else if (std::ranges::find(kModelExts, ext) != kModelExts.end()) {
                    FileID file_id = registerAsset(source_path, AssetType::Mesh);
                    auto mesh = create_scope<MeshAsset>();
                    mesh->setFileID(file_id);
                    mesh->setName(FileSystem::PathToNameNoExt(source_path));
                    m_assets[file_id] = std::move(mesh);
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

    void AssetManager::discoverAssets() {
        DynamicArray<String> paths;
        FileSystem::TraverseDirectory(paths, m_asset_dir, {".asset"}, false);

        for (const auto& path : paths) {
            FsPath rel = std::filesystem::relative(path, m_asset_dir);
            String rel_str = String(rel.generic_string().c_str());

            std::unique_lock lock(m_mutex);
            if (m_path_to_file_id.find(rel_str) != m_path_to_file_id.end()) {
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
            if (json.contains("file_id")) {
                Serializer::read(json["file_id"], meta.file_id);
            } else {
                String sp = json.value("source_path", rel_str);
                meta.file_id = FileID(sp, AssetDatabase::generateUUID());
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
            if (json.contains("asset_path")) {
                meta.asset_path = json["asset_path"].get<String>();
            }

            FileID file_id = meta.file_id;
            m_path_to_file_id[rel_str] = file_id;

            Size_t type_idx = static_cast<Size_t>(meta.type);
            if (type_idx < static_cast<Size_t>(AssetType::Count)) {
                m_assets_by_type[type_idx].push_back(file_id);
            }

            if (m_database) {
                m_database->setMetaData(file_id, meta);
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

    DynamicArray<FileID> AssetManager::getDependents(const FileID& file_id) const {
        std::shared_lock lock(m_mutex);
        DynamicArray<FileID> result;
        for (const auto& [other_fid, asset] : m_assets) {
            const auto& deps = asset->getMetaData().dependencies;
            for (const auto& dep : deps) {
                if (dep == file_id) {
                    result.push_back(other_fid);
                    break;
                }
            }
        }
        return result;
    }

    DynamicArray<FileID> AssetManager::getDependencies(const FileID& file_id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_assets.find(file_id);
        if (it != m_assets.end()) {
            return it->second->getMetaData().dependencies;
        }
        return {};
    }

    String AssetManager::getAssetPath(const FileID& file_id) const {
        return file_id.getPath();
    }

} // dodoe
