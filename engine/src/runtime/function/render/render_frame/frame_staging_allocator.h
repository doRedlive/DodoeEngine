// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct FrameStagingAllocatorCreateInfo {
        GfxDeviceHandle device{};
        UInt64 ring_size_bytes{64 * 1024 * 1024};
    };

    class FrameStagingAllocator final : public Managed<FrameStagingAllocator, FrameStagingAllocatorCreateInfo> {
        friend class Managed<FrameStagingAllocator, FrameStagingAllocatorCreateInfo>;

    public:
#ifdef DODOE_PERF_ENABLED
        struct GlobalStats {
            Size_t allocator_count{0};
            UInt64 total_bytes{0};
            UInt64 used_bytes{0};
            UInt64 peak_used_bytes{0};
            UInt64 stall_count{0};
            UInt64 overflow_count{0};
        };
#endif

        struct Allocation {
            GfxBufferHandle buffer{};
            UInt64 offset{0};
            UInt64 size{0};
            void* mapped_data{nullptr};
        };

#ifdef DODOE_PERF_ENABLED
        static GlobalStats QueryGlobalStats();
#endif

        Allocation allocate(UInt64 size, UInt64 alignment = 256);
        void reset();

        [[nodiscard]] UInt64 getUsedBytes() const { return m_head; }
        [[nodiscard]] UInt64 getFreeBytes() const { return m_ring_size - m_head; }
        [[nodiscard]] UInt64 getTotalBytes() const { return m_ring_size; }
        [[nodiscard]] UInt32 getStallCount() const { return m_stall_count; }
        [[nodiscard]] UInt32 getOverflowCount() const { return m_overflow_count; }

    private:
        Bool initialize(const FrameStagingAllocatorCreateInfo& info);
        void shutdown();

        GfxDeviceHandle m_device{};
        GfxBufferHandle m_ring_buffer{};
        UInt8* m_mapped_base{nullptr};
        UInt64 m_ring_size{0};
        UInt64 m_head{0};
#ifdef DODOE_PERF_ENABLED
        UInt64 m_peak_used_bytes{0};
#endif

        UInt32 m_stall_count{0};
        UInt32 m_overflow_count{0};

#ifdef DODOE_PERF_ENABLED
        static std::mutex s_stats_mutex;
        static std::vector<FrameStagingAllocator*> s_instances;
#endif
    };

} // namespace dodoe
