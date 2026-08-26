// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/audio/audio_types.h"

namespace dodoe {

    class AudioClip;

    class AudioBackend {
    public:
        virtual ~AudioBackend() = default;

        virtual Bool initialize(const AudioBackendInitInfo& info) = 0;
        virtual void shutdown() = 0;
        virtual void update(Float dt) = 0;

        virtual void setMasterVolume(Float volume) = 0;

        virtual void setListenerPosition(const Vector3f& position) = 0;

        virtual AudioSourceId createSource() = 0;
        virtual void destroySource(AudioSourceId id) = 0;
        virtual void setSourceClip(AudioSourceId id, AudioClip* clip) = 0;
        virtual void playSource(AudioSourceId id) = 0;
        virtual void pauseSource(AudioSourceId id) = 0;
        virtual void stopSource(AudioSourceId id) = 0;
        virtual void setSourceVolume(AudioSourceId id, Float volume) = 0;
        virtual void setSourcePitch(AudioSourceId id, Float pitch) = 0;
        virtual void setSourceLoop(AudioSourceId id, Bool loop) = 0;
        virtual void setSourcePosition(AudioSourceId id, const Vector3f& position) = 0;
        virtual void setSourceSpatialBlend(AudioSourceId id, Float blend) = 0;
        [[nodiscard]] virtual Bool isSourcePlaying(AudioSourceId id) const = 0;
        virtual void playOneShot(AudioClip* clip, Float volume_scale) = 0;
    };

} // dodoe
