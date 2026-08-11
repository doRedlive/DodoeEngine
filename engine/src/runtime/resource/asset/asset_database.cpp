// do@Redlive

#include "asset_database.h"
#include "runtime/core/meta/serializer/serializer.h"

namespace dodoe {

    AssetDatabase::AssetDatabase(const FsPath& project_asset_dir)
        : m_database_path(project_asset_dir / "asset_database.json") {}

    AssetDatabase::~AssetDatabase() {
        if (m_dirty) {
            save();
        }
    }

    Bool AssetDatabase::load() {
        if (!std::filesystem::exists(m_database_path)) {
            return true;
        }

        std::ifstream file(m_database_path);
        if (!file.is_open()) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        Json json;
        try {
            json = Json::parse(buffer.str());
        } catch (const Json::exception&) {
            return false;
        }

        if (!json.contains("assets") || !json["assets"].is_object()) {
            return false;
        }

        m_metadata_cache.clear();

        for (const auto& [key_str, asset_json] : json["assets"].items()) {
            FileID file_id;

            if (asset_json.contains("file_id")) {
                Serializer::read(asset_json["file_id"], file_id);
            } else if (asset_json.contains("source_path")) {
                String sp = asset_json["source_path"].get<String>();
                file_id = FileID(sp);
            }

            if (!file_id.isValid()) {
                continue;
            }

            AssetMetaData meta;
            meta.file_id = file_id;

            if (asset_json.contains("type")) {
                meta.type = Asset::AssetTypeFromString(asset_json["type"].get<String>());
            }
            if (asset_json.contains("name")) {
                meta.name = asset_json["name"].get<String>();
            }
            if (asset_json.contains("source_path")) {
                meta.source_path = asset_json["source_path"].get<String>();
            }
            if (asset_json.contains("source_file_mtime")) {
                meta.source_file_mtime = asset_json["source_file_mtime"].get<UInt64>();
            }
            if (asset_json.contains("asset_file_mtime")) {
                meta.asset_file_mtime = asset_json["asset_file_mtime"].get<UInt64>();
            }
            if (asset_json.contains("import_signature")) {
                meta.import_signature = asset_json["import_signature"].get<UInt64>();
            }
            if (asset_json.contains("is_builtin")) {
                meta.is_builtin = asset_json["is_builtin"].get<Bool>();
            }
            if (asset_json.contains("tags")) {
                for (const auto& tag : asset_json["tags"]) {
                    meta.tags.push_back(tag.get<String>());
                }
            }
            if (asset_json.contains("dependencies")) {
                for (const auto& dep : asset_json["dependencies"]) {
                    UUID dep_uuid;
                    Serializer::read(dep, dep_uuid);
                    meta.dependencies.push_back(dep_uuid);
                }
            }

            m_metadata_cache[file_id] = std::move(meta);
        }

        m_dirty = false;
        return true;
    }

    Bool AssetDatabase::save() {
        std::error_code ec;
        std::filesystem::create_directories(m_database_path.parent_path(), ec);

        std::ofstream file(m_database_path);
        if (!file.is_open()) {
            return false;
        }

        Json root;
        root["version"] = 1;

        Json assets = Json::object();
        for (const auto& [file_id, meta] : m_metadata_cache) {
            Json asset_json;
            asset_json["file_id"] = Serializer::write(file_id);
            asset_json["type"] = Asset::AssetTypeToString(meta.type);
            asset_json["name"] = meta.name;
            asset_json["source_path"] = meta.source_path;
            asset_json["source_file_mtime"] = meta.source_file_mtime;
            asset_json["asset_file_mtime"] = meta.asset_file_mtime;
            asset_json["import_signature"] = meta.import_signature;
            asset_json["is_builtin"] = meta.is_builtin;

            Json tags = Json::array();
            for (const auto& tag : meta.tags) {
                tags.push_back(tag);
            }
            asset_json["tags"] = tags;

            Json deps = Json::array();
            for (const auto& dep : meta.dependencies) {
                deps.push_back(Serializer::write(dep));
            }
            asset_json["dependencies"] = deps;

            String key_str(std::to_string(file_id.getID()).c_str());
            assets[key_str.c_str()] = asset_json;
        }

        root["assets"] = assets;

        file << root.dump(4);
        file.flush();
        m_dirty = false;
        return true;
    }

    Bool AssetDatabase::hasAsset(const FileID& file_id) const {
        return m_metadata_cache.find(file_id) != m_metadata_cache.end();
    }

    AssetMetaData AssetDatabase::getMetaData(const FileID& file_id) const {
        auto it = m_metadata_cache.find(file_id);
        if (it != m_metadata_cache.end()) {
            return it->second;
        }
        return {};
    }

    void AssetDatabase::setMetaData(const FileID& file_id, const AssetMetaData& meta) {
        m_metadata_cache[file_id] = meta;
        m_dirty = true;
    }

    void AssetDatabase::removeAsset(const FileID& file_id) {
        m_metadata_cache.erase(file_id);
        m_dirty = true;
    }

    DynamicArray<FileID> AssetDatabase::getAllAssetFileIDs() const {
        DynamicArray<FileID> result;
        result.reserve(m_metadata_cache.size());
        for (const auto& [file_id, meta] : m_metadata_cache) {
            result.push_back(file_id);
        }
        return result;
    }

    DynamicArray<FileID> AssetDatabase::getAssetsOfType(AssetType type) const {
        DynamicArray<FileID> result;
        for (const auto& [file_id, meta] : m_metadata_cache) {
            if (meta.type == type) {
                result.push_back(file_id);
            }
        }
        return result;
    }

    DynamicArray<FileID> AssetDatabase::getAssetsByTag(const String& tag) const {
        DynamicArray<FileID> result;
        for (const auto& [file_id, meta] : m_metadata_cache) {
            for (const auto& t : meta.tags) {
                if (t == tag) {
                    result.push_back(file_id);
                    break;
                }
            }
        }
        return result;
    }

    UUID AssetDatabase::generateUUID() {
        return UUID();
    }

} // dodoe
