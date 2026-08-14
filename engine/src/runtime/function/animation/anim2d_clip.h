// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"
#include "runtime/core/object/pptr.h"

namespace dodoe {

    class Texture2D;

    struct AnimClipEvent {
        Float time{0.0f};
        String function_name{};

        AnimClipEvent() = default;
        AnimClipEvent(const Float in_time, const String& in_function_name)
            : time(in_time), function_name(in_function_name) {}
    };

    struct AnimFrame2D {
        PPtr<Texture2D> texture{};
        Float duration{100.0f};

        AnimFrame2D() = default;
        explicit AnimFrame2D(const PPtr<Texture2D>& in_texture) : texture(in_texture) {}
    };

    class DODOE_API Anim2DClip : public Object {
        DynamicArray<AnimFrame2D> m_frames{};
        DynamicArray<AnimClipEvent> m_events{};
        Bool m_loop{false};
        Float m_frame_ms{100.0f};
        String m_path{};

    public:
        Anim2DClip() = default;
        explicit Anim2DClip(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "Anim2DClip"; }

        void setFrames(const DynamicArray<AnimFrame2D>& frames) { m_frames = frames; }
        void setEvents(const DynamicArray<AnimClipEvent>& events) { m_events = events; }
        void setLoop(const Bool loop) { m_loop = loop; }
        void setFrameMs(const Float frame_ms) { m_frame_ms = frame_ms; }
        void setPath(const String& path) { m_path = path; }

        [[nodiscard]] const DynamicArray<AnimFrame2D>& getFrames() const { return m_frames; }
        [[nodiscard]] const DynamicArray<AnimClipEvent>& getEvents() const { return m_events; }
        [[nodiscard]] Bool getLoop() const { return m_loop; }
        [[nodiscard]] Float getFrameMs() const { return m_frame_ms; }
        [[nodiscard]] const String& getPath() const { return m_path; }

        [[nodiscard]] Float totalDurationMs() const {
            Float total = 0.0f;
            for (const auto& frame : m_frames) {
                total += frame.duration;
            }
            return total;
        }

        Bool loadFromJson(const String& absolute_path);
        [[nodiscard]] Bool saveToJson(const String& absolute_path) const;

        [[nodiscard]] static Anim2DClip* Create(const ObjectID& ref, const String& path);
        static void Shutdown();
    };

} // dodoe
