// do@Redlive

#include "audio_play_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/audio/audio_system.h"
#include "runtime/resource/resource_manager.h"

namespace dodoe {

    AudioPlaySystem::~AudioPlaySystem() = default;

    SystemAccess AudioPlaySystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<IDComponent, TransformComponent, AudioSourceComponent, AudioListenerComponent>()
            .build();
    }

    void AudioPlaySystem::update(Registry& reg, float dt) {
        (void)dt;
        syncListener(reg);
        syncSources(reg);
    }

    void AudioPlaySystem::syncListener(Registry& reg) {
        auto* audio = GetAudioSystem();
        if (!audio) {
            return;
        }
        auto view = reg.view<IDComponent, TransformComponent, AudioListenerComponent>();
        for (auto entity : view) {
            auto& transform = entity.getComponent<TransformComponent>();
            audio->setListenerPosition(transform.position);
            break;
        }
    }

    void AudioPlaySystem::syncSources(Registry& reg) {
        auto* audio = GetAudioSystem();
        if (!audio) {
            return;
        }

        auto view = reg.view<IDComponent, TransformComponent, AudioSourceComponent>();
        UnorderedSet<UUID> active{};

        for (auto entity : view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& transform = entity.getComponent<TransformComponent>();
            auto& src = entity.getComponent<AudioSourceComponent>();
            active.insert(id.id);

            auto it = m_sources.find(id.id);
            if (it == m_sources.end()) {
                const AudioSourceId source_id = audio->createSource();
                it = m_sources.emplace(id.id, source_id).first;
                src.dirty = true;
                if (src.play_on_awake && m_awake_played.find(id.id) == m_awake_played.end()) {
                    src.play_state = AudioPlayState::Playing;
                    m_awake_played.insert(id.id);
                }
            }
            const AudioSourceId source_id = it->second;

            if (src.clip.getObjectID().isValid() && !src.clip.get()) {
                AudioClip* clip = ResourceManager::Self().loadObject<AudioClip>(
                    src.clip.getObjectID().asset_id, src.clip.getObjectID().local_id);
                if (clip) {
                    src.clip = PPtr<AudioClip>(clip);
                    src.dirty = true;
                }
            }

            audio->setSourceClip(source_id, src.clip.get());
            audio->setSourceVolume(source_id, src.volume);
            audio->setSourcePitch(source_id, src.pitch);
            audio->setSourceLoop(source_id, src.loop);
            audio->setSourceSpatialBlend(source_id, src.spatial_blend);
            if (src.spatial_blend > 0.0f) {
                audio->setSourcePosition(source_id, transform.position);
            }

            if (src.dirty) {
                switch (src.play_state) {
                    case AudioPlayState::Playing:
                        audio->playSource(source_id);
                        break;
                    case AudioPlayState::Stopped:
                        audio->stopSource(source_id);
                        break;
                    case AudioPlayState::Paused:
                        audio->pauseSource(source_id);
                        break;
                }
                src.dirty = false;
            } else if (src.play_state == AudioPlayState::Playing && !audio->isSourcePlaying(source_id)) {
                src.play_state = AudioPlayState::Stopped;
            }
        }

        pruneRemoved(active);
    }

    void AudioPlaySystem::pruneRemoved(const UnorderedSet<UUID>& active) {
        auto* audio = GetAudioSystem();
        if (!audio) {
            m_sources.clear();
            return;
        }
        for (auto it = m_sources.begin(); it != m_sources.end();) {
            if (active.find(it->first) == active.end()) {
                audio->destroySource(it->second);
                it = m_sources.erase(it);
            } else {
                ++it;
            }
        }
    }

} // dodoe
