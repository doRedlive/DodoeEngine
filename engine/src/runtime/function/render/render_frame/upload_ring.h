// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class UploadRing {
    public:
        struct Allocation {
            GfxBufferHandle buffer{};
            UInt64 offset{0};
            UInt64 size{0};
            void* mapped_data{nullptr};
        };

        Bool initialize(GfxDeviceHandle device, UInt64 ring_size_bytes = 64 * 1024 * 1024);
        void shutdown();

        Allocation allocate(UInt64 size, UInt64 alignment = 256);
        void reset();

        [[nodiscard]] UInt64 getUsedBytes() const { return m_head; }
        [[nodiscard]] UInt64 getFreeBytes() const { return m_ring_size - m_head; }
        [[nodiscard]] UInt64 getTotalBytes() const { return m_ring_size; }
        [[nodiscard]] UInt32 getStallCount() const { return m_stall_count; }
        [[nodiscard]] UInt32 getOverflowCount() const { return m_overflow_count; }

    private:
        GfxDeviceHandle m_device{};
        GfxBufferHandle m_ring_buffer{};
        UInt8* m_mapped_base{nullptr};
        UInt64 m_ring_size{0};
        UInt64 m_head{0};

        UInt32 m_stall_count{0};
        UInt32 m_overflow_count{0};
    };

} // namespace dodoe
