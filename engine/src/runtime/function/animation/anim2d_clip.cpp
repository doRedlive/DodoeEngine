// do@Redlive

#include "anim2d_clip.h"

#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/utils/common.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/resource_manager.h"

#include <fstream>
#include <sstream>

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<Anim2DClip>> s_anim2d_clip_cache{};

        void ReadFrame(const Json& frame_json, AnimFrame2D& out_frame) {
            if (frame_json.contains("texture")) {
                Serializer::read(frame_json["texture"], out_frame.texture);
            }
            if (frame_json.contains("duration")) {
                Serializer::read(frame_json["duration"], out_frame.duration);
            }
        }

        void ReadEvent(const Json& event_json, AnimClipEvent& out_event) {
            if (event_json.contains("time")) {
                Serializer::read(event_json["time"], out_event.time);
            }
            if (event_json.contains("function_name")) {
                Serializer::read(event_json["function_name"], out_event.function_name);
            }
        }

    } // namespace

    Bool Anim2DClip::loadFromJson(const String& absolute_path) {
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

        if (json.contains("frames") && json["frames"].is_array()) {
            DynamicArray<AnimFrame2D> frames;
            frames.reserve(json["frames"].size());
            for (const auto& frame_json : json["frames"]) {
                AnimFrame2D frame;
                ReadFrame(frame_json, frame);
                frames.push_back(std::move(frame));
            }
            m_frames = std::move(frames);
        }
        if (json.contains("events") && json["events"].is_array()) {
            DynamicArray<AnimClipEvent> events;
            events.reserve(json["events"].size());
            for (const auto& event_json : json["events"]) {
                AnimClipEvent event;
                ReadEvent(event_json, event);
                events.push_back(std::move(event));
            }
            m_events = std::move(events);
        }
        if (json.contains("loop")) {
            Serializer::read(json["loop"], m_loop);
        }
        if (json.contains("frame_ms")) {
            Serializer::read(json["frame_ms"], m_frame_ms);
        }

        return true;
    }

    Bool Anim2DClip::saveToJson(const String& absolute_path) const {
        std::ofstream file(absolute_path.c_str());
        if (!file.is_open()) {
            return false;
        }

        Json json;
        Json frames = Json::array();
        for (const auto& frame : m_frames) {
            Json frame_json;
            frame_json["texture"] = Serializer::write(frame.texture);
            frame_json["duration"] = Serializer::write(frame.duration);
            frames.push_back(std::move(frame_json));
        }
        json["frames"] = std::move(frames);

        Json events = Json::array();
        for (const auto& event : m_events) {
            Json event_json;
            event_json["time"] = Serializer::write(event.time);
            event_json["function_name"] = Serializer::write(event.function_name);
            events.push_back(std::move(event_json));
        }
        json["events"] = std::move(events);

        json["loop"] = Serializer::write(m_loop);
        json["frame_ms"] = Serializer::write(m_frame_ms);

        file << json.dump(4);
        file.flush();
        return true;
    }

    Anim2DClip* Anim2DClip::Create(const ObjectID& ref, const String& path) {
        if (!ref.isValid() || path.empty()) {
            return nullptr;
        }

        if (const InstanceID existing = Object::FindInstanceID(ref); existing != 0) {
            if (auto* obj = Object::FindObjectFromInstanceID(existing)) {
                return static_cast<Anim2DClip*>(obj);
            }
        }

        String absolute_path = path;
        if (auto* asset_manager = ResourceManager::Self().getAssetManager()) {
            if (FsPath(path.c_str()).is_relative()) {
                absolute_path = String((asset_manager->getAssetDir() / FsPath(path.c_str())).generic_string().c_str());
            }
        }

        auto clip = create_scope<Anim2DClip>(ref);
        Anim2DClip* raw = clip.get();
        raw->setPath(absolute_path);
        raw->loadFromJson(absolute_path);
        const InstanceID instance_id = raw->getInstanceID();
        s_anim2d_clip_cache.emplace(instance_id, std::move(clip));
        return raw;
    }

    void Anim2DClip::Shutdown() {
        s_anim2d_clip_cache.clear();
    }

} // dodoe
