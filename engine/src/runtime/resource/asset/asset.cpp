// do@Redlive

#include "asset.h"
#include "runtime/core/meta/serializer/serializer.h"

namespace dodoe {

    Bool Asset::saveToSource(const String& absolute_path) const {
        return false;
    }

    Json Asset::serializeMeta() const {
        Json json;
        json["file_id"] = Serializer::write(m_meta.file_id);
        json["type"] = assetTypeToString(m_meta.type);
        json["name"] = m_meta.name;
        json["source_path"] = m_meta.source_path;
        json["asset_path"] = m_meta.asset_path;
        json["source_file_mtime"] = m_meta.source_file_mtime;
        json["asset_file_mtime"] = m_meta.asset_file_mtime;

        Json import_settings = Json::object();
        for (const auto& [key, value] : m_meta.import_settings) {
            import_settings[key.c_str()] = value;
        }
        json["import_settings"] = import_settings;

        Json tags = Json::array();
        for (const auto& tag : m_meta.tags) {
            tags.push_back(tag);
        }
        json["tags"] = tags;

        Json deps = Json::array();
        for (const auto& dep : m_meta.dependencies) {
            deps.push_back(Serializer::write(dep));
        }
        json["dependencies"] = deps;

        json["is_builtin"] = m_meta.is_builtin;

        return json;
    }

    Bool Asset::deserializeMeta(const Json& json) {
        if (!json.is_object()) {
            return false;
        }

        if (json.contains("file_id")) {
            FileID fid;
            Serializer::read(json["file_id"], fid);
            m_meta.file_id = fid;
        }
        if (json.contains("type")) {
            m_meta.type = assetTypeFromString(json["type"].get<String>());
        }
        if (json.contains("name")) {
            m_meta.name = json["name"].get<String>();
        }
        if (json.contains("source_path")) {
            m_meta.source_path = json["source_path"].get<String>();
        }
        if (json.contains("asset_path")) {
            m_meta.asset_path = json["asset_path"].get<String>();
        }
        if (json.contains("source_file_mtime")) {
            m_meta.source_file_mtime = json["source_file_mtime"].get<UInt64>();
        }
        if (json.contains("asset_file_mtime")) {
            m_meta.asset_file_mtime = json["asset_file_mtime"].get<UInt64>();
        }
        if (json.contains("import_settings")) {
            for (const auto& [key, value] : json["import_settings"].items()) {
                m_meta.import_settings[String(key.c_str())] = value.get<String>();
            }
        }
        if (json.contains("tags")) {
            for (const auto& tag : json["tags"]) {
                m_meta.tags.push_back(tag.get<String>());
            }
        }
        if (json.contains("dependencies")) {
            for (const auto& dep : json["dependencies"]) {
                FileID dep_fid;
                Serializer::read(dep, dep_fid);
                m_meta.dependencies.push_back(dep_fid);
            }
        }
        if (json.contains("is_builtin")) {
            m_meta.is_builtin = json["is_builtin"].get<Bool>();
        }

        return true;
    }

    const char* Asset::assetTypeToString(AssetType type) {
        switch (type) {
            case AssetType::Texture:        return "Texture";
            case AssetType::Mesh:           return "Mesh";
            case AssetType::Material:        return "Material";
            case AssetType::AnimationClip:   return "AnimationClip";
            case AssetType::Scene:           return "Scene";
            case AssetType::Shader:          return "Shader";
            case AssetType::Script:          return "Script";
            case AssetType::Tileset:         return "Tileset";
            case AssetType::Prefab:          return "Prefab";
            case AssetType::Audio:           return "Audio";
            case AssetType::Unknown:
            default:                         return "Unknown";
        }
    }

    AssetType Asset::assetTypeFromString(const String& str) {
        if (str == "Texture")        return AssetType::Texture;
        if (str == "Mesh")           return AssetType::Mesh;
        if (str == "Material")        return AssetType::Material;
        if (str == "AnimationClip")   return AssetType::AnimationClip;
        if (str == "Scene")           return AssetType::Scene;
        if (str == "Shader")          return AssetType::Shader;
        if (str == "Script")          return AssetType::Script;
        if (str == "Tileset")         return AssetType::Tileset;
        if (str == "Prefab")          return AssetType::Prefab;
        if (str == "Audio")           return AssetType::Audio;
        return AssetType::Unknown;
    }

    const char* Asset::assetTypeToExtension(AssetType type) {
        switch (type) {
            case AssetType::Texture:        return ".png";
            case AssetType::Mesh:           return ".obj";
            case AssetType::Material:        return ".domat";
            case AssetType::AnimationClip:   return ".doaniclip";
            case AssetType::Scene:           return ".doscn";
            case AssetType::Shader:          return ".shader";
            case AssetType::Script:          return ".cs";
            case AssetType::Tileset:         return ".tsx";
            case AssetType::Prefab:          return ".prefab";
            case AssetType::Audio:           return ".wav";
            case AssetType::Unknown:
            default:                         return ".asset";
        }
    }

    Bool Asset::assetTypeIsReadOnly(AssetType type) {
        switch (type) {
            case AssetType::Texture:
            case AssetType::Mesh:
            case AssetType::Shader:
            case AssetType::Script:
            case AssetType::Audio:
                return true;
            case AssetType::Material:
            case AssetType::AnimationClip:
            case AssetType::Scene:
            case AssetType::Tileset:
            case AssetType::Prefab:
                return false;
            case AssetType::Unknown:
            default:
                return true;
        }
    }

} // dodoe
