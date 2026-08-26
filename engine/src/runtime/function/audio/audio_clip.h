// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"

namespace dodoe {

    class DODOE_API AudioClip : public Object {
    public:
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

    private:
        struct Impl;
        Impl* m_impl{nullptr};
    };

} // dodoe
