// do@Redlive

#include "tileset_asset.h"

#include "runtime/core/utils/json.h"

#include <fstream>
#include <sstream>

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
        if (json.contains("ImagePath")) {
            m_image_path = json["ImagePath"].get<String>();
        }
        if (json.contains("TextureId")) {
            m_texture_id = json["TextureId"].get<UInt32>();
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
        json["ImagePath"] = m_image_path;
        json["TextureId"] = m_texture_id;

        file << json.dump(4);
        file.flush();
        return true;
    }

} // dodoe
