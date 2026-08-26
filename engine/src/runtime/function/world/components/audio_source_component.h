// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/audio/audio_types.h"
#include "runtime/function/audio/audio_clip.h"

REFLECTION_TYPE(AudioSourceComponent)

namespace dodoe {

    STRUCT(AudioSourceComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(AudioSourceComponent)

        META(Enable)
        PPtr<AudioClip> clip{};
        META(Enable)
        Float volume{1.0f};
        META(Enable)
        Float pitch{1.0f};
        META(Enable)
        Bool loop{false};
        META(Enable)
        Bool play_on_awake{true};
        META(Enable)
        Float spatial_blend{0.0f};

        AudioPlayState play_state{AudioPlayState::Stopped};
        Bool dirty{true};

        void play() { play_state = AudioPlayState::Playing; dirty = true; }
        void stop() { play_state = AudioPlayState::Stopped; dirty = true; }
        void pause() { play_state = AudioPlayState::Paused; dirty = true; }
        void unPause() {
            if (play_state == AudioPlayState::Paused) {
                play_state = AudioPlayState::Playing;
                dirty = true;
            }
        }

        [[nodiscard]] Bool isPlaying() const { return play_state == AudioPlayState::Playing; }

        void setClip(const PPtr<AudioClip>& in_clip) { clip = in_clip; dirty = true; }
        void setVolume(const Float in_volume) { volume = in_volume; dirty = true; }
        void setPitch(const Float in_pitch) { pitch = in_pitch; dirty = true; }
        void setLoop(const Bool in_loop) { loop = in_loop; dirty = true; }
        void setSpatialBlend(const Float in_blend) { spatial_blend = in_blend; dirty = true; }
    };

} // dodoe
