// do@Redlive

#include "memory.h"
#include "thread_allocator.h"
#include "runtime/core/log/log_system.h"

namespace dodoe {

	MallocAllocator Memory::s_fallback{};

	TierStats Memory::s_tier_stats[static_cast<int>(AllocTier::Count)][static_cast<int>(AllocTag::Count)]{};
	std::atomic<UInt64> Memory::s_frame_epoch{0};
	std::vector<ThreadAllocator*> Memory::s_thread_allocators{};
	std::mutex Memory::s_thread_allocators_mutex{};
	PoolAllocator* Memory::s_pools[static_cast<int>(AllocTag::Count)]{};
	std::mutex Memory::s_pools_mutex{};

	void TierStats::recordAlloc(Size_t size) {
		alloc_count.fetch_add(1, std::memory_order_relaxed);
		Size_t cur = current_bytes.fetch_add(size, std::memory_order_relaxed) + size;

		Size_t peak = peak_bytes.load(std::memory_order_relaxed);
		while (cur > peak) {
			if (peak_bytes.compare_exchange_weak(peak, cur, std::memory_order_relaxed, std::memory_order_relaxed)) {
				break;
			}
		}
	}

	void TierStats::recordFree(Size_t size) {
		current_bytes.fetch_sub(size, std::memory_order_relaxed);
		dealloc_count.fetch_add(1, std::memory_order_relaxed);
	}

	void Memory::Init() {
	}

	void Memory::Shutdown() {
		std::lock_guard<std::mutex> lock(s_pools_mutex);
		for (auto*& pool : s_pools) {
			delete pool;
			pool = nullptr;
		}
	}

	void Memory::RegisterThreadAllocator(ThreadAllocator* ta) {
		std::lock_guard<std::mutex> lock(s_thread_allocators_mutex);
		s_thread_allocators.push_back(ta);
	}

	void Memory::UnregisterThreadAllocator(ThreadAllocator* ta) {
		std::lock_guard<std::mutex> lock(s_thread_allocators_mutex);
		for (Size_t i = 0; i < s_thread_allocators.size(); ++i) {
			if (s_thread_allocators[i] == ta) {
				s_thread_allocators[i] = s_thread_allocators.back();
				s_thread_allocators.pop_back();
				break;
			}
		}
	}

	void Memory::InitThread() {
		(void)threadAllocator();
	}

	void Memory::ShutdownThread() {
		threadAllocatorDestroy();
	}

	void Memory::AdvanceFrameEpoch() {
		s_frame_epoch.fetch_add(1, std::memory_order_release);
	}

	UInt64 Memory::CurrentFrameEpoch() {
		return s_frame_epoch.load(std::memory_order_acquire);
	}

	void* Memory::Allocate(AllocTier tier, Size_t size, Size_t align, AllocTag tag, const char* type_name) {
		(void)type_name;
		int tag_idx = static_cast<int>(tag);
		void* p = nullptr;

		switch (tier) {
		case AllocTier::Persistent:
			if (PoolAllocator* pool = s_pools[tag_idx]) {
				p = pool->allocate(size, align);
				if (!p) {
					p = s_fallback.allocate(size, align);
				}
			} else {
				p = s_fallback.allocate(size, align);
			}
			break;
		case AllocTier::Frame:
			p = threadAllocator().frame.allocate(size, align);
			break;
		case AllocTier::Scratch:
			p = threadAllocator().scratch.allocate(size, align);
			break;
		default:
			break;
		}

		if (p) {
			int tier_idx = static_cast<int>(tier);
			s_tier_stats[tier_idx][tag_idx].recordAlloc(size);
		}
		return p;
	}

	void Memory::Deallocate(AllocTier tier, void* p, Size_t size, AllocTag tag) {
		if (!p) return;
		int tag_idx = static_cast<int>(tag);

		switch (tier) {
		case AllocTier::Persistent: {
			bool released = false;
			for (auto* pool : s_pools) {
				if (pool && pool->owns(p)) {
					pool->deallocate(p, size);
					released = true;
					break;
				}
			}
			if (!released) {
				s_fallback.deallocate(p, size);
			}
			break;
		}
		case AllocTier::Frame:
		case AllocTier::Scratch:
			break;
		default:
			break;
		}

		int tier_idx = static_cast<int>(tier);
		s_tier_stats[tier_idx][tag_idx].recordFree(size);
	}

	void* Memory::AllocatePersistent(Size_t size, Size_t align, AllocTag tag) {
		return Allocate(AllocTier::Persistent, size, align, tag);
	}

	void Memory::DeallocatePersistent(void* p, Size_t size, AllocTag tag) {
		Deallocate(AllocTier::Persistent, p, size, tag);
	}

	void* Memory::AllocateFrame(Size_t size, Size_t align, AllocTag tag) {
		return Allocate(AllocTier::Frame, size, align, tag);
	}

	void* Memory::AllocateScratch(Size_t size, Size_t align) {
		return Allocate(AllocTier::Scratch, size, align, AllocTag::Misc);
	}

	void Memory::RegisterPool(AllocTag tag, Size_t block_size, Size_t block_align) {
		const int idx = static_cast<int>(tag);
		std::lock_guard<std::mutex> lock(s_pools_mutex);
		if (s_pools[idx]) {
			DO_WARN("Memory::RegisterPool: pool already registered for tag {}", static_cast<int>(tag));
			return;
		}
		s_pools[idx] = new PoolAllocator(block_size, block_align);
	}

	const TierStats& Memory::GetStats(AllocTier tier, AllocTag tag) {
		return s_tier_stats[static_cast<int>(tier)][static_cast<int>(tag)];
	}

	Size_t Memory::FrameUsedBytesTotal() {
		Size_t total = 0;
		std::lock_guard<std::mutex> lock(s_thread_allocators_mutex);
		for (auto* ta : s_thread_allocators) {
			total += ta->frame.usedByteSize();
		}
		return total;
	}

	void Memory::ResetAllStats() {
		for (int t = 0; t < static_cast<int>(AllocTier::Count); ++t) {
			for (int g = 0; g < static_cast<int>(AllocTag::Count); ++g) {
				s_tier_stats[t][g].current_bytes.store(0, std::memory_order_relaxed);
				s_tier_stats[t][g].peak_bytes.store(0, std::memory_order_relaxed);
				s_tier_stats[t][g].alloc_count.store(0, std::memory_order_relaxed);
				s_tier_stats[t][g].dealloc_count.store(0, std::memory_order_relaxed);
			}
		}
	}

	void Memory::DumpAll() {
		const char* tier_names[] = {"Persistent", "Frame", "Scratch"};
		const char* tag_names[] = {"Object", "RenderCmd", "Texture", "Resource", "Misc"};
		for (int t = 0; t < static_cast<int>(AllocTier::Count); ++t) {
			for (int g = 0; g < static_cast<int>(AllocTag::Count); ++g) {
				auto& s = s_tier_stats[t][g];
				Size_t cur = s.current_bytes.load(std::memory_order_relaxed);
				Size_t peak = s.peak_bytes.load(std::memory_order_relaxed);
				Size_t allocs = s.alloc_count.load(std::memory_order_relaxed);
				Size_t frees = s.dealloc_count.load(std::memory_order_relaxed);
				LOG_INFO("[Memory] {}/{}: current={} peak={} allocs={} frees={}",
				         tier_names[t], tag_names[g], cur, peak, allocs, frees);
			}
		}
	}

	void* Memory::Allocate(Size_t size, Size_t align, AllocCategory cat, const char* typeName) {
		(void)typeName;
		AllocTag tag = AllocTag::Misc;
		switch (cat) {
		case AllocCategory::Object:   tag = AllocTag::Object;   break;
		case AllocCategory::Texture:  tag = AllocTag::Texture;  break;
		case AllocCategory::RenderCmd:tag = AllocTag::RenderCmd;break;
		case AllocCategory::Resource: tag = AllocTag::Resource; break;
		case AllocCategory::String:
		case AllocCategory::Container:
		case AllocCategory::Misc:     tag = AllocTag::Misc;     break;
		default: break;
		}
		AllocTier tier = (cat == AllocCategory::RenderCmd) ? AllocTier::Frame : AllocTier::Persistent;
		return Allocate(tier, size, align, tag, typeName);
	}

	void Memory::Deallocate(void* p, Size_t size, AllocCategory cat) {
		if (!p) return;
		AllocTag tag = AllocTag::Misc;
		switch (cat) {
		case AllocCategory::Object:   tag = AllocTag::Object;   break;
		case AllocCategory::Texture:  tag = AllocTag::Texture;  break;
		case AllocCategory::RenderCmd:tag = AllocTag::RenderCmd;break;
		case AllocCategory::Resource: tag = AllocTag::Resource; break;
		case AllocCategory::String:
		case AllocCategory::Container:
		case AllocCategory::Misc:     tag = AllocTag::Misc;     break;
		default: break;
		}
		AllocTier tier = (cat == AllocCategory::RenderCmd) ? AllocTier::Frame : AllocTier::Persistent;
		Deallocate(tier, p, size, tag);
	}

	void Memory::ResetFrame() {
		std::lock_guard<std::mutex> lock(s_thread_allocators_mutex);
		for (auto* ta : s_thread_allocators) {
			ta->frame.reset();
		}
		AdvanceFrameEpoch();
	}

} // namespace dodoe
