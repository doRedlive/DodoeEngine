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
#include "runtime/resource/asset/importer/tiled_map_importer.h"
#include "runtime/resource/asset/importer/audio_importer.h"
#include "runtime/resource/asset/types/animator_controller_asset.h"
#include "runtime/resource/asset/types/tileset_asset.h"
#include "runtime/resource/asset/types/input_action_asset.h"
#include "runtime/resource/asset/types/tiled_map_asset.h"
#include "runtime/resource/asset/types/audio_clip_asset.h"

namespace dodoe {

    namespace {
        const DynamicArray<String> kImageExts = {
            ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga", ".psd", ".hdr"
        };

        const DynamicArray<String> kModelExts = {
            ".obj", ".fbx", ".gltf", ".glb"
        };

        const DynamicArray<String> kTiledMapExts = {
            ".tmj"
        };

        const DynamicArray<String> kAudioExts = {
            ".wav", ".mp3", ".flac", ".ogg"
        };

        const String kSceneExt = ".doscn";
        const String kMaterialExt = ".domat";
        const String kAnimClipExt = ".doaniclip";
        const String kAnimatorControllerExt = ".doanim";
        const String kTilesetExt = ".tsx";
        const String kInputActionExt = ".doinput";
    }

    Bool AssetManager::initialize(const AssetManagerCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("AssetManager::initialize", "startup");
        (void)info;
        return true;
    }

    void AssetManager::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("AssetManager::shutdown", "shutdown");
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

    void AssetManager::registerMaterialAsset(Scope<MaterialAsset> asset) {
        if (!asset) {
            return;
        }

        std::unique_lock lock(m_mutex);

        const ObjectID ref = asset->getObjectID();
        const AssetMetaData& meta = asset->getMetaData();
        if (ref.asset_id.isValid() && !meta.source_path.empty()) {
            m_path_to_asset_id[meta.source_path] = ref.asset_id;

            const Size_t type_idx = static_cast<Size_t>(AssetType::Material);
            if (type_idx < static_cast<Size_t>(AssetType::Count)) {
                auto& by_type = m_assets_by_type[type_idx];
                if (std::ranges::find(by_type, ref.asset_id) == by_type.end()) {
                    by_type.push_back(ref.asset_id);
                }
            }

            if (m_database) {
                m_database->setMetaData(ref, meta);
            }
        }

        m_assets[ref.asset_id] = std::move(asset);
    }

    Scope<Asset> AssetManager::createAssetInstance(AssetType type) {
        switch (type) {
            case AssetType::Texture:        return create_scope<TextureAsset>();
            case AssetType::Sprite:          return create_scope<SpriteAsset>();
            case AssetType::Mesh:           return create_scope<MeshAsset>();
            case AssetType::Material:        return create_scope<MaterialAsset>();
            case AssetType::Anim2DClip:      return create_scope<Anim2DClipAsset>();
            case AssetType::AnimatorController: return create_scope<AnimatorControllerAsset>();
            case AssetType::Scene:           return create_scope<SceneAsset>();
            case AssetType::Tileset:         return create_scope<TilesetAsset>();
            case AssetType::InputAction:     return create_scope<InputActionAsset>();
            case AssetType::TiledMap:        return create_scope<TiledMapAsset>();
            case AssetType::Audio:           return create_scope<AudioClipAsset>();
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
            String lookup_path = source_path;
            FsPath meta_path;
            if (FsPath(source_path.c_str()).is_absolute()) {
                if (!m_asset_dir.empty()) {
                    std::error_code ec;
                    const FsPath rel = std::filesystem::relative(FsPath(source_path.c_str()), m_asset_dir, ec);
                    if (!ec) {
                        lookup_path = String(rel.generic_string().c_str());
                    }
                }
                meta_path = FsPath(source_path.c_str());
            } else {
                meta_path = m_asset_dir / FsPath(source_path.c_str());
            }
            {
                std::shared_lock lock(m_mutex);
                const auto it = m_path_to_asset_id.find(lookup_path);
                if (it != m_path_to_asset_id.end()) {
                    return ObjectID{it->second, 0};
                }
            }
            if (!meta_path.empty()) {
                ImportSettings settings;
                if (ImportSettingsIO::Load(meta_path, settings) && settings.guid.isValid()) {
                    return ObjectID{settings.guid, 0};
                }
            }
        }
        return {};
    }

    ObjectID AssetManager::resolveSubObjectRef(const FileID& file_id, UInt32 local_id) const {
        const ObjectID main_ref = resolvePathToRef(file_id);
        if (!main_ref.isValid()) {
            return {};
        }
        const String& source_path = file_id.getPath();
        if (source_path.empty()) {
            return {};
        }
        const FsPath meta_path = FsPath(source_path.c_str()).is_absolute()
            ? FsPath(source_path.c_str())
            : (m_asset_dir / FsPath(source_path.c_str()));
        ImportSettings settings;
        if (!ImportSettingsIO::Load(meta_path, settings) || settings.sprites.empty()) {
            return {};
        }
        const SpriteMeta* found = nullptr;
        for (const auto& sprite : settings.sprites) {
            if (sprite.local_id == local_id) {
                found = &sprite;
                break;
            }
        }
        if (!found) {
            found = &settings.sprites.front();
        }
        if (found->local_id == 0) {
            return {};
        }
        return ObjectID{main_ref.asset_id, found->local_id};
    }

    ObjectID AssetManager::ensureImported(const String& absolute_path) {
        if (m_asset_dir.empty() || !m_database) {
            DO_ERROR("AssetManager::ensureImported: asset manager not initialized");
            return {};
        }

        const FsPath abs = FsPath(absolute_path.c_str()).lexically_normal();
        if (!std::filesystem::exists(abs) || !std::filesystem::is_regular_file(abs)) {
            DO_ERROR("AssetManager::ensureImported: '{}' is not a regular file", absolute_path);
            return {};
        }

        std::error_code ec;
        const FsPath rel = std::filesystem::relative(abs, m_asset_dir, ec);
        const String rel_str = String(rel.generic_string().c_str());
        if (ec || rel.empty() || rel_str.starts_with("..")) {
            DO_WARN("AssetManager::ensureImported: '{}' is outside the project asset directory '{}'",
                    absolute_path, m_asset_dir.string());
            return {};
        }
        const String source_path = String(rel_str.c_str());

        String ext = String(abs.extension().string().c_str());
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (std::ranges::find(kImageExts, ext) == kImageExts.end()
            && std::ranges::find(kModelExts, ext) == kModelExts.end()
            && std::ranges::find(kTiledMapExts, ext) == kTiledMapExts.end()) {
            DO_ERROR("AssetManager::ensureImported: unsupported format '{}'", ext);
            return {};
        }

        EnsureBuiltinImporters();
        importSourceFile(abs, source_path, ext);

        return resolvePathToRef(FileID(source_path));
    }

    ObjectID AssetManager::ensureTilesetImported(const String& absolute_path) {
        if (m_asset_dir.empty() || !m_database) {
            DO_ERROR("AssetManager::ensureTilesetImported: asset manager not initialized");
            return {};
        }

        const FsPath abs = FsPath(absolute_path.c_str()).lexically_normal();
        if (!std::filesystem::exists(abs) || !std::filesystem::is_regular_file(abs)) {
            DO_ERROR("AssetManager::ensureTilesetImported: '{}' is not a regular file", absolute_path);
            return {};
        }

        std::error_code ec;
        const FsPath rel = std::filesystem::relative(abs, m_asset_dir, ec);
        const String rel_str = String(rel.generic_string().c_str());
        if (ec || rel.empty() || rel_str.starts_with("..")) {
            DO_WARN("AssetManager::ensureTilesetImported: '{}' is outside the project asset directory '{}'",
                    absolute_path, m_asset_dir.string());
            return {};
        }
        const String source_path = String(rel_str.c_str());

        UUID asset_id;
        {
            std::unique_lock lock(m_mutex);
            const auto it = m_path_to_asset_id.find(source_path);
            if (it != m_path_to_asset_id.end()) {
                asset_id = it->second;
            }
        }
        if (!asset_id.isValid()) {
            asset_id = registerAsset(source_path, AssetType::Tileset);
            if (!asset_id.isValid()) {
                return {};
            }
        }
        {
            std::unique_lock lock(m_mutex);
            if (m_assets.find(asset_id) == m_assets.end()) {
                auto tileset = create_scope<TilesetAsset>();
                tileset->setObjectID(ObjectID{asset_id, 0});
                tileset->setName(FileSystem::PathToNameNoExt(source_path));
                if (tileset->loadFromSource(abs.generic_string().c_str())) {
                    tileset->setLoadState(AssetLoadState::Loaded);
                }
                m_assets[asset_id] = std::move(tileset);
            }
        }
        return ObjectID{asset_id, 0};
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
        DO_PROFILE_SCOPE_CATEGORY("AssetManager::loadAssets", "startup");
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
        DO_PROFILE_MARK("AssetManager::loadAssets.loadDatabase", "startup");
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
            DO_PROFILE_MARK("AssetManager::loadAssets.databaseReady", "startup");
            return true;
        }

        EnsureBuiltinImporters();
        DO_PROFILE_MARK("AssetManager::loadAssets.scanDirectory", "startup");

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
                    UUID asset_id = registerAsset(source_path, AssetType::Anim2DClip);
                    auto anim = create_scope<Anim2DClipAsset>();
                    anim->setObjectID(ObjectID{asset_id, 0});
                    anim->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (anim->loadFromSource(abs_path)) {
                        anim->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[asset_id] = std::move(anim);
                } else if (ext == kAnimatorControllerExt) {
                    UUID asset_id = registerAsset(source_path, AssetType::AnimatorController);
                    auto controller = create_scope<AnimatorControllerAsset>();
                    controller->setObjectID(ObjectID{asset_id, 0});
                    controller->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (controller->loadFromSource(abs_path)) {
                        controller->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[asset_id] = std::move(controller);
                } else if (ext == kTilesetExt) {
                    UUID asset_id = registerAsset(source_path, AssetType::Tileset);
                    auto tileset = create_scope<TilesetAsset>();
                    tileset->setObjectID(ObjectID{asset_id, 0});
                    tileset->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (tileset->loadFromSource(abs_path)) {
                        tileset->setLoadState(AssetLoadState::Loaded);
                    }
                    m_assets[asset_id] = std::move(tileset);
                } else if (ext == kInputActionExt) {
                    UUID asset_id = registerAsset(source_path, AssetType::InputAction);
                    auto input = create_scope<InputActionAsset>();
                    input->setObjectID(ObjectID{asset_id, 0});
                    input->setName(FileSystem::PathToNameNoExt(source_path));
                    String abs_path(entry.path().generic_string().c_str());
                    if (input->loadFromSource(abs_path)) input->setLoadState(AssetLoadState::Loaded);
                    m_assets[asset_id] = std::move(input);
                } else if (std::ranges::find(kImageExts, ext) != kImageExts.end()
                           || std::ranges::find(kModelExts, ext) != kModelExts.end()
                           || std::ranges::find(kTiledMapExts, ext) != kTiledMapExts.end()) {
                    importSourceFile(entry.path(), source_path, ext);
                }
            }
        }
        catch (const std::filesystem::filesystem_error& err) {
            DO_ERROR("Traverse {} error: {}", m_asset_dir.string(), err.what());
            return false;
        }

        DO_PROFILE_MARK("AssetManager::loadAssets.scanComplete", "startup");
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
        for (const auto& ext : kTiledMapExts) {
            registry.registerImporter(ext, create_scope<TiledMapImporter>());
        }
        for (const auto& ext : kAudioExts) {
            registry.registerImporter(ext, create_scope<AudioImporter>());
        }
        s_initialized = true;
    }

    void AssetManager::importSourceFile(const FsPath& absolute_path,
                                        const String& source_path,
                                        const String& ext) {
        DO_PROFILE_MARK("AssetManager::importSourceFile", "startup");
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
                          asset_id, settings.settings, up_to_date ? &cached : nullptr};

        lock.unlock();
        Scope<Asset> asset = importer->import(ctx);
        lock.lock();
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

    Bool AssetManager::refreshAssets(RefreshProgressFn progress) {
        if (m_asset_dir.empty()) {
            return false;
        }

        EnsureBuiltinImporters();
        m_refresh_cancelled.store(false, std::memory_order_relaxed);

        const auto is_importable = [](const String& ext) {
            return std::ranges::find(kImageExts, ext) != kImageExts.end()
                || std::ranges::find(kModelExts, ext) != kModelExts.end()
                || std::ranges::find(kTiledMapExts, ext) != kTiledMapExts.end();
        };

        std::size_t total = 0;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(m_asset_dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                String ext = String(entry.path().extension().string().c_str());
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".meta" || !is_importable(ext)) {
                    continue;
                }
                ++total;
            }
        }
        catch (const std::filesystem::filesystem_error& err) {
            DO_ERROR("Refresh {} error: {}", m_asset_dir.string(), err.what());
            return false;
        }

        std::size_t done = 0;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(m_asset_dir)) {
                if (m_refresh_cancelled.load(std::memory_order_relaxed)) {
                    return false;
                }
                if (!entry.is_regular_file()) {
                    continue;
                }

                String ext = String(entry.path().extension().string().c_str());
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".meta" || !is_importable(ext)) {
                    continue;
                }

                FsPath rel_path = std::filesystem::relative(entry.path(), m_asset_dir);
                String source_path = String(rel_path.generic_string().c_str());
                importSourceFile(entry.path(), source_path, ext);
                ++done;
                if (progress && (done % 8 == 0 || done == total)) {
                    progress(done, total);
                }
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
