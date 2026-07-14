// do@Redlive

#pragma once

#include "dopch.h"
#include "allocator.h"

#include <atomic>

namespace dodoe {

    enum class AllocCategory : UInt8 {
        Object,
        Texture,
        RenderCmd,
        Resource,
        String,
        Container,
        Misc,
        Count
    };

    struct CategoryStats {
        std::atomic<Size_t> current_bytes{0};
        std::atomic<Size_t> peak_bytes{0};
        std::atomic<UInt64> alloc_count{0};
        std::atomic<UInt64> dealloc_count{0};

        void recordAlloc(Size_t size);
        void recordFree(Size_t size);
    };

    class Memory {
        static MallocAllocator s_fallback;
        static LinearAllocator s_frame_allocator;
        static IAllocator* s_allocators[static_cast<int>(AllocCategory::Count)];
        static CategoryStats s_stats[static_cast<int>(AllocCategory::Count)];

    public:
        static void* Allocate(Size_t size, Size_t align, AllocCategory cat, const char* typeName = nullptr);
        static void  Deallocate(void* p, Size_t size, AllocCategory cat);

        static void SetAllocator(AllocCategory cat, IAllocator* allocator);
        static IAllocator* GetAllocator(AllocCategory cat);

        static const CategoryStats& GetStats(AllocCategory cat);
        static void ResetFrame();
        static void ResetAllStats();
        static void DumpAll();

        static Size_t FrameUsedBytes();
        static Size_t FrameBlockCount();
        static Size_t FrameDefaultBlockSize();
        static void FrameReserve(Size_t byte_size);
    };

    template <typename T>
    inline constexpr AllocCategory categoryFor() {
        return AllocCategory::Misc;
    }

} // namespace dodoe

#define DODOE_NEW(T, cat, ...) \
    (new (dodoe::Memory::Allocate(sizeof(T), alignof(T), cat, #T)) T(__VA_ARGS__))

#define DODOE_DELETE(p, T, cat) \
    do { if (p) { (p)->~T(); dodoe::Memory::Deallocate(p, sizeof(T), cat); } } while(0)
