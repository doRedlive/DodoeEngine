// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/audio/audio_types.h"

namespace dodoe {

    class AudioBackend;
    class AudioClip;

    class AudioSystem : public Managed<AudioSystem, AudioSystemCreateInfo> {
        friend class Managed<AudioSystem, AudioSystemCreateInfo>;

    public:
        void update(Float dt);

        void setMasterVolume(Float volume);
        void setListenerPosition(const Vector3f& position);

        [[nodiscard]] AudioSourceId createSource();
        void destroySource(AudioSourceId id);
        void setSourceClip(AudioSourceId id, AudioClip* clip);
        void playSource(AudioSourceId id);
        void pauseSource(AudioSourceId id);
        void stopSource(AudioSourceId id);
        void setSourceVolume(AudioSourceId id, Float volume);
        void setSourcePitch(AudioSourceId id, Float pitch);
        void setSourceLoop(AudioSourceId id, Bool loop);
        void setSourcePosition(AudioSourceId id, const Vector3f& position);
        void setSourceSpatialBlend(AudioSourceId id, Float blend);
        [[nodiscard]] Bool isSourcePlaying(AudioSourceId id) const;
        void playOneShot(AudioClip* clip, Float volume_scale = 1.0f);

    private:
        Bool initialize(const AudioSystemCreateInfo& create_info);
        void shutdown();

        AudioBackend* m_backend{nullptr};
    };

} // dodoe
