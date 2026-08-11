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

        if (json.contains("uuid") && json["uuid"].is_number_unsigned()) {
            out.uuid = UUID(json["uuid"].get<UInt64>());
        }
        if (json.contains("importer") && json["importer"].is_string()) {
            out.importer = json["importer"].get<String>();
        }
        if (json.contains("settings") && json["settings"].is_object()) {
            out.settings = json["settings"];
        }

        return true;
    }

    Bool ImportSettingsIO::Save(const FsPath& absolute_source_path, const ImportSettings& settings) {
        const FsPath meta_path(absolute_source_path.string() + kMetaSuffix);

        Json json;
        json["uuid"] = static_cast<UInt64>(settings.uuid);
        json["importer"] = string_to_std(settings.importer);
        json["settings"] = settings.settings;

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
        result.uuid = GenerateGuid();
        result.importer = default_importer;
        result.settings = default_settings;

        if (Load(absolute_source_path, result)) {
            if (!result.uuid.isValid()) {
                result.uuid = GenerateGuid();
            }
            if (result.importer.empty()) {
                result.importer = default_importer;
            }
            if (result.settings.is_null()) {
                result.settings = default_settings;
            }
            return result;
        }

        Save(absolute_source_path, result);
        return result;
    }

    UUID ImportSettingsIO::GenerateGuid() {
        return UUID::Generate();
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
