// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/resource/asset/asset.h"
#include "runtime/function/animation/animation.h"

REFLECTION_TYPE(Anim2DClipAsset)

namespace dodoe {

    CLASS(Anim2DClipAsset, WhiteListFields) : public Asset {
        REFLECTION_BODY(Anim2DClipAsset)

        DynamicArray<AnimFrame2D> m_frames{};
        DynamicArray<AnimClipEvent> m_events{};
        META(Enable)
        Bool m_loop{false};
        META(Enable)
        Float m_frame_ms{100.0f};

    public:
        static constexpr AssetType kStaticType = AssetType::Anim2DClip;

        Anim2DClipAsset() { m_meta.type = AssetType::Anim2DClip; }

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
