// do@Redlive

#pragma once

#include "dopch.h"

#include "miniaudio.h"

#include "runtime/core/container/containers.h"
#include "runtime/function/audio/audio_backend.h"

namespace dodoe {

    class AudioBackendMiniaudio : public AudioBackend {
    public:
        ~AudioBackendMiniaudio() override;

        Bool initialize(const AudioBackendInitInfo& info) override;
        void shutdown() override;
        void update(Float dt) override;

        void setMasterVolume(Float volume) override;

        void setListenerPosition(const Vector3f& position) override;

        AudioSourceId createSource() override;
        void destroySource(AudioSourceId id) override;
        void setSourceClip(AudioSourceId id, AudioClip* clip) override;
        void playSource(AudioSourceId id) override;
        void pauseSource(AudioSourceId id) override;
        void stopSource(AudioSourceId id) override;
        void setSourceVolume(AudioSourceId id, Float volume) override;
        void setSourcePitch(AudioSourceId id, Float pitch) override;
        void setSourceLoop(AudioSourceId id, Bool loop) override;
        void setSourcePosition(AudioSourceId id, const Vector3f& position) override;
        void setSourceSpatialBlend(AudioSourceId id, Float blend) override;
        [[nodiscard]] Bool isSourcePlaying(AudioSourceId id) const override;
        void playOneShot(AudioClip* clip, Float volume_scale) override;

    private:
        struct SoundEntry {
            Bool initialized{false};
            ma_sound sound{};
            void* reader{nullptr};
            AudioClip* clip{nullptr};
            Float volume{1.0f};
            Float pitch{1.0f};
            Bool loop{false};
            Float spatial_blend{0.0f};
            Vector3f position{0.0f};
        };

        [[nodiscard]] SoundEntry* findEntry(AudioSourceId id);
        [[nodiscard]] const SoundEntry* findEntry(AudioSourceId id) const;
        void teardownSound(SoundEntry& entry);
        Bool rebuildSound(SoundEntry& entry);
        void applyParams(SoundEntry& entry);
        AudioSourceId allocateId();

        ma_engine m_engine{};
        Bool m_engine_initialized{false};
        Float m_master_volume{1.0f};
        UnorderedMap<AudioSourceId, SoundEntry> m_sources{};
        UnorderedMap<AudioSourceId, SoundEntry> m_one_shots{};
        AudioSourceId m_next_source_id{1};
    };

} // dodoe
