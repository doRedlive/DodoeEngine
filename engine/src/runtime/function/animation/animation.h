//
// Created by Redlive on 2026/3/23.
//

#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"

namespace dodoe {

    struct AnimClipEvent {
        Float time{0.0f};
        String function_name{};

        AnimClipEvent() = default;
        AnimClipEvent(const Float in_time, const String& in_function_name)
            : time(in_time), function_name(in_function_name) {}
    };

    class Animation {

    };

    class DODOE_API AnimationClip : public Object {
        DynamicArray<AnimFrame2D> m_frames{};
        Bool m_loop{false};
        Float m_frame_ms{100.0f};
        String m_path{};

    public:
        AnimationClip() = default;
        explicit AnimationClip(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "AnimationClip"; }

        void setFrames(const DynamicArray<AnimFrame2D>& frames) { m_frames = frames; }
        void setLoop(const Bool loop) { m_loop = loop; }
        void setFrameMs(const Float frame_ms) { m_frame_ms = frame_ms; }
        void setPath(const String& path) { m_path = path; }

        [[nodiscard]] const DynamicArray<AnimFrame2D>& getFrames() const { return m_frames; }
        [[nodiscard]] Bool getLoop() const { return m_loop; }
        [[nodiscard]] Float getFrameMs() const { return m_frame_ms; }
        [[nodiscard]] const String& getPath() const { return m_path; }

        [[nodiscard]] Bool loadFromJson(const String& absolute_path);
        [[nodiscard]] Bool saveToJson(const String& absolute_path) const;

        [[nodiscard]] static AnimationClip* Load(const String& path);
        static void Shutdown();
    };

} // dodoe
