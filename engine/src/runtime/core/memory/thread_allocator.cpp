// do@Redlive

#include "thread_allocator.h"
#include "memory.h"

namespace dodoe {

	static thread_local ThreadAllocator* t_thread_alloc = nullptr;

	ThreadAllocator& threadAllocator() {
		if (!t_thread_alloc) {
			t_thread_alloc = new ThreadAllocator();
			Memory::RegisterThreadAllocator(t_thread_alloc);
		}
		return *t_thread_alloc;
	}

	ThreadAllocator* threadAllocatorPtr() {
		return t_thread_alloc;
	}

	void threadAllocatorDestroy() {
		if (t_thread_alloc) {
			Memory::UnregisterThreadAllocator(t_thread_alloc);
			delete t_thread_alloc;
			t_thread_alloc = nullptr;
		}
	}

	ScratchArena::ScratchArena()
		: m_alloc(&threadAllocator().scratch)
		, m_mark(m_alloc->usedByteSize()) {
	}

	ScratchArena::~ScratchArena() {
		m_alloc->resetTo(m_mark);
	}

	void* ScratchArena::allocate(Size_t size, Size_t align) {
		return m_alloc->allocate(size, align);
	}

} // namespace dodoe
