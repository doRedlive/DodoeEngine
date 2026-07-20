// do@Redlive

#include "upload_ring.h"

namespace dodoe {

    Bool UploadRing::initialize(GfxDeviceHandle device, UInt64 ring_size_bytes) {
        m_device = device;
        m_ring_size = ring_size_bytes;

        GfxBufferDesc desc;
        desc.byteSize = ring_size_bytes;
        desc.cpuAccess = GfxCpuAccessMode::Write;
        desc.debugName = "UploadRing";
        desc.keepInitialState = true;

        m_ring_buffer = create_ref<GfxBuffer>(desc);
        m_ring_buffer->initializeRHI(m_device);
        m_mapped_base = static_cast<UInt8*>(m_device->mapBuffer(m_ring_buffer->getRHI(), CpuAccessMode::Write));

        m_head = 0;
        m_stall_count = 0;
        m_overflow_count = 0;

        return m_mapped_base != nullptr;
    }

    void UploadRing::shutdown() {
        if (m_ring_buffer && m_mapped_base) {
            m_device->unmapBuffer(m_ring_buffer->getRHI());
        }
        m_ring_buffer.reset();
        m_mapped_base = nullptr;
        m_device = nullptr;
        m_ring_size = 0;
        m_head = 0;
    }

    UploadRing::Allocation UploadRing::allocate(UInt64 size, UInt64 alignment) {
        UInt64 aligned_offset = (m_head + alignment - 1) & ~(alignment - 1);

        if (aligned_offset + size > m_ring_size) {
            if (size > m_ring_size) {
                m_overflow_count++;
                DO_ASSERT(false, "UploadRing::allocate: requested size exceeds total ring capacity");
                return {};
            }
            m_stall_count++;
            DO_ASSERT(false, "UploadRing::allocate: ring buffer exhausted for this frame");
            return {};
        }

        Allocation alloc;
        alloc.buffer = m_ring_buffer;
        alloc.offset = aligned_offset;
        alloc.size = size;
        alloc.mapped_data = m_mapped_base + aligned_offset;

        m_head = aligned_offset + size;

        return alloc;
    }

    void UploadRing::reset() {
        m_head = 0;
    }

} // namespace dodoe
