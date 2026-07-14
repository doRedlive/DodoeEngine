// do@Redlive

#include "memory.h"
#include "runtime/function/log/log_system.h"

namespace dodoe {

    MallocAllocator Memory::s_fallback{};
    LinearAllocator Memory::s_frame_allocator{LinearAllocator::kDefaultBlockSize};
    IAllocator* Memory::s_allocators[static_cast<int>(AllocCategory::Count)]{};
    CategoryStats Memory::s_stats[static_cast<int>(AllocCategory::Count)]{};

    void CategoryStats::recordAlloc(Size_t size) {
        alloc_count.fetch_add(1, std::memory_order_relaxed);
        Size_t cur = current_bytes.fetch_add(size, std::memory_order_relaxed) + size;

        Size_t peak = peak_bytes.load(std::memory_order_relaxed);
        while (cur > peak) {
            if (peak_bytes.compare_exchange_weak(peak, cur, std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }

    void CategoryStats::recordFree(Size_t size) {
        current_bytes.fetch_sub(size, std::memory_order_relaxed);
        dealloc_count.fetch_add(1, std::memory_order_relaxed);
    }

    void* Memory::Allocate(Size_t size, Size_t align, AllocCategory cat, const char* typeName) {
        (void)typeName;
        int idx = static_cast<int>(cat);
        IAllocator* allocator = s_allocators[idx];
        if (!allocator) {
            allocator = (cat == AllocCategory::RenderCmd) ? static_cast<IAllocator*>(&s_frame_allocator) : &s_fallback;
        }
        void* p = allocator->allocate(size, align);
        if (p) {
            s_stats[idx].recordAlloc(size);
        }
        return p;
    }

    void Memory::Deallocate(void* p, Size_t size, AllocCategory cat) {
        if (!p) return;
        int idx = static_cast<int>(cat);
        IAllocator* allocator = s_allocators[idx];
        if (!allocator) {
            allocator = (cat == AllocCategory::RenderCmd) ? static_cast<IAllocator*>(&s_frame_allocator) : &s_fallback;
        }
        allocator->deallocate(p, size);
        s_stats[idx].recordFree(size);
    }

    void Memory::SetAllocator(AllocCategory cat, IAllocator* allocator) {
        int idx = static_cast<int>(cat);
        s_allocators[idx] = allocator;
    }

    IAllocator* Memory::GetAllocator(AllocCategory cat) {
        int idx = static_cast<int>(cat);
        return s_allocators[idx] ? s_allocators[idx] : &s_fallback;
    }

    const CategoryStats& Memory::GetStats(AllocCategory cat) {
        return s_stats[static_cast<int>(cat)];
    }

    void Memory::ResetFrame() {
        s_frame_allocator.reset();
        int idx = static_cast<int>(AllocCategory::RenderCmd);
        s_stats[idx].current_bytes.store(0, std::memory_order_relaxed);
    }

    void Memory::ResetAllStats() {
        for (int i = 0; i < static_cast<int>(AllocCategory::Count); ++i) {
            s_stats[i].current_bytes.store(0, std::memory_order_relaxed);
            s_stats[i].peak_bytes.store(0, std::memory_order_relaxed);
            s_stats[i].alloc_count.store(0, std::memory_order_relaxed);
            s_stats[i].dealloc_count.store(0, std::memory_order_relaxed);
        }
    }

    void Memory::DumpAll() {
        for (int i = 0; i < static_cast<int>(AllocCategory::Count); ++i) {
            auto& s = s_stats[i];
            Size_t cur = s.current_bytes.load(std::memory_order_relaxed);
            Size_t peak = s.peak_bytes.load(std::memory_order_relaxed);
            Size_t allocs = s.alloc_count.load(std::memory_order_relaxed);
            Size_t frees = s.dealloc_count.load(std::memory_order_relaxed);
            LOG_INFO("[Memory] cat={}: current={} peak={} allocs={} frees={}", i, cur, peak, allocs, frees);
        }
    }

    Size_t Memory::FrameUsedBytes() {
        return s_frame_allocator.usedByteSize();
    }

    Size_t Memory::FrameBlockCount() {
        return s_frame_allocator.blockCount();
    }

    Size_t Memory::FrameDefaultBlockSize() {
        return s_frame_allocator.defaultBlockSize();
    }

    void Memory::FrameReserve(Size_t byte_size) {
        s_frame_allocator.reserve(byte_size);
    }

} // namespace dodoe
