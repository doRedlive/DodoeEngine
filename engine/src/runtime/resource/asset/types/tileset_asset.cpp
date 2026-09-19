// do@Redlive

#include "tileset_asset.h"

#include "runtime/core/utils/json.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace dodoe {

    Bool TilesetAsset::loadFromSource(const String& absolute_source_path) {
        std::ifstream file(absolute_source_path.c_str());
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
            m_name = json["Name"].get<String>();
        }
        if (json.contains("FirstGid")) {
            m_first_gid = json["FirstGid"].get<UInt32>();
        }
        if (json.contains("TileWidth")) {
            m_tile_width = json["TileWidth"].get<UInt32>();
        }
        if (json.contains("TileHeight")) {
            m_tile_height = json["TileHeight"].get<UInt32>();
        }
        if (json.contains("Columns")) {
            m_columns = json["Columns"].get<UInt32>();
        }
        if (json.contains("TileCount")) {
            m_tile_count = json["TileCount"].get<UInt32>();
        }
        if (json.contains("Margin")) {
            m_margin = json["Margin"].get<UInt32>();
        }
        if (json.contains("Spacing")) {
            m_spacing = json["Spacing"].get<UInt32>();
        }
        if (json.contains("ImagePath")) {
            m_image_path = json["ImagePath"].get<String>();
        }
        if (json.contains("TextureId")) {
            m_texture_id = json["TextureId"].get<UInt32>();
        }
        if (json.contains("TileProperties") && json["TileProperties"].is_object()) {
            for (auto tileIt = json["TileProperties"].begin(); tileIt != json["TileProperties"].end(); ++tileIt) {
                if (!tileIt.value().is_object()) continue;
                char* end = nullptr;
                const unsigned long localId = std::strtoul(tileIt.key().c_str(), &end, 10);
                if (!end || *end != '\0') continue;
                UnorderedMap<String, String> properties;
                for (auto propIt = tileIt.value().begin(); propIt != tileIt.value().end(); ++propIt) {
                    if (propIt.value().is_string()) {
                        properties.emplace(String(propIt.key().c_str()), propIt.value().get<String>());
                    }
                }
                if (!properties.empty()) {
                    m_tile_properties.emplace(static_cast<UInt32>(localId), std::move(properties));
                }
            }
        }

        m_meta.source_path = absolute_source_path;
        return true;
    }

    void TilesetAsset::unloadRuntime() {
        m_image_path.clear();
        m_name.clear();
    }

    Bool TilesetAsset::saveToSource(const String& absolute_path) const {
        std::ofstream file(absolute_path.c_str());
        if (!file.is_open()) {
            return false;
        }

        Json json;
        json["Name"] = m_name;
        json["FirstGid"] = m_first_gid;
        json["TileWidth"] = m_tile_width;
        json["TileHeight"] = m_tile_height;
        json["Columns"] = m_columns;
        json["TileCount"] = m_tile_count;
        json["Margin"] = m_margin;
        json["Spacing"] = m_spacing;
        json["ImagePath"] = m_image_path;
        json["TextureId"] = m_texture_id;
        if (!m_tile_properties.empty()) {
            Json properties = Json::object();
            for (const auto& [localId, props] : m_tile_properties) {
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

} // dodoe
