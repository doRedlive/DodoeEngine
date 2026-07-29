// do@Redlive

#pragma once

#include "dopch.h"
#include "allocator.h"

#include <atomic>
#include <new>
#include <vector>

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

	enum class AllocTier : UInt8 {
		Persistent,
		Frame,
		Scratch,
		Count
	};

	enum class AllocTag : UInt8 {
		Object,
		RenderCmd,
		Texture,
		Resource,
		Misc,
		Count
	};

	struct TierStats {
		std::atomic<Size_t> current_bytes{0};
		std::atomic<Size_t> peak_bytes{0};
		std::atomic<UInt64> alloc_count{0};
		std::atomic<UInt64> dealloc_count{0};

		void recordAlloc(Size_t size);
		void recordFree(Size_t size);
	};

	struct ThreadAllocator;

	class DODOE_API Memory {
		static MallocAllocator s_fallback;

		static TierStats s_tier_stats[static_cast<int>(AllocTier::Count)][static_cast<int>(AllocTag::Count)];
		static std::atomic<UInt64> s_frame_epoch;
		static std::vector<ThreadAllocator*> s_thread_allocators;
		static std::mutex s_thread_allocators_mutex;

	public:
		static void Init();
		static void Shutdown();

		static void* Allocate(AllocTier tier, Size_t size, Size_t align,
		                      AllocTag tag = AllocTag::Misc, const char* type_name = nullptr);
		static void  Deallocate(AllocTier tier, void* p, Size_t size, AllocTag tag = AllocTag::Misc);

		static void* AllocatePersistent(Size_t size, Size_t align, AllocTag tag = AllocTag::Object);
		static void  DeallocatePersistent(void* p, Size_t size, AllocTag tag = AllocTag::Object);
		static void* AllocateFrame(Size_t size, Size_t align, AllocTag tag = AllocTag::RenderCmd);
		static void* AllocateScratch(Size_t size, Size_t align);

		static void InitThread();
		static void ShutdownThread();
		static void AdvanceFrameEpoch();
		static UInt64 CurrentFrameEpoch();

		static void RegisterPool(AllocTag tag, Size_t block_size, Size_t block_align);

		static const TierStats& GetStats(AllocTier tier, AllocTag tag = AllocTag::Misc);
		static Size_t FrameUsedBytesTotal();

		static void ResetAllStats();
		static void DumpAll();

		static void RegisterThreadAllocator(ThreadAllocator* ta);
		static void UnregisterThreadAllocator(ThreadAllocator* ta);

		static void* Allocate(Size_t size, Size_t align, AllocCategory cat, const char* typeName = nullptr);
		static void  Deallocate(void* p, Size_t size, AllocCategory cat);

		static void ResetFrame();
	};

	template <typename T>
	inline constexpr AllocCategory categoryFor() {
		return AllocCategory::Misc;
	}

} // namespace dodoe

#define DODOE_NEW(T, cat, ...) \
    ([&]() -> T* { \
        void* memory = dodoe::Memory::Allocate(sizeof(T), alignof(T), cat, #T); \
        if (!memory) throw std::bad_alloc{}; \
        return new (memory) T(__VA_ARGS__); \
    }())

#define DODOE_DELETE(p, T, cat) \
    do { \
        auto* object = (p); \
        if (object) { \
            object->~T(); \
            dodoe::Memory::Deallocate(object, sizeof(T), cat); \
        } \
    } while(0)
