// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/function/animation/anim_clip_2d.h"

namespace dodoe {

    class AnimationClipAsset : public Asset {
        DynamicArray<AnimFrame2D> m_frames{};
        DynamicArray<AnimClipEvent> m_events{};
        Bool m_loop{false};
        Float m_frame_ms{100.0f};

    public:
        static constexpr AssetType kStaticType = AssetType::AnimationClip;

        AnimationClipAsset() { m_meta.type = AssetType::AnimationClip; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return false; }
        [[nodiscard]] Bool saveToSource(const String& absolute_path) const override;

        [[nodiscard]] const DynamicArray<AnimFrame2D>& getFrames() const { return m_frames; }
        [[nodiscard]] const DynamicArray<AnimClipEvent>& getEvents() const { return m_events; }
        [[nodiscard]] Bool getLoop() const { return m_loop; }
        [[nodiscard]] Float getFrameMs() const { return m_frame_ms; }
    };

} // dodoe
