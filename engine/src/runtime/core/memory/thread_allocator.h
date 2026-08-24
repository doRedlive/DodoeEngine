// do@Redlive

#pragma once

#include "runtime/core/base.h"
#include "allocator.h"

#include <atomic>

namespace dodoe {

	struct ThreadAllocator {
		LinearAllocator frame;
		LinearAllocator scratch;
		std::atomic<UInt64> last_reset_epoch{0};

		ThreadAllocator() : frame(64 * 1024), scratch(16 * 1024) {}
	};

	ThreadAllocator& threadAllocator();
	ThreadAllocator* threadAllocatorPtr();
	void threadAllocatorDestroy();

	class ScratchArena {
		LinearAllocator* m_alloc;
		Size_t m_mark;

	public:
		ScratchArena();
		~ScratchArena();
		ScratchArena(const ScratchArena&) = delete;
		ScratchArena& operator=(const ScratchArena&) = delete;

		void* allocate(Size_t size, Size_t align);
	};

} // namespace dodoe
