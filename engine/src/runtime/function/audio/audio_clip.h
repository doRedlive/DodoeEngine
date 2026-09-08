// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"

namespace dodoe {

    class DODOE_API AudioClip : public Object {
    public:
#ifdef DODOE_PERF_ENABLED
        struct MemoryStats {
            Size_t clip_count{0};
            UInt64 pcm_bytes{0};
            UInt64 peak_pcm_bytes{0};
            Size_t reader_count{0};
            UInt64 reader_bytes{0};
            UInt64 peak_reader_bytes{0};
        };
#endif

        AudioClip();
        explicit AudioClip(const ObjectID& id);
        ~AudioClip() override;
        AudioClip(const AudioClip&) = delete;
        AudioClip& operator=(const AudioClip&) = delete;

        [[nodiscard]] const char* getObjectTypeName() const override { return "AudioClip"; }

        Bool loadFromFile(const String& absolute_path);
        Bool loadFromMemory(const void* data, Size_t size);
        void unload();

        [[nodiscard]] Bool isValid() const { return m_impl != nullptr; }
        [[nodiscard]] UInt64 getFrameCount() const;
        [[nodiscard]] UInt32 getSampleRate() const;
        [[nodiscard]] UInt32 getChannelCount() const;
        [[nodiscard]] Float getDurationSeconds() const;

        void* acquireReader() const;
        void releaseReader(void* reader) const;

#ifdef DODOE_PERF_ENABLED
        static MemoryStats QueryMemoryStats();
#endif

    private:
        struct Impl;
        Impl* m_impl{nullptr};
    };

} // dodoe
