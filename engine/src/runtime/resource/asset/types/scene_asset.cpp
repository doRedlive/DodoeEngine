// do@Redlive

#include "scene_asset.h"

#include "runtime/core/meta/serializer/serializer.h"

namespace dodoe {

    Bool SceneAsset::loadFromSource(const String& absolute_source_path) {
        std::ifstream file(absolute_source_path);
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

        Serializer::read(json, m_scene_res);
        m_meta.source_path = absolute_source_path;
        return true;
    }

    void SceneAsset::unloadRuntime() {
        m_scene_res = SceneRes{};
    }

    Bool SceneAsset::saveToSource(const String& absolute_path) const {
        std::ofstream file(absolute_path);
        if (!file.is_open()) {
            return false;
        }

        Json json = Serializer::write(m_scene_res);
        file << json.dump(4);
        file.flush();
        return true;
    }

} // dodoe
