// do@Redlive

#include "asset.h"
#include "runtime/core/meta/serializer/serializer.h"

namespace dodoe {

    Bool Asset::saveToSource(const String& absolute_path) const {
        return false;
    }

    Json Asset::serializeMeta() const {
        Json json;
        json["asset_id"] = Serializer::write(m_meta.ref.asset_id);
        json["sub_object_id"] = Serializer::write(m_meta.ref.local_id);
        json["source_file"] = Serializer::write(m_meta.source_file);
        json["type"] = assetTypeToString(m_meta.type);
        json["name"] = m_meta.name;
        json["source_path"] = m_meta.source_path;
        json["source_file_mtime"] = m_meta.source_file_mtime;
        json["asset_file_mtime"] = m_meta.asset_file_mtime;
        json["import_signature"] = m_meta.import_signature;

        Json tags = Json::array();
        for (const auto& tag : m_meta.tags) {
            tags.push_back(tag);
        }
        json["tags"] = tags;

        Json deps = Json::array();
        for (const auto& dep : m_meta.dependencies) {
            Json dep_json = Json::object();
            dep_json["asset_id"] = Serializer::write(dep.asset_id);
            dep_json["sub_object_id"] = Serializer::write(dep.local_id);
            deps.push_back(dep_json);
        }
        json["dependencies"] = deps;

        json["is_builtin"] = m_meta.is_builtin;

        return json;
    }

    Bool Asset::deserializeMeta(const Json& json) {
        if (!json.is_object()) {
            return false;
        }

        if (json.contains("asset_id")) {
            m_meta.ref.asset_id = UUID(json["asset_id"].get<UInt64>());
        }
        if (json.contains("sub_object_id")) {
            m_meta.ref.local_id = json["sub_object_id"].get<UInt32>();
        }
        if (json.contains("source_file")) {
            Serializer::read(json["source_file"], m_meta.source_file);
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
        if (json.contains("source_file_mtime")) {
            m_meta.source_file_mtime = json["source_file_mtime"].get<UInt64>();
        }
        if (json.contains("asset_file_mtime")) {
            m_meta.asset_file_mtime = json["asset_file_mtime"].get<UInt64>();
        }
        if (json.contains("import_signature")) {
            m_meta.import_signature = json["import_signature"].get<UInt64>();
        }
        if (json.contains("tags")) {
            for (const auto& tag : json["tags"]) {
                m_meta.tags.push_back(tag.get<String>());
            }
        }
        if (json.contains("dependencies")) {
            for (const auto& dep : json["dependencies"]) {
                ObjectID dep_id;
                if (dep.contains("asset_id")) {
                    dep_id.asset_id = UUID(dep["asset_id"].get<UInt64>());
                }
                if (dep.contains("sub_object_id")) {
                    dep_id.local_id = dep["sub_object_id"].get<UInt32>();
                }
                m_meta.dependencies.push_back(dep_id);
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
            case AssetType::Sprite:          return "Sprite";
            case AssetType::Mesh:           return "Mesh";
            case AssetType::Material:        return "Material";
            case AssetType::Anim2DClip:      return "Anim2DClip";
            case AssetType::AnimatorController: return "AnimatorController";
            case AssetType::Scene:           return "Scene";
            case AssetType::Shader:          return "Shader";
            case AssetType::Script:          return "Script";
            case AssetType::Tileset:         return "Tileset";
            case AssetType::Prefab:          return "Prefab";
            case AssetType::Audio:           return "Audio";
            case AssetType::InputAction:     return "InputAction";
            case AssetType::TiledMap:        return "TiledMap";
            case AssetType::Unknown:
            default:                         return "Unknown";
        }
    }

    AssetType Asset::assetTypeFromString(const String& str) {
        if (str == "Texture")        return AssetType::Texture;
        if (str == "Sprite")          return AssetType::Sprite;
        if (str == "Mesh")           return AssetType::Mesh;
        if (str == "Material")        return AssetType::Material;
        if (str == "AnimationClip")   return AssetType::Anim2DClip;
        if (str == "Anim2DClip")      return AssetType::Anim2DClip;
        if (str == "AnimatorController") return AssetType::AnimatorController;
        if (str == "Scene")           return AssetType::Scene;
        if (str == "Shader")          return AssetType::Shader;
        if (str == "Script")          return AssetType::Script;
        if (str == "Tileset")         return AssetType::Tileset;
        if (str == "Prefab")          return AssetType::Prefab;
        if (str == "Audio")           return AssetType::Audio;
        if (str == "InputAction")     return AssetType::InputAction;
        if (str == "TiledMap")        return AssetType::TiledMap;
        return AssetType::Unknown;
    }

    const char* Asset::assetTypeToExtension(AssetType type) {
        switch (type) {
            case AssetType::Texture:
            case AssetType::Sprite:          return ".png";
            case AssetType::Mesh:           return ".obj";
            case AssetType::Material:        return ".domat";
            case AssetType::Anim2DClip:      return ".doaniclip";
            case AssetType::AnimatorController: return ".doanim";
            case AssetType::Scene:           return ".doscn";
            case AssetType::Shader:          return ".shader";
            case AssetType::Script:          return ".cs";
            case AssetType::Tileset:         return ".tsx";
            case AssetType::Prefab:          return ".prefab";
            case AssetType::Audio:           return ".wav";
            case AssetType::InputAction:     return ".doinput";
            case AssetType::Unknown:
            default:                         return ".asset";
        }
    }

    Bool Asset::assetTypeIsReadOnly(AssetType type) {
        switch (type) {
            case AssetType::Texture:
            case AssetType::Sprite:
            case AssetType::Mesh:
            case AssetType::Shader:
            case AssetType::Script:
            case AssetType::Audio:
                return true;
            case AssetType::Material:
            case AssetType::Anim2DClip:
            case AssetType::AnimatorController:
            case AssetType::Scene:
            case AssetType::Tileset:
            case AssetType::Prefab:
            case AssetType::InputAction:
                return false;
            case AssetType::Unknown:
            default:
                return true;
        }
    }

} // dodoe
