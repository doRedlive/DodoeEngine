// do@Redlive

#include "frame_staging_allocator.h"

namespace dodoe {

#ifdef DODOE_PERF_ENABLED
    std::mutex FrameStagingAllocator::s_stats_mutex{};
    std::vector<FrameStagingAllocator*> FrameStagingAllocator::s_instances{};

    FrameStagingAllocator::GlobalStats FrameStagingAllocator::QueryGlobalStats() {
        GlobalStats stats;
        std::lock_guard<std::mutex> lock(s_stats_mutex);
        stats.allocator_count = s_instances.size();
        for (FrameStagingAllocator* allocator : s_instances) {
            stats.total_bytes += allocator->m_ring_size;
            stats.used_bytes += allocator->m_head;
            stats.peak_used_bytes += allocator->m_peak_used_bytes;
            stats.stall_count += allocator->m_stall_count;
            stats.overflow_count += allocator->m_overflow_count;
        }
        return stats;
    }
#endif

    Bool FrameStagingAllocator::initialize(const FrameStagingAllocatorCreateInfo& info) {
        m_device = info.device;
        m_ring_size = info.ring_size_bytes;

        GfxBufferDesc desc;
        desc.byteSize = m_ring_size;
        desc.cpuAccess = GfxCpuAccessMode::Write;
        desc.isConstantBuffer = true;
        desc.debugName = "FrameStagingAllocator";
        desc.enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer);

        m_ring_buffer = create_ref<GfxBuffer>(desc);
        m_ring_buffer->initializeGpu(m_device);
        m_mapped_base = static_cast<UInt8*>(m_device->mapBuffer(m_ring_buffer->getRHI(), GfxCpuAccessMode::Write));

        m_head = 0;
#ifdef DODOE_PERF_ENABLED
        m_peak_used_bytes = 0;
#endif
        m_stall_count = 0;
        m_overflow_count = 0;
        if (!m_mapped_base) {
            return false;
        }
#ifdef DODOE_PERF_ENABLED
        {
            std::lock_guard<std::mutex> lock(s_stats_mutex);
            s_instances.push_back(this);
        }
#endif

        return true;
    }

    void FrameStagingAllocator::shutdown() {
#ifdef DODOE_PERF_ENABLED
        {
            std::lock_guard<std::mutex> lock(s_stats_mutex);
            for (Size_t i = 0; i < s_instances.size(); ++i) {
                if (s_instances[i] == this) {
                    s_instances[i] = s_instances.back();
                    s_instances.pop_back();
                    break;
                }
            }
        }
#endif
        if (m_ring_buffer && m_mapped_base) {
            m_device->unmapBuffer(m_ring_buffer->getRHI());
        }
        m_ring_buffer.reset();
        m_mapped_base = nullptr;
        m_device = nullptr;
        m_ring_size = 0;
        m_head = 0;
#ifdef DODOE_PERF_ENABLED
        m_peak_used_bytes = 0;
#endif
    }

    FrameStagingAllocator::Allocation FrameStagingAllocator::allocate(UInt64 size, UInt64 alignment) {
        UInt64 aligned_offset = (m_head + alignment - 1) & ~(alignment - 1);
        UInt64 aligned_size = (size + alignment - 1) & ~(alignment - 1);

        if (aligned_offset + aligned_size > m_ring_size) {
            if (aligned_size > m_ring_size) {
                m_overflow_count++;
                DO_ERROR("FrameStagingAllocator::allocate: requested size exceeds total ring capacity");
                return {};
            }
            m_stall_count++;
            DO_WARN("FrameStagingAllocator::allocate: ring buffer exhausted for this frame");
            return {};
        }

        Allocation alloc;
        alloc.buffer = m_ring_buffer;
        alloc.offset = aligned_offset;
        alloc.size = aligned_size;
        alloc.mapped_data = m_mapped_base + aligned_offset;

        m_head = aligned_offset + aligned_size;
#ifdef DODOE_PERF_ENABLED
        m_peak_used_bytes = std::max(m_peak_used_bytes, m_head);
#endif

        return alloc;
    }

    void FrameStagingAllocator::reset() {
        m_head = 0;
    }

} // namespace dodoe
