// do@Redlive

#include "runtime/function/audio/audio_backend_miniaudio.h"

#include "runtime/function/audio/audio_clip.h"

namespace dodoe {

    namespace {

        ma_positioning ToMiniaudioPositioning(Float blend) {
            return blend > 0.0f ? ma_positioning_absolute : ma_positioning_relative;
        }

    }

    AudioBackendMiniaudio::~AudioBackendMiniaudio() {
        if (m_engine_initialized) {
            shutdown();
        }
    }

    Bool AudioBackendMiniaudio::initialize(const AudioBackendInitInfo& info) {
        if (m_engine_initialized) {
            return true;
        }

        ma_engine_config config = ma_engine_config_init();
        if (ma_engine_init(&config, &m_engine) != MA_SUCCESS) {
            return false;
        }

        m_engine_initialized = true;
        m_master_volume = info.master_volume;
        ma_engine_set_volume(&m_engine, m_master_volume);
        return true;
    }

    void AudioBackendMiniaudio::shutdown() {
        if (!m_engine_initialized) {
            return;
        }

        for (auto& [id, entry] : m_sources) {
            (void)id;
            teardownSound(entry);
        }
        m_sources.clear();

        for (auto& [id, entry] : m_one_shots) {
            (void)id;
            teardownSound(entry);
        }
        m_one_shots.clear();

        ma_engine_uninit(&m_engine);
        m_engine_initialized = false;
    }

    void AudioBackendMiniaudio::update(Float dt) {
        (void)dt;
        if (!m_engine_initialized) {
            return;
        }

        for (auto it = m_one_shots.begin(); it != m_one_shots.end();) {
            SoundEntry& entry = it->second;
            if (entry.initialized && ma_sound_at_end(&entry.sound)) {
                teardownSound(entry);
                it = m_one_shots.erase(it);
            } else {
                ++it;
            }
        }
    }

    void AudioBackendMiniaudio::setMasterVolume(Float volume) {
        m_master_volume = volume;
        if (m_engine_initialized) {
            ma_engine_set_volume(&m_engine, m_master_volume);
        }
    }

    void AudioBackendMiniaudio::setListenerPosition(const Vector3f& position) {
        if (m_engine_initialized) {
            ma_engine_listener_set_position(&m_engine, 0, position.x, position.y, position.z);
        }
    }

    AudioSourceId AudioBackendMiniaudio::createSource() {
        const AudioSourceId id = allocateId();
        m_sources.emplace(id, SoundEntry{});
        return id;
    }

    void AudioBackendMiniaudio::destroySource(AudioSourceId id) {
        auto it = m_sources.find(id);
        if (it == m_sources.end()) {
            return;
        }
        teardownSound(it->second);
        m_sources.erase(it);
    }

    void AudioBackendMiniaudio::setSourceClip(AudioSourceId id, AudioClip* clip) {
        SoundEntry* entry = findEntry(id);
        if (!entry) {
            return;
        }
        if (entry->clip == clip && entry->initialized) {
            return;
        }
        entry->clip = clip;
        if (clip && clip->isValid()) {
            rebuildSound(*entry);
        } else {
            teardownSound(*entry);
        }
    }

    void AudioBackendMiniaudio::playSource(AudioSourceId id) {
        SoundEntry* entry = findEntry(id);
        if (!entry) {
            return;
        }
        if (!entry->initialized && !rebuildSound(*entry)) {
            return;
        }
        ma_sound_start(&entry->sound);
    }

    void AudioBackendMiniaudio::pauseSource(AudioSourceId id) {
        SoundEntry* entry = findEntry(id);
        if (!entry || !entry->initialized) {
            return;
        }
        ma_sound_stop(&entry->sound);
    }

    void AudioBackendMiniaudio::stopSource(AudioSourceId id) {
        SoundEntry* entry = findEntry(id);
        if (!entry || !entry->initialized) {
            return;
        }
        ma_sound_stop(&entry->sound);
        ma_sound_seek_to_pcm_frame(&entry->sound, 0);
    }

    void AudioBackendMiniaudio::setSourceVolume(AudioSourceId id, Float volume) {
        SoundEntry* entry = findEntry(id);
        if (!entry) {
            return;
        }
        entry->volume = volume;
        if (entry->initialized) {
            ma_sound_set_volume(&entry->sound, volume);
        }
    }

    void AudioBackendMiniaudio::setSourcePitch(AudioSourceId id, Float pitch) {
        SoundEntry* entry = findEntry(id);
        if (!entry) {
            return;
        }
        entry->pitch = pitch;
        if (entry->initialized) {
            ma_sound_set_pitch(&entry->sound, pitch);
        }
    }

    void AudioBackendMiniaudio::setSourceLoop(AudioSourceId id, Bool loop) {
        SoundEntry* entry = findEntry(id);
        if (!entry) {
            return;
        }
        entry->loop = loop;
        if (entry->initialized) {
            ma_sound_set_looping(&entry->sound, loop ? MA_TRUE : MA_FALSE);
        }
    }

    void AudioBackendMiniaudio::setSourcePosition(AudioSourceId id, const Vector3f& position) {
        SoundEntry* entry = findEntry(id);
        if (!entry) {
            return;
        }
        entry->position = position;
        if (entry->initialized) {
            ma_sound_set_position(&entry->sound, position.x, position.y, position.z);
        }
    }

    void AudioBackendMiniaudio::setSourceSpatialBlend(AudioSourceId id, Float blend) {
        SoundEntry* entry = findEntry(id);
        if (!entry) {
            return;
        }
        entry->spatial_blend = blend;
        if (entry->initialized) {
            ma_sound_set_positioning(&entry->sound, ToMiniaudioPositioning(blend));
        }
    }

    Bool AudioBackendMiniaudio::isSourcePlaying(AudioSourceId id) const {
        const SoundEntry* entry = findEntry(id);
        if (!entry || !entry->initialized) {
            return false;
        }
        return ma_sound_is_playing(&entry->sound) == MA_TRUE;
    }

    void AudioBackendMiniaudio::playOneShot(AudioClip* clip, Float volume_scale) {
        if (!m_engine_initialized || !clip || !clip->isValid()) {
            return;
        }

        const AudioSourceId id = allocateId();
        SoundEntry entry;
        entry.clip = clip;
        entry.volume = volume_scale;
        entry.pitch = 1.0f;
        entry.loop = false;
        if (!rebuildSound(entry)) {
            return;
        }
        ma_sound_start(&entry.sound);
        m_one_shots.emplace(id, std::move(entry));
    }

    AudioBackendMiniaudio::SoundEntry* AudioBackendMiniaudio::findEntry(AudioSourceId id) {
        auto it = m_sources.find(id);
        return it != m_sources.end() ? &it->second : nullptr;
    }

    const AudioBackendMiniaudio::SoundEntry* AudioBackendMiniaudio::findEntry(AudioSourceId id) const {
        auto it = m_sources.find(id);
        return it != m_sources.end() ? &it->second : nullptr;
    }

    void AudioBackendMiniaudio::teardownSound(SoundEntry& entry) {
        if (entry.initialized) {
            ma_sound_uninit(&entry.sound);
            entry.initialized = false;
        }
        if (entry.reader) {
            if (entry.clip) {
                entry.clip->releaseReader(entry.reader);
            }
            entry.reader = nullptr;
        }
    }

    Bool AudioBackendMiniaudio::rebuildSound(SoundEntry& entry) {
        if (!m_engine_initialized || !entry.clip || !entry.clip->isValid()) {
            return false;
        }

        teardownSound(entry);

        void* reader = entry.clip->acquireReader();
        if (!reader) {
            return false;
        }
        if (ma_sound_init_from_data_source(&m_engine, static_cast<ma_data_source*>(reader), 0, nullptr,
                                           &entry.sound) != MA_SUCCESS) {
            entry.clip->releaseReader(reader);
            return false;
        }

        entry.initialized = true;
        entry.reader = reader;
        applyParams(entry);
        return true;
    }

    void AudioBackendMiniaudio::applyParams(SoundEntry& entry) {
        ma_sound_set_volume(&entry.sound, entry.volume);
        ma_sound_set_pitch(&entry.sound, entry.pitch);
        ma_sound_set_looping(&entry.sound, entry.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_position(&entry.sound, entry.position.x, entry.position.y, entry.position.z);
        ma_sound_set_positioning(&entry.sound, ToMiniaudioPositioning(entry.spatial_blend));
    }

    AudioSourceId AudioBackendMiniaudio::allocateId() {
        const AudioSourceId id = m_next_source_id++;
        if (m_next_source_id == 0) {
            m_next_source_id = 1;
        }
        return id;
    }

} // dodoe
