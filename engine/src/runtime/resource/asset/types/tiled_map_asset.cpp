// do@Redlive

#include "tiled_map_asset.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace dodoe {

    namespace {

        constexpr UInt32 kFlipHorizontal = 0x80000000u;
        constexpr UInt32 kFlipVertical = 0x40000000u;
        constexpr UInt32 kFlipDiagonal = 0x20000000u;
        constexpr UInt32 kFlipMask = kFlipHorizontal | kFlipVertical | kFlipDiagonal;

        Bool ParseUInt32(const Json& json, const char* key, UInt32& out) {
            if (json.contains(key) && json[key].is_number()) {
                out = json[key].get<UInt32>();
                return true;
            }
            return false;
        }

        Bool ParseInt32(const Json& json, const char* key, Int32& out) {
            if (json.contains(key) && json[key].is_number()) {
                out = json[key].get<Int32>();
                return true;
            }
            return false;
        }

        Bool ParseFloat(const Json& json, const char* key, Float& out) {
            if (json.contains(key) && json[key].is_number()) {
                out = json[key].get<Float>();
                return true;
            }
            return false;
        }

        Bool Base64Decode(const String& input, DynamicArray<unsigned char>& out) {
            static constexpr char kTable[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            unsigned char buf[4] = {0, 0, 0, 0};
            int bufLen = 0;
            for (Size_t i = 0; i < input.size(); ++i) {
                const char c = input[i];
                if (c == '=') {
                    break;
                }
                const char* pos = std::strchr(kTable, c);
                if (!pos) {
                    continue;
                }
                buf[bufLen++] = static_cast<unsigned char>(pos - kTable);
                if (bufLen == 4) {
                    out.push_back(static_cast<unsigned char>((buf[0] << 2) | (buf[1] >> 4)));
                    out.push_back(static_cast<unsigned char>(((buf[1] & 0x0F) << 4) | (buf[2] >> 2)));
                    out.push_back(static_cast<unsigned char>(((buf[2] & 0x03) << 6) | buf[3]));
                    bufLen = 0;
                }
            }
            if (bufLen >= 2) {
                out.push_back(static_cast<unsigned char>((buf[0] << 2) | (buf[1] >> 4)));
                if (bufLen >= 3) {
                    out.push_back(static_cast<unsigned char>(((buf[1] & 0x0F) << 4) | (buf[2] >> 2)));
                }
            }
            return true;
        }

        Bool ParseBase64Layer(const Json& layer, DynamicArray<UInt32>& tiles) {
            if (!layer.contains("data") || !layer["data"].is_string()) {
                return false;
            }
            const String encoding = layer.value("encoding", String());
            if (encoding != "base64") {
                return false;
            }
            const String compression = layer.value("compression", String());
            if (!compression.empty()) {
                return false;
            }
            DynamicArray<unsigned char> bytes;
            Base64Decode(layer["data"].get<String>(), bytes);
            tiles.reserve(bytes.size() / sizeof(UInt32));
            for (Size_t i = 0; i + 3 < bytes.size(); i += 4) {
                const UInt32 gid = static_cast<UInt32>(bytes[i]) |
                    (static_cast<UInt32>(bytes[i + 1]) << 8) |
                    (static_cast<UInt32>(bytes[i + 2]) << 16) |
                    (static_cast<UInt32>(bytes[i + 3]) << 24);
                tiles.push_back(gid & ~kFlipMask);
            }
            return true;
        }

        std::unordered_map<String, String> ExtractXmlTagAttrs(const String& content, const char* tag) {
            std::unordered_map<String, String> attrs;
            const String open = String("<") + tag;
            const Size_t start = content.find(open);
            if (start == String::npos) {
                return attrs;
            }
            Size_t pos = start + open.size();
            while (pos < content.size()) {
                while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t' ||
                                                content[pos] == '\n' || content[pos] == '\r')) {
                    ++pos;
                }
                if (pos >= content.size() || content[pos] == '>' || content[pos] == '/') {
                    break;
                }
                const Size_t keyStart = pos;
                while (pos < content.size() && content[pos] != '=' && content[pos] != ' ' &&
                       content[pos] != '\t' && content[pos] != '\n' && content[pos] != '\r' &&
                       content[pos] != '>' && content[pos] != '/') {
                    ++pos;
                }
                const String key = content.substr(keyStart, pos - keyStart);
                while (pos < content.size() && content[pos] != '=' && content[pos] != '>') {
                    ++pos;
                }
                if (pos >= content.size() || content[pos] != '=') {
                    continue;
                }
                ++pos;
                while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) {
                    ++pos;
                }
                if (pos < content.size() && content[pos] == '"') {
                    ++pos;
                    const Size_t valueStart = pos;
                    while (pos < content.size() && content[pos] != '"') {
                        ++pos;
                    }
                    attrs[key] = content.substr(valueStart, pos - valueStart);
                    if (pos < content.size()) {
                        ++pos;
                    }
                }
            }
            return attrs;
        }

        Bool LoadJsonTileset(const FsPath& absolute_path, TiledMapTilesetData& data) {
            std::ifstream file(absolute_path);
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
            if (!json.is_object()) {
                return false;
            }
            if (json.contains("name") && json["name"].is_string()) {
                data.name = json["name"].get<String>();
            }
            ParseUInt32(json, "tilewidth", data.tile_width);
            ParseUInt32(json, "tileheight", data.tile_height);
            ParseUInt32(json, "columns", data.columns);
            ParseUInt32(json, "tilecount", data.tile_count);
            ParseUInt32(json, "margin", data.margin);
            ParseUInt32(json, "spacing", data.spacing);
            if (json.contains("image") && json["image"].is_string()) {
                data.image_path = json["image"].get<String>();
            }
            return true;
        }

        Bool LoadXmlTileset(const FsPath& absolute_path, TiledMapTilesetData& data) {
            std::ifstream file(absolute_path);
            if (!file.is_open()) {
                return false;
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            const String content = String(buffer.str().c_str());

            const auto tilesetAttrs = ExtractXmlTagAttrs(content, "tileset");
            if (tilesetAttrs.empty()) {
                return false;
            }
            auto it = tilesetAttrs.find("name");
            if (it != tilesetAttrs.end()) {
                data.name = it->second;
            }
            it = tilesetAttrs.find("tilewidth");
            if (it != tilesetAttrs.end()) {
                data.tile_width = static_cast<UInt32>(std::strtoul(it->second.c_str(), nullptr, 10));
            }
            it = tilesetAttrs.find("tileheight");
            if (it != tilesetAttrs.end()) {
                data.tile_height = static_cast<UInt32>(std::strtoul(it->second.c_str(), nullptr, 10));
            }
            it = tilesetAttrs.find("columns");
            if (it != tilesetAttrs.end()) {
                data.columns = static_cast<UInt32>(std::strtoul(it->second.c_str(), nullptr, 10));
            }
            it = tilesetAttrs.find("tilecount");
            if (it != tilesetAttrs.end()) {
                data.tile_count = static_cast<UInt32>(std::strtoul(it->second.c_str(), nullptr, 10));
            }

            const auto imageAttrs = ExtractXmlTagAttrs(content, "image");
            it = imageAttrs.find("source");
            if (it != imageAttrs.end()) {
                data.image_path = it->second;
            }
            return true;
        }

        void FillEmbeddedTileset(const Json& ts, TiledMapTilesetData& data) {
            if (ts.contains("name") && ts["name"].is_string()) {
                data.name = ts["name"].get<String>();
            }
            ParseUInt32(ts, "tilewidth", data.tile_width);
            ParseUInt32(ts, "tileheight", data.tile_height);
            ParseUInt32(ts, "columns", data.columns);
            ParseUInt32(ts, "tilecount", data.tile_count);
            ParseUInt32(ts, "margin", data.margin);
            ParseUInt32(ts, "spacing", data.spacing);
            if (ts.contains("image") && ts["image"].is_string()) {
                data.image_path = ts["image"].get<String>();
            }
        }

        Bool LoadExternalTileset(const FsPath& absolute_path, TiledMapTilesetData& data) {
            const String ext(absolute_path.extension().string().c_str());
            if (ext == ".tsj" || ext == ".json") {
                return LoadJsonTileset(absolute_path, data);
            }
            if (ext == ".tsx") {
                return LoadXmlTileset(absolute_path, data);
            }
            return false;
        }

    } // namespace

    Bool TiledMapAsset::loadFromSource(const String& absolute_source_path) {
        m_tilesets.clear();
        m_layers.clear();

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
        if (!json.is_object()) {
            return false;
        }

        ParseUInt32(json, "width", m_map_width);
        ParseUInt32(json, "height", m_map_height);
        ParseUInt32(json, "tilewidth", m_tile_width);
        ParseUInt32(json, "tileheight", m_tile_height);
        if (json.contains("orientation") && json["orientation"].is_string()) {
            m_orientation = json["orientation"].get<String>();
        }

        const FsPath base_dir = FsPath(absolute_source_path.c_str()).parent_path();

        if (json.contains("tilesets") && json["tilesets"].is_array()) {
            for (const auto& ts : json["tilesets"]) {
                if (!ts.is_object()) {
                    continue;
                }
                TiledMapTilesetData data;
                ParseUInt32(ts, "firstgid", data.first_gid);
                if (ts.contains("source") && ts["source"].is_string()) {
                    const String source = ts["source"].get<String>();
                    const FsPath external_path = base_dir / FsPath(source.c_str());
                    if (!LoadExternalTileset(external_path, data)) {
                        continue;
                    }
                    if (!data.image_path.empty()) {
                        const FsPath img = external_path.parent_path() / FsPath(data.image_path.c_str());
                        data.image_path = String(img.lexically_relative(base_dir).generic_string().c_str());
                    }
                } else {
                    FillEmbeddedTileset(ts, data);
                }
                m_tilesets.push_back(std::move(data));
            }
        }

        if (json.contains("layers") && json["layers"].is_array()) {
            for (const auto& layer : json["layers"]) {
                if (!layer.is_object()) {
                    continue;
                }
                const String type = layer.value("type", String("tilelayer"));
                if (type != "tilelayer") {
                    continue;
                }
                TiledMapLayerData data;
                if (layer.contains("name") && layer["name"].is_string()) {
                    data.name = layer["name"].get<String>();
                }
                ParseUInt32(layer, "width", data.width);
                ParseUInt32(layer, "height", data.height);
                if (layer.contains("visible") && layer["visible"].is_boolean()) {
                    data.visible = layer["visible"].get<Bool>();
                }
                ParseFloat(layer, "opacity", data.opacity);
                ParseInt32(layer, "offsetx", data.offset_x);
                ParseInt32(layer, "offsety", data.offset_y);
                if (layer.contains("data") && layer["data"].is_array()) {
                    data.tiles.reserve(layer["data"].size());
                    for (const auto& gid : layer["data"]) {
                        if (gid.is_number_unsigned()) {
                            data.tiles.push_back(gid.get<UInt32>() & ~kFlipMask);
                        } else {
                            data.tiles.push_back(0);
                        }
                    }
                } else if (layer.contains("data") && layer["data"].is_string()) {
                    if (!ParseBase64Layer(layer, data.tiles)) {
                        continue;
                    }
                }
                m_layers.push_back(std::move(data));
            }
        }

        m_meta.source_path = absolute_source_path;
        return true;
    }

    void TiledMapAsset::unloadRuntime() {
        m_tilesets.clear();
        m_layers.clear();
    }

} // dodoe
