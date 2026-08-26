// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/function/audio/audio_clip.h"

namespace dodoe {

    class AudioClipAsset : public Asset {
    public:
        static constexpr AssetType kStaticType = AssetType::Audio;

        AudioClipAsset() { m_meta.type = AssetType::Audio; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return true; }

        [[nodiscard]] AudioClip* getClip() const { return m_clip.get(); }

    private:
        Scope<AudioClip> m_clip{nullptr};
    };

} // dodoe
