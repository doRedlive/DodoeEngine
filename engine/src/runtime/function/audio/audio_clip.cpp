// do@Redlive

#include "runtime/function/audio/audio_clip.h"

#include "miniaudio.h"

namespace dodoe {

    struct AudioClip::Impl {
        ma_audio_buffer buffer{};
        ma_uint64 frame_count{0};
        ma_uint32 sample_rate{0};
        ma_uint32 channels{0};
    };

    AudioClip::AudioClip() = default;

    AudioClip::AudioClip(const ObjectID& id)
        : Object(id) {}

    AudioClip::~AudioClip() {
        unload();
    }

    Bool AudioClip::loadFromFile(const String& absolute_path) {
        unload();

        ma_uint64 frame_count = 0;
        void* pcm_data = nullptr;
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        if (ma_decode_file(absolute_path.c_str(), &config, &frame_count, &pcm_data) != MA_SUCCESS) {
            return false;
        }

        auto* impl = new Impl();
        ma_audio_buffer_config buffer_config =
            ma_audio_buffer_config_init(ma_format_f32, config.channels, frame_count, pcm_data, nullptr);
        if (ma_audio_buffer_init_copy(&buffer_config, &impl->buffer) != MA_SUCCESS) {
            ma_free(pcm_data, nullptr);
            delete impl;
            return false;
        }

        ma_free(pcm_data, nullptr);

        impl->frame_count = frame_count;
        impl->sample_rate = config.sampleRate;
        impl->channels = config.channels;
        m_impl = impl;
        return true;
    }

    Bool AudioClip::loadFromMemory(const void* data, Size_t size) {
        unload();

        ma_uint64 frame_count = 0;
        void* pcm_data = nullptr;
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        if (ma_decode_memory(data, size, &config, &frame_count, &pcm_data) != MA_SUCCESS) {
            return false;
        }

        auto* impl = new Impl();
        ma_audio_buffer_config buffer_config =
            ma_audio_buffer_config_init(ma_format_f32, config.channels, frame_count, pcm_data, nullptr);
        if (ma_audio_buffer_init_copy(&buffer_config, &impl->buffer) != MA_SUCCESS) {
            ma_free(pcm_data, nullptr);
            delete impl;
            return false;
        }

        ma_free(pcm_data, nullptr);

        impl->frame_count = frame_count;
        impl->sample_rate = config.sampleRate;
        impl->channels = config.channels;
        m_impl = impl;
        return true;
    }

    void AudioClip::unload() {
        if (!m_impl) {
            return;
        }
        ma_audio_buffer_uninit(&m_impl->buffer);
        delete m_impl;
        m_impl = nullptr;
    }

    UInt64 AudioClip::getFrameCount() const {
        return m_impl ? m_impl->frame_count : 0;
    }

    UInt32 AudioClip::getSampleRate() const {
        return m_impl ? m_impl->sample_rate : 0;
    }

    UInt32 AudioClip::getChannelCount() const {
        return m_impl ? m_impl->channels : 0;
    }

    Float AudioClip::getDurationSeconds() const {
        if (!m_impl || m_impl->sample_rate == 0) {
            return 0.0f;
        }
        return static_cast<Float>(m_impl->frame_count) / static_cast<Float>(m_impl->sample_rate);
    }

    void* AudioClip::acquireReader() const {
        if (!m_impl) {
            return nullptr;
        }
        auto* reader = static_cast<ma_audio_buffer_ref*>(ma_malloc(sizeof(ma_audio_buffer_ref), nullptr));
        if (!reader) {
            return nullptr;
        }
        if (ma_audio_buffer_ref_init(ma_format_f32, m_impl->channels, m_impl->buffer.ref.pData,
                                     m_impl->buffer.ref.sizeInFrames, reader) != MA_SUCCESS) {
            ma_free(reader, nullptr);
            return nullptr;
        }
        reader->sampleRate = m_impl->sample_rate;
        return reader;
    }

    void AudioClip::releaseReader(void* reader) const {
        if (!reader) {
            return;
        }
        ma_audio_buffer_ref_uninit(static_cast<ma_audio_buffer_ref*>(reader));
        ma_free(reader, nullptr);
    }

} // dodoe
