// do@Redlive

#include "runtime/function/audio/audio_system.h"

#include "runtime/function/audio/audio_backend.h"
#include "runtime/function/audio/audio_backend_miniaudio.h"

namespace dodoe {

    Bool AudioSystem::initialize(const AudioSystemCreateInfo& create_info) {
        m_backend = new AudioBackendMiniaudio();
        AudioBackendInitInfo backend_info;
        backend_info.master_volume = create_info.master_volume;
        if (!m_backend->initialize(backend_info)) {
            delete m_backend;
            m_backend = nullptr;
            return false;
        }
        return true;
    }

    void AudioSystem::shutdown() {
        if (!m_backend) {
            return;
        }
        m_backend->shutdown();
        delete m_backend;
        m_backend = nullptr;
    }

    void AudioSystem::update(Float dt) {
        if (m_backend) {
            m_backend->update(dt);
        }
    }

    void AudioSystem::setMasterVolume(Float volume) {
        if (m_backend) {
            m_backend->setMasterVolume(volume);
        }
    }

    void AudioSystem::setListenerPosition(const Vector3f& position) {
        if (m_backend) {
            m_backend->setListenerPosition(position);
        }
    }

    AudioSourceId AudioSystem::createSource() {
        return m_backend ? m_backend->createSource() : kInvalidAudioSourceId;
    }

    void AudioSystem::destroySource(AudioSourceId id) {
        if (m_backend) {
            m_backend->destroySource(id);
        }
    }

    void AudioSystem::setSourceClip(AudioSourceId id, AudioClip* clip) {
        if (m_backend) {
            m_backend->setSourceClip(id, clip);
        }
    }

    void AudioSystem::playSource(AudioSourceId id) {
        if (m_backend) {
            m_backend->playSource(id);
        }
    }

    void AudioSystem::pauseSource(AudioSourceId id) {
        if (m_backend) {
            m_backend->pauseSource(id);
        }
    }

    void AudioSystem::stopSource(AudioSourceId id) {
        if (m_backend) {
            m_backend->stopSource(id);
        }
    }

    void AudioSystem::setSourceVolume(AudioSourceId id, Float volume) {
        if (m_backend) {
            m_backend->setSourceVolume(id, volume);
        }
    }

    void AudioSystem::setSourcePitch(AudioSourceId id, Float pitch) {
        if (m_backend) {
            m_backend->setSourcePitch(id, pitch);
        }
    }

    void AudioSystem::setSourceLoop(AudioSourceId id, Bool loop) {
        if (m_backend) {
            m_backend->setSourceLoop(id, loop);
        }
    }

    void AudioSystem::setSourcePosition(AudioSourceId id, const Vector3f& position) {
        if (m_backend) {
            m_backend->setSourcePosition(id, position);
        }
    }

    void AudioSystem::setSourceSpatialBlend(AudioSourceId id, Float blend) {
        if (m_backend) {
            m_backend->setSourceSpatialBlend(id, blend);
        }
    }

    Bool AudioSystem::isSourcePlaying(AudioSourceId id) const {
        return m_backend ? m_backend->isSourcePlaying(id) : false;
    }

    void AudioSystem::playOneShot(AudioClip* clip, Float volume_scale) {
        if (m_backend) {
            m_backend->playOneShot(clip, volume_scale);
        }
    }

} // dodoe
