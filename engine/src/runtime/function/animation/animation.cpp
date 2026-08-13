//
// Created by Redlive on 2026/3/23.
//

#include "animation.h"

#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/utils/common.h"
#include "runtime/core/utils/json.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/resource_manager.h"

#include <fstream>
#include <sstream>

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<AnimationClip>> s_animation_clip_cache{};

    }

    Bool AnimationClip::loadFromJson(const String& absolute_path) {
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

        if (json.contains("frames")) {
            Serializer::read(json["frames"], m_frames);
        }
        if (json.contains("loop")) {
            Serializer::read(json["loop"], m_loop);
        }
        if (json.contains("frame_ms")) {
            Serializer::read(json["frame_ms"], m_frame_ms);
        }

        return true;
    }

    Bool AnimationClip::saveToJson(const String& absolute_path) const {
        std::ofstream file(absolute_path.c_str());
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

    AnimationClip* AnimationClip::Create(const ObjectID& ref, const String& path) {
        if (!ref.isValid() || path.empty()) {
            return nullptr;
        }

        String absolute_path = path;
        if (auto* asset_manager = ResourceManager::Self().getAssetManager()) {
            if (FsPath(path.c_str()).is_relative()) {
                absolute_path = String((asset_manager->getAssetDir() / FsPath(path.c_str())).generic_string().c_str());
            }
        }

        auto clip = create_scope<AnimationClip>(ref);
        AnimationClip* raw = clip.get();
        raw->setPath(absolute_path);
        raw->loadFromJson(absolute_path);
        const InstanceID instance_id = raw->getInstanceID();
        s_animation_clip_cache.emplace(instance_id, std::move(clip));
        return raw;
    }

    void AnimationClip::Shutdown() {
        s_animation_clip_cache.clear();
    }

} // dodoe
