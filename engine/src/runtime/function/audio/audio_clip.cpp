// do@Redlive

#include "runtime/function/audio/audio_clip.h"

#include "miniaudio.h"

namespace dodoe {

#ifdef DODOE_PERF_ENABLED
    namespace {
        std::atomic<Size_t> s_audio_clip_count{0};
        std::atomic<UInt64> s_audio_pcm_bytes{0};
        std::atomic<UInt64> s_audio_pcm_peak{0};
        std::atomic<Size_t> s_audio_reader_count{0};
        std::atomic<UInt64> s_audio_reader_bytes{0};
        std::atomic<UInt64> s_audio_reader_peak{0};

        void RecordPeak(std::atomic<UInt64>& peak, const UInt64 current) {
            UInt64 expected = peak.load(std::memory_order_relaxed);
            while (current > expected &&
                   !peak.compare_exchange_weak(expected, current, std::memory_order_relaxed, std::memory_order_relaxed)) {
            }
        }

        UInt64 ComputePcmBytes(const ma_uint64 frame_count, const ma_uint32 channels) {
            return static_cast<UInt64>(frame_count) * static_cast<UInt64>(channels) * sizeof(float);
        }
    }
#endif

    struct AudioClip::Impl {
        ma_audio_buffer buffer{};
        ma_uint64 frame_count{0};
        ma_uint32 sample_rate{0};
        ma_uint32 channels{0};
#ifdef DODOE_PERF_ENABLED
        UInt64 pcm_bytes{0};
#endif
    };

#ifdef DODOE_PERF_ENABLED
    AudioClip::MemoryStats AudioClip::QueryMemoryStats() {
        MemoryStats stats;
        stats.clip_count = s_audio_clip_count.load(std::memory_order_relaxed);
        stats.pcm_bytes = s_audio_pcm_bytes.load(std::memory_order_relaxed);
        stats.peak_pcm_bytes = s_audio_pcm_peak.load(std::memory_order_relaxed);
        stats.reader_count = s_audio_reader_count.load(std::memory_order_relaxed);
        stats.reader_bytes = s_audio_reader_bytes.load(std::memory_order_relaxed);
        stats.peak_reader_bytes = s_audio_reader_peak.load(std::memory_order_relaxed);
        return stats;
    }
#endif

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

        auto* impl = DODOE_NEW(Impl, AllocCategory::Object);
        ma_audio_buffer_config buffer_config =
            ma_audio_buffer_config_init(ma_format_f32, config.channels, frame_count, pcm_data, nullptr);
        if (ma_audio_buffer_init_copy(&buffer_config, &impl->buffer) != MA_SUCCESS) {
            ma_free(pcm_data, nullptr);
            DODOE_DELETE(impl, Impl, AllocCategory::Object);
            return false;
        }

        ma_free(pcm_data, nullptr);

        impl->frame_count = frame_count;
        impl->sample_rate = config.sampleRate;
        impl->channels = config.channels;
#ifdef DODOE_PERF_ENABLED
        impl->pcm_bytes = ComputePcmBytes(frame_count, config.channels);
#endif
        m_impl = impl;
#ifdef DODOE_PERF_ENABLED
        s_audio_clip_count.fetch_add(1, std::memory_order_relaxed);
        const UInt64 pcm_bytes_now = s_audio_pcm_bytes.fetch_add(impl->pcm_bytes, std::memory_order_relaxed) + impl->pcm_bytes;
        RecordPeak(s_audio_pcm_peak, pcm_bytes_now);
#endif
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

        auto* impl = DODOE_NEW(Impl, AllocCategory::Object);
        ma_audio_buffer_config buffer_config =
            ma_audio_buffer_config_init(ma_format_f32, config.channels, frame_count, pcm_data, nullptr);
        if (ma_audio_buffer_init_copy(&buffer_config, &impl->buffer) != MA_SUCCESS) {
            ma_free(pcm_data, nullptr);
            DODOE_DELETE(impl, Impl, AllocCategory::Object);
            return false;
        }

        ma_free(pcm_data, nullptr);

        impl->frame_count = frame_count;
        impl->sample_rate = config.sampleRate;
        impl->channels = config.channels;
#ifdef DODOE_PERF_ENABLED
        impl->pcm_bytes = ComputePcmBytes(frame_count, config.channels);
#endif
        m_impl = impl;
#ifdef DODOE_PERF_ENABLED
        s_audio_clip_count.fetch_add(1, std::memory_order_relaxed);
        const UInt64 pcm_bytes_now = s_audio_pcm_bytes.fetch_add(impl->pcm_bytes, std::memory_order_relaxed) + impl->pcm_bytes;
        RecordPeak(s_audio_pcm_peak, pcm_bytes_now);
#endif
        return true;
    }

    void AudioClip::unload() {
        if (!m_impl) {
            return;
        }
        ma_audio_buffer_uninit(&m_impl->buffer);
#ifdef DODOE_PERF_ENABLED
        s_audio_clip_count.fetch_sub(1, std::memory_order_relaxed);
        s_audio_pcm_bytes.fetch_sub(m_impl->pcm_bytes, std::memory_order_relaxed);
#endif
        DODOE_DELETE(m_impl, Impl, AllocCategory::Object);
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
#ifdef DODOE_PERF_ENABLED
        s_audio_reader_count.fetch_add(1, std::memory_order_relaxed);
        const UInt64 reader_bytes_now = s_audio_reader_bytes.fetch_add(sizeof(ma_audio_buffer_ref), std::memory_order_relaxed)
            + sizeof(ma_audio_buffer_ref);
        RecordPeak(s_audio_reader_peak, reader_bytes_now);
#endif
        return reader;
    }

    void AudioClip::releaseReader(void* reader) const {
        if (!reader) {
            return;
        }
        ma_audio_buffer_ref_uninit(static_cast<ma_audio_buffer_ref*>(reader));
#ifdef DODOE_PERF_ENABLED
        s_audio_reader_count.fetch_sub(1, std::memory_order_relaxed);
        s_audio_reader_bytes.fetch_sub(sizeof(ma_audio_buffer_ref), std::memory_order_relaxed);
#endif
        ma_free(reader, nullptr);
    }

} // dodoe
