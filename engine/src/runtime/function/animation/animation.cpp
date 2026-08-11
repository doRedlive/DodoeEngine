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

        constexpr UInt64 kAnimationClipUuidSalt = 0x5555A5A5AAAA5A5AULL;

        UnorderedMap<InstanceID, Scope<AnimationClip>> s_animation_clip_cache{};

        ObjectID MakeDefaultAnimationClipObjectID(const String& path) {
            return ObjectID{UUID(static_cast<UInt64>(string2hash(path)) ^ kAnimationClipUuidSalt), 0};
        }

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

    AnimationClip* AnimationClip::Load(const String& path) {
        if (path.empty()) {
            return nullptr;
        }

        ObjectID id;
        if (auto* asset_manager = ResourceManager::Self().getAssetManager()) {
            id = asset_manager->resolvePathToRef(FileID(path));
        }
        if (!id.isValid()) {
            id = MakeDefaultAnimationClipObjectID(path);
        }

        const InstanceID existing = Object::FindInstanceID(id);
        if (existing != 0) {
            return static_cast<AnimationClip*>(Object::FindObjectFromInstanceID(existing));
        }

        auto clip = create_scope<AnimationClip>(id);
        AnimationClip* raw = clip.get();
        raw->setPath(path);
        raw->loadFromJson(path);
        const InstanceID instance_id = raw->getInstanceID();
        s_animation_clip_cache.emplace(instance_id, std::move(clip));
        return raw;
    }

    void AnimationClip::Shutdown() {
        s_animation_clip_cache.clear();
    }

} // dodoe
