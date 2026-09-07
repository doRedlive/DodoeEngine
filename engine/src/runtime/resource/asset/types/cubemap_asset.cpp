// do@Redlive

#include "cubemap_asset.h"

#include "runtime/core/meta/serializer/serializer.h"

namespace dodoe {

    Bool CubemapAsset::loadFromSource(const String& absolute_source_path) {
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

        if (json.contains("facePaths") && json["facePaths"].is_array()) {
            m_face_paths.clear();
            for (const auto& path : json["facePaths"]) {
                if (path.is_string()) {
                    m_face_paths.push_back(String(path.get<std::string>().c_str()));
                }
            }
        }

        m_meta.source_path = absolute_source_path;
        return true;
    }

    void CubemapAsset::unloadRuntime() {
    }

    Bool CubemapAsset::saveToSource(const String& absolute_path) const {
        std::ofstream file(absolute_path.c_str());
        if (!file.is_open()) {
            return false;
        }

        Json json = Json::array();
        for (const auto& path : m_face_paths) {
            json.push_back(Serializer::write(path));
        }
        Json out = Json::object();
        out["facePaths"] = json;

        file << out.dump(4);
        file.flush();
        return true;
    }

} // dodoe
