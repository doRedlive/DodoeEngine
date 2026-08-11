// do@Redlive

#include "import_settings_io.h"

#include "runtime/resource/file/file_system.h"

namespace dodoe {

    Bool ImportSettingsIO::Load(const FsPath& absolute_source_path, ImportSettings& out) {
        const FsPath meta_path(absolute_source_path.string() + kMetaSuffix);
        if (!std::filesystem::exists(meta_path)) {
            return false;
        }

        std::ifstream file(meta_path);
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

        if (json.contains("guid") && json["guid"].is_number_unsigned()) {
            out.guid = UUID(json["guid"].get<UInt64>());
        }
        if (json.contains("importer") && json["importer"].is_string()) {
            out.importer = json["importer"].get<String>();
        }
        if (json.contains("settings") && json["settings"].is_object()) {
            out.settings = json["settings"];
        }
        if (json.contains("sprites") && json["sprites"].is_array()) {
            out.sprites.clear();
            for (const auto& sprite_json : json["sprites"]) {
                if (!sprite_json.is_object()) {
                    continue;
                }
                SpriteMeta sprite;
                if (sprite_json.contains("name") && sprite_json["name"].is_string()) {
                    sprite.name = sprite_json["name"].get<String>();
                }
                if (sprite_json.contains("local_id") && sprite_json["local_id"].is_number_unsigned()) {
                    sprite.local_id = sprite_json["local_id"].get<UInt32>();
                }
                if (sprite_json.contains("ppu") && sprite_json["ppu"].is_number()) {
                    sprite.ppu = sprite_json["ppu"].get<Float>();
                }
                if (sprite_json.contains("pivot") && sprite_json["pivot"].is_array()
                    && sprite_json["pivot"].size() >= 2) {
                    sprite.pivot_x = sprite_json["pivot"][0].get<Float>();
                    sprite.pivot_y = sprite_json["pivot"][1].get<Float>();
                }
                if (sprite_json.contains("slice") && sprite_json["slice"].is_array()
                    && sprite_json["slice"].size() >= 4) {
                    sprite.slice_left = sprite_json["slice"][0].get<Float>();
                    sprite.slice_bottom = sprite_json["slice"][1].get<Float>();
                    sprite.slice_right = sprite_json["slice"][2].get<Float>();
                    sprite.slice_top = sprite_json["slice"][3].get<Float>();
                }
                out.sprites.push_back(sprite);
            }
        }

        return true;
    }

    Bool ImportSettingsIO::Save(const FsPath& absolute_source_path, const ImportSettings& settings) {
        const FsPath meta_path(absolute_source_path.string() + kMetaSuffix);

        Json json;
        json["guid"] = static_cast<UInt64>(settings.guid);
        json["importer"] = string_to_std(settings.importer);
        json["settings"] = settings.settings;
        if (!settings.sprites.empty()) {
            Json sprites = Json::array();
            for (const auto& sprite : settings.sprites) {
                Json sprite_json;
                sprite_json["name"] = sprite.name;
                sprite_json["local_id"] = sprite.local_id;
                sprite_json["ppu"] = sprite.ppu;
                sprite_json["pivot"] = Json::array({sprite.pivot_x, sprite.pivot_y});
                sprite_json["slice"] = Json::array({sprite.slice_left, sprite.slice_bottom,
                                                    sprite.slice_right, sprite.slice_top});
                sprites.push_back(sprite_json);
            }
            json["sprites"] = sprites;
        }

        std::ofstream file(meta_path);
        if (!file.is_open()) {
            return false;
        }

        file << json.dump(4);
        file.flush();
        return true;
    }

    ImportSettings ImportSettingsIO::LoadOrCreate(const FsPath& absolute_source_path,
                                                  const String& source_path,
                                                  const String& default_importer,
                                                  const Json& default_settings) {
        ImportSettings result;
        result.importer = default_importer;
        result.settings = default_settings;

        if (Load(absolute_source_path, result)) {
            if (!result.guid.isValid()) {
                result.guid = UUID::Generate();
            }
            if (result.importer.empty()) {
                result.importer = default_importer;
            }
            if (result.settings.is_null()) {
                result.settings = default_settings;
            }
            return result;
        }

        result.guid = UUID::Generate();
        Save(absolute_source_path, result);
        return result;
    }

    UInt64 ImportSettingsIO::LastWriteTimeSeconds(const FsPath& path) {
        std::error_code ec;
        const auto ftime = std::filesystem::last_write_time(path, ec);
        if (ec) {
            return 0;
        }
        return static_cast<UInt64>(
            std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count());
    }

} // dodoe
