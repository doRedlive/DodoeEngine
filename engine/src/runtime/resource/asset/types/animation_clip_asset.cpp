// do@Redlive

#include "animation_clip_asset.h"

#include "runtime/core/meta/serializer/serializer.h"

namespace dodoe {

    Bool AnimationClipAsset::loadFromSource(const String& absolute_source_path) {
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

        if (json.contains("frames")) {
            Serializer::read(json["frames"], m_frames);
        }
        if (json.contains("loop")) {
            Serializer::read(json["loop"], m_loop);
        }
        if (json.contains("frame_ms")) {
            Serializer::read(json["frame_ms"], m_frame_ms);
        }

        m_meta.source_path = absolute_source_path;
        return true;
    }

    void AnimationClipAsset::unloadRuntime() {
        m_frames.clear();
    }

    Bool AnimationClipAsset::saveToSource(const String& absolute_path) const {
        std::ofstream file(absolute_path);
        if (!file.is_open()) {
            return false;
        }

        Json json;
        json["frames"] = Serializer::write(m_frames);
        json["loop"] = Serializer::write(m_loop);
        json["frame_ms"] = Serializer::write(m_frame_ms);

        file << json.dump(4);
        file.flush();
        return true;
    }

} // dodoe
