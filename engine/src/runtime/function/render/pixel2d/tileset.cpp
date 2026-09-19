// do@Redlive

#include "tileset.h"

#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/utils/json.h"
#include "runtime/resource/asset/asset_database.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<Tileset>> s_tileset_cache{};

    } // namespace

    Bool Tileset::loadFromJson(const String& absolute_path) {
        std::ifstream file(absolute_path.c_str());
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

        if (json.contains("Name")) {
            name = json["Name"].get<String>();
        }
        if (json.contains("FirstGid")) {
            first_gid = json["FirstGid"].get<UInt32>();
        }
        if (json.contains("TileWidth")) {
            tile_width = json["TileWidth"].get<UInt32>();
        }
        if (json.contains("TileHeight")) {
            tile_height = json["TileHeight"].get<UInt32>();
        }
        if (json.contains("Columns")) {
            columns = json["Columns"].get<UInt32>();
        }
        if (json.contains("TileCount")) {
            tile_count = json["TileCount"].get<UInt32>();
        }
        if (json.contains("Margin")) {
            margin = json["Margin"].get<UInt32>();
        }
        if (json.contains("Spacing")) {
            spacing = json["Spacing"].get<UInt32>();
        }
        if (json.contains("ImagePath")) {
            image_path = json["ImagePath"].get<String>();
        }
        if (json.contains("TextureId")) {
            texture_id = json["TextureId"].get<UInt32>();
        }
        if (json.contains("TileProperties") && json["TileProperties"].is_object()) {
            for (auto tile_it = json["TileProperties"].begin(); tile_it != json["TileProperties"].end(); ++tile_it) {
                if (!tile_it.value().is_object()) continue;
                char* end = nullptr;
                const unsigned long localId = std::strtoul(tile_it.key().c_str(), &end, 10);
                if (!end || *end != '\0') continue;
                UnorderedMap<String, String> properties;
                for (auto prop_it = tile_it.value().begin(); prop_it != tile_it.value().end(); ++prop_it) {
                    if (prop_it.value().is_string()) {
                        properties.emplace(String(prop_it.key().c_str()), prop_it.value().get<String>());
                    }
                }
                if (!properties.empty()) {
                    tile_properties.emplace(static_cast<UInt32>(localId), std::move(properties));
                }
            }
        }

        return true;
    }

    Bool Tileset::saveToJson(const String& absolute_path) const {
        std::ofstream file(absolute_path.c_str());
        if (!file.is_open()) {
            return false;
        }

        Json json;
        json["Name"] = name;
        json["FirstGid"] = first_gid;
        json["TileWidth"] = tile_width;
        json["TileHeight"] = tile_height;
        json["Columns"] = columns;
        json["TileCount"] = tile_count;
        json["Margin"] = margin;
        json["Spacing"] = spacing;
        json["ImagePath"] = image_path;
        json["TextureId"] = texture_id;
        if (!tile_properties.empty()) {
            Json properties = Json::object();
            for (const auto& [localId, props] : tile_properties) {
                Json entry = Json::object();
                for (const auto& [key, value] : props) {
                    entry[std::string(key.c_str())] = value;
                }
                properties[std::to_string(localId)] = std::move(entry);
            }
            json["TileProperties"] = std::move(properties);
        }

        file << json.dump(4);
        file.flush();
        return true;
    }

    Tileset* Tileset::Create(const ObjectID& id, const String& path) {
        if (!id.isValid()) {
            return nullptr;
        }
        if (const InstanceID existing = Object::FindInstanceID(id); existing != 0) {
            if (auto* obj = Object::FindObjectFromInstanceID(existing)) {
                return static_cast<Tileset*>(obj);
            }
        }

        auto tileset = create_scope<Tileset>(id);
        Tileset* raw = tileset.get();
        if (!path.empty()) {
            (void)raw->loadFromJson(path);
        }
        s_tileset_cache.emplace(raw->getInstanceID(), std::move(tileset));
        return raw;
    }

    Tileset* Tileset::CreateTransient() {
        const ObjectID id{AssetDatabase::generateUUID(), 0};
        auto tileset = create_scope<Tileset>(id);
        Tileset* raw = tileset.get();
        s_tileset_cache.emplace(raw->getInstanceID(), std::move(tileset));
        return raw;
    }

    void Tileset::Shutdown() {
        s_tileset_cache.clear();
    }

} // dodoe
